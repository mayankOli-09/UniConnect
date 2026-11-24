// Stage 6b check: groups and messages against the real MySQL database and its seeded conversations.
#include <iostream>
#include <string>

#include "database/MySqlChatRepository.h"
#include "database/MySqlUserRepository.h"
#include "services/ChatService.h"

using namespace uniconnect;
using database::rowInt;
using database::rowStr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

static database::MySqlDatabase* g_db = nullptr;
static int count(const std::string& sql, const std::vector<database::DbValue>& p = {}) {
    const auto r = g_db->execute(sql, p);
    return r.ok && !r.rows.empty() ? rowInt(r.rows[0], "n") : -1;
}
static int rank(const std::string& type) {
    const char* order[] = {"DEPARTMENT", "SECTION", "COURSE", "GROUP", "PRIVATE"};
    for (int i = 0; i < 5; ++i) if (type == order[i]) return i;
    return 9;
}

int main() {
    database::DbConfig cfg = database::DbConfig::fromEnvironment();
    if (cfg.password.empty()) cfg.password = "UniConnect#Dev2026";   // documented local development password
    database::MySqlDatabase db(cfg);
    g_db = &db;
    std::string err;
    if (!db.connect(err)) { std::cout << "CANNOT CONNECT: " << err << "\n"; return 1; }

    database::MySqlUserRepository users(db);
    database::MySqlChatRepository repo(db);
    services::ChatService chat(repo);
    const auto rahul = *users.findByEmail("rahul@uniconnect.com");
    const auto admin = *users.findByEmail("admin@uniconnect.com");

    const int msgBefore = count("SELECT COUNT(*) AS n FROM messages");
    const int maxMsgBefore = count("SELECT MAX(message_id) AS n FROM messages");
    const int convBefore = count("SELECT COUNT(*) AS n FROM conversations");

    std::cout << "1. Rahul's groups\n";
    auto r = chat.groups(rahul, "");
    const int expected = count("SELECT COUNT(*) AS n FROM conversation_members WHERE user_id = ?", {std::to_string(rahul.id)});
    CHECK(r.status == 200 && static_cast<int>(r.body["groups"].size()) == expected && expected >= 3);
    bool ordered = true;
    for (size_t i = 1; i < r.body["groups"].size(); ++i)
        if (rank(r.body["groups"].at(i - 1)["type"].asString()) > rank(r.body["groups"].at(i)["type"].asString())) ordered = false;
    CHECK(ordered);                                                                           // department, section, then course groups
    bool hasSection = false;
    for (size_t i = 0; i < r.body["groups"].size(); ++i) {
        const auto& g = r.body["groups"].at(i);
        std::cout << "   " << g["type"].asString() << ": " << g["name"].asString() << " (" << g["member_count"].asInt() << " members)\n";
        if (g["name"].asString() == "CSE Section A" && g["type"].asString() == "SECTION") hasSection = true;
    }
    CHECK(hasSection);
    CHECK(chat.groups(rahul, "2").status == 403);                                             // Priya's groups are not his to see
    CHECK(chat.groups(admin, "1").status == 200);

    std::cout << "2. messages of the DBMS course group, with the real seeded rows\n";
    r = chat.messages(rahul, "1", "", "");
    CHECK(r.status == 200 && r.body["messages"].size() == 3 && !r.body["has_more"].asBool());
    const auto& first = r.body["messages"].at(0);
    CHECK(first["content"].asString() == "DBMS assignment submission is due on Friday." && first["sender_name"].asString() == "Dr. Rajesh Kumar");
    CHECK(r.body["messages"].at(1)["sender_name"].asString() == "Rahul Sharma");
    const std::string when = first["sent_at"].asString();
    CHECK(when.size() == 20 && when[10] == 'T' && when.back() == 'Z');                         // ISO 8601, UTC
    std::cout << "   first message: \"" << first["content"].asString() << "\" at " << when << "\n";
    r = chat.messages(rahul, "1", "2", "");
    CHECK(r.body["messages"].size() == 2 && r.body["has_more"].asBool() && r.body["messages"].at(0)["message_id"].asInt() == 2);   // the newest two, oldest first
    r = chat.messages(rahul, "1", "10", "2");
    CHECK(r.body["messages"].size() == 1 && r.body["messages"].at(0)["message_id"].asInt() == 1);

    std::cout << "3. sending, including awkward text\n";
    std::vector<int> heard;
    chat.setMessageListener([&](const models::ChatMessage&, const std::vector<int>& to) { heard = to; });
    const std::string tricky = "Hello \xF0\x9F\x8E\x93 it's 'quoted'; DROP TABLE messages;-- \\ end";
    auto body = utils::JsonValue::object();
    body["content"] = tricky;
    body["priority"] = 5;
    r = chat.send(rahul, "1", body);
    CHECK(r.status == 200 && r.body["success"].asBool());
    const int newId = r.body["message"]["message_id"].asInt();
    CHECK(newId > maxMsgBefore && r.body["message"]["sender_name"].asString() == "Rahul Sharma" && r.body["message"]["content"].asString() == tricky);
    CHECK(count("SELECT COUNT(*) AS n FROM messages") == msgBefore + 1);
    auto latest = chat.messages(rahul, "1", "1", "");
    CHECK(latest.body["messages"].at(0)["content"].asString() == tricky);                      // stored as plain text, table intact
    CHECK(heard.size() == static_cast<size_t>(count("SELECT COUNT(*) AS n FROM conversation_members WHERE conversation_id = 1")));
    CHECK(count("SELECT ABS(TIMESTAMPDIFF(SECOND, sent_at, UTC_TIMESTAMP())) AS n FROM messages WHERE message_id = ?", {std::to_string(newId)}) <= 5);   // the clock is UTC

    std::cout << "4. who may read and write\n";
    const auto stranger = chat.send(rahul, "1", [&] { auto b = utils::JsonValue::object(); b["content"] = "x"; b["sender_id"] = 2; return b; }());
    CHECK(stranger.status == 403 && count("SELECT COUNT(*) AS n FROM messages") == msgBefore + 1);   // impersonation refused, nothing stored
    db.execute("INSERT INTO conversations (conversation_name, conversation_type, created_by) VALUES ('Stage6b private', 'GROUP', ?)", {std::to_string(admin.id)});
    const int privateConv = count("SELECT conversation_id AS n FROM conversations WHERE conversation_name = 'Stage6b private'");
    CHECK(privateConv > 0);
    CHECK(chat.messages(rahul, std::to_string(privateConv), "", "").status == 404);            // exists, but Rahul is not a member
    auto b2 = utils::JsonValue::object(); b2["content"] = "hi";
    CHECK(chat.send(rahul, std::to_string(privateConv), b2).status == 404);
    CHECK(chat.messages(rahul, "99999", "", "").status == 404);

    std::cout << "5. clean up\n";
    db.execute("DELETE FROM messages WHERE message_id > ?", {std::to_string(maxMsgBefore)});
    db.execute("DELETE FROM conversations WHERE conversation_id = ?", {std::to_string(privateConv)});
    CHECK(count("SELECT COUNT(*) AS n FROM messages") == msgBefore);
    CHECK(count("SELECT COUNT(*) AS n FROM conversations") == convBefore);

    if (failures == 0) std::cout << "\nSTAGE 6b OK\n";
    else std::cout << "\nSTAGE 6b FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
