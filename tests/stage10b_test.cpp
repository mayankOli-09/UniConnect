// Stage 10b check: direct chats, user-made groups, mute, unread counts and the people directory, on the real MySQL database.
#include <atomic>
#include <iostream>
#include <string>
#include <thread>

#include "database/MySqlChatRepository.h"
#include "database/MySqlDatabase.h"

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

int main() {
    database::DbConfig cfg = database::DbConfig::fromEnvironment();
    if (cfg.password.empty()) cfg.password = "UniConnect#Dev2026";   // documented local development password
    database::MySqlDatabase db(cfg), db2(cfg);
    g_db = &db;
    std::string err;
    if (!db.connect(err) || !db2.connect(err)) { std::cout << "CANNOT CONNECT: " << err << "\n"; return 1; }
    if (count("SELECT COUNT(*) AS n FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'conversation_settings'") != 1) {
        std::cout << "The conversation_settings table is missing. Run:  mysql -u root uniconnect -e \"source database/part2_chat.sql\"\n";
        return 1;
    }
    database::MySqlChatRepository repo(db), repo2(db2);

    const int maxConv = count("SELECT MAX(conversation_id) AS n FROM conversations");
    const int convBefore = count("SELECT COUNT(*) AS n FROM conversations");
    const int msgBefore = count("SELECT COUNT(*) AS n FROM messages");
    const int settingsBefore = count("SELECT COUNT(*) AS n FROM conversation_settings");

    std::cout << "1. a person's groups, with unread counts and previews (Rahul, user 1)\n";
    auto groups = repo.groupsForUser(1);
    CHECK(groups.size() >= 3);
    int unreadTotal = 0;
    for (const auto& g : groups) unreadTotal += g.unread_count;
    CHECK(unreadTotal == count("SELECT COUNT(*) AS n FROM messages m JOIN conversation_members cm ON cm.conversation_id = m.conversation_id AND cm.user_id = 1 WHERE m.sender_id <> 1"));
    const models::ChatGroup* dbms = nullptr;
    for (const auto& g : groups) if (g.id == 1) dbms = &g;
    CHECK(dbms && !dbms->last_text.empty() && !dbms->last_sender.empty() && dbms->last_at.size() == 20);
    CHECK(groups.front().id == 1 || !groups.front().last_at.empty());             // groups with messages come first
    bool ordered = true;
    for (size_t i = 1; i < groups.size(); ++i) if (!groups[i].last_at.empty() && groups[i - 1].last_at.empty()) ordered = false;
    CHECK(ordered);

    std::cout << "2. the people directory\n";
    auto all = repo.searchUsers(1, "", 50);
    CHECK(all.size() == 6 && all.front().id != 1);                                  // everyone except Rahul
    CHECK(repo.searchUsers(2, "rah", 50).size() == 1 && repo.searchUsers(2, "rah", 50)[0].name == "Rahul Sharma");
    CHECK(repo.searchUsers(1, "%", 50).empty() && repo.searchUsers(1, "_", 50).empty());   // % and _ are plain letters, not patterns
    CHECK(repo.searchUsers(1, "", 2).size() == 2);
    CHECK(repo.searchUsers(1, "' OR '1'='1", 50).empty());
    CHECK(repo.usersExist({1, 2}) && repo.usersExist({}) && !repo.usersExist({1, 99999}) && repo.usersExist({1, 1, 2}));

    std::cout << "3. direct chats: one per pair, named for the other person\n";
    auto d1 = repo.findOrCreateDirect(1, 2);
    auto d2 = repo.findOrCreateDirect(2, 1);
    CHECK(d1 && d2 && d1->id == d2->id && d1->type == "PRIVATE");
    CHECK(d1->name == "Priya Singh" && d1->other_user_id == 2 && d2->name == "Rahul Sharma" && d2->other_user_id == 1 && d1->member_count == 2);
    CHECK(count("SELECT COUNT(*) AS n FROM conversations c JOIN conversation_members a ON a.conversation_id = c.conversation_id AND a.user_id = 1 "
                "JOIN conversation_members b ON b.conversation_id = c.conversation_id AND b.user_id = 2 WHERE c.conversation_type = 'PRIVATE'") == 1);
    CHECK(!repo.findOrCreateDirect(1, 1) && !repo.findOrCreateDirect(1, 99999));
    {   // two requests for the same new pair at the same time, from two separate connections
        std::optional<models::ChatGroup> ra, rb;
        std::thread ta([&] { ra = repo.findOrCreateDirect(3, 4); });
        std::thread tb([&] { rb = repo2.findOrCreateDirect(4, 3); });
        ta.join(); tb.join();
        CHECK(ra && rb && ra->id == rb->id);
        CHECK(count("SELECT COUNT(*) AS n FROM conversations c JOIN conversation_members a ON a.conversation_id = c.conversation_id AND a.user_id = 3 "
                    "JOIN conversation_members b ON b.conversation_id = c.conversation_id AND b.user_id = 4 WHERE c.conversation_type = 'PRIVATE'") == 1);
    }

    std::cout << "4. unread counts, previews and read position\n";
    auto m1 = repo.addMessage(d1->id, 1, "Hi Priya");
    auto m2 = repo.addMessage(d1->id, 1, "Are you coming to the lab?");
    CHECK(m1 && m2);
    auto forPriya = repo.groupFor(d1->id, 2), forRahul = repo.groupFor(d1->id, 1);
    CHECK(forPriya && forRahul && forPriya->unread_count == 2 && forRahul->unread_count == 0);   // your own messages are never unread for you
    CHECK(forPriya->last_text == "Are you coming to the lab?" && forPriya->last_sender == "Rahul Sharma" && forPriya->last_at.size() == 20);
    CHECK(repo.groupsForUser(1).front().id == d1->id && repo.groupsForUser(2).front().id == d1->id);   // newest activity first
    CHECK(repo.markRead(d1->id, 2, std::nullopt) == m2->id);
    CHECK(repo.groupFor(d1->id, 2)->unread_count == 0);
    auto m3 = repo.addMessage(d1->id, 1, "Reply when free");
    CHECK(repo.groupFor(d1->id, 2)->unread_count == 1);
    CHECK(repo.markRead(d1->id, 2, m1->id) == m2->id);                                             // the position never moves backwards
    CHECK(repo.groupFor(d1->id, 2)->unread_count == 1);
    CHECK(repo.markRead(d1->id, 2, m3->id) == m3->id && repo.groupFor(d1->id, 2)->unread_count == 0);

    std::cout << "5. mute is per person and is remembered\n";
    repo.setMuted(d1->id, 2, true);
    CHECK(repo.groupFor(d1->id, 2)->muted && !repo.groupFor(d1->id, 1)->muted);
    CHECK(repo.markRead(d1->id, 2, std::nullopt) == m3->id && repo.groupFor(d1->id, 2)->muted);   // reading does not undo the mute
    repo.setMuted(d1->id, 2, false);
    CHECK(!repo.groupFor(d1->id, 2)->muted);
    repo.setMuted(1, 1, true);                                                                    // a seeded course group
    CHECK(repo.groupFor(1, 1)->muted);
    repo.setMuted(1, 1, false);

    std::cout << "6. user-made groups\n";
    auto g = repo.createGroup(1, "Study circle \xF0\x9F\x93\x9A", {2, 3, 3});
    CHECK(g && g->type == "GROUP" && g->name == "Study circle \xF0\x9F\x93\x9A" && g->member_count == 3);
    CHECK(repo.isMember(g->id, 1) && repo.isMember(g->id, 2) && repo.isMember(g->id, 3) && !repo.isMember(g->id, 4));
    CHECK(repo.groupType(g->id) == "GROUP" && repo.groupType(d1->id) == "PRIVATE" && repo.groupType(1) == "COURSE" && repo.groupType(99999).empty());
    CHECK(repo.groupFor(g->id, 4) == std::nullopt);
    CHECK(repo.leaveGroup(g->id, 2) && !repo.isMember(g->id, 2) && repo.groupFor(g->id, 1)->member_count == 2);
    CHECK(!repo.leaveGroup(g->id, 2));                                                             // already left
    repo.addMessage(g->id, 1, "hello group");
    CHECK(repo.leaveGroup(g->id, 1) && repo.leaveGroup(g->id, 3));
    CHECK(repo.groupType(g->id).empty());                                                          // the empty group (and its messages) are gone

    std::cout << "7. clean up\n";
    db.execute("DELETE FROM conversations WHERE conversation_id > ?", {std::to_string(maxConv)});  // settings and messages go with them
    db.execute("DELETE FROM conversation_settings WHERE conversation_id = 1 AND user_id = 1");
    CHECK(count("SELECT COUNT(*) AS n FROM conversations") == convBefore);
    CHECK(count("SELECT COUNT(*) AS n FROM messages") == msgBefore);
    CHECK(count("SELECT COUNT(*) AS n FROM conversation_settings") == settingsBefore);

    if (failures == 0) std::cout << "\nSTAGE 10b OK\n";
    else std::cout << "\nSTAGE 10b FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
