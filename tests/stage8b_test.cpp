// Stage 8b check: announcements and notifications against the real MySQL database.
#include <iostream>
#include <string>

#include "database/MySqlChatRepository.h"
#include "database/MySqlNotificationRepository.h"
#include "database/MySqlUserRepository.h"
#include "services/NotificationService.h"

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
    database::MySqlDatabase db(cfg);
    g_db = &db;
    std::string err;
    if (!db.connect(err)) { std::cout << "CANNOT CONNECT: " << err << "\n"; return 1; }

    database::MySqlUserRepository users(db);
    database::MySqlChatRepository chat(db);
    database::MySqlNotificationRepository repo(db);
    services::NotificationService svc(repo, chat);
    const auto rahul = *users.findByEmail("rahul@uniconnect.com");
    const auto priya = *users.findByEmail("priya@uniconnect.com");
    const auto rajesh = *users.findByEmail("rajesh@uniconnect.com");   // teaches DBMS: member of group 1
    const auto anjali = *users.findByEmail("anjali@uniconnect.com");   // not in group 1
    const auto admin = *users.findByEmail("admin@uniconnect.com");

    const int annBefore = count("SELECT COUNT(*) AS n FROM announcements");
    const int noteBefore = count("SELECT COUNT(*) AS n FROM notifications");
    const int maxAnn = count("SELECT MAX(announcement_id) AS n FROM announcements");
    const int members = count("SELECT COUNT(*) AS n FROM conversation_members WHERE conversation_id = 1");

    std::cout << "1. the seeded notifications\n";
    auto r = svc.list(rahul, "");
    CHECK(r.status == 200 && r.body["notifications"].size() >= 1);
    const auto& seeded = r.body["notifications"].at(r.body["notifications"].size() - 1);
    CHECK(seeded["title"].asString() == "New DBMS Announcement" && seeded["announcement_id"].asInt() == 1 && seeded["is_read"].asBool());   // Rahul read it
    const auto pr = svc.list(priya, "");
    CHECK(pr.body["unread_count"].asInt() >= 1);                                                                                              // Priya did not
    std::cout << "   Rahul has " << r.body["notifications"].size() << " notification(s); Priya has " << pr.body["unread_count"].asInt() << " unread\n";

    std::cout << "2. faculty announce to their group (DBMS, group 1)\n";
    const int priyaUnread = pr.body["unread_count"].asInt();
    std::vector<int> pushedTo;
    svc.setListener([&](const std::vector<models::Notification>& made) { for (const auto& n : made) pushedTo.push_back(n.user_id); });
    const std::string tricky = "Lab moved to Room 5; it's \"final\" \xF0\x9F\x8E\x93 '); DROP TABLE notifications;--";
    auto body = utils::JsonValue::object();
    body["group_id"] = 1;
    body["title"] = "Schedule change";
    body["content"] = tricky;
    auto res = svc.announce(rajesh, body);
    CHECK(res.status == 200 && res.body["recipients"].asInt() == members - 1);                 // everyone in the group except the sender
    const int annId = res.body["announcement_id"].asInt();
    CHECK(annId > maxAnn);
    CHECK(static_cast<int>(pushedTo.size()) == members - 1);
    CHECK(count("SELECT COUNT(*) AS n FROM announcements") == annBefore + 1);
    CHECK(count("SELECT COUNT(*) AS n FROM notifications") == noteBefore + members - 1);
    CHECK(count("SELECT COUNT(*) AS n FROM announcements WHERE announcement_id = ? AND course_id = 1 AND section_id = 1 AND created_by = ?", {std::to_string(annId), std::to_string(rajesh.id)}) == 1);   // scoped to the same course and section as the group, authored by Rajesh
    CHECK(count("SELECT COUNT(*) AS n FROM notifications WHERE announcement_id = ? AND user_id = ?", {std::to_string(annId), std::to_string(rajesh.id)}) == 0);   // the sender is not notified

    std::cout << "3. recipients see it, with the text stored exactly\n";
    r = svc.list(rahul, "");
    CHECK(r.body["notifications"].at(0)["title"].asString() == "Schedule change" && r.body["notifications"].at(0)["content"].asString() == tricky);
    CHECK(!r.body["notifications"].at(0)["is_read"].asBool());
    const std::string when = r.body["notifications"].at(0)["created_at"].asString();
    CHECK(when.size() == 20 && when.back() == 'Z');
    CHECK(svc.list(priya, "").body["unread_count"].asInt() == priyaUnread + 1);
    CHECK(count("SELECT COUNT(*) AS n FROM notifications") >= 1);                              // the table is still there after the injection text

    std::cout << "4. who may post\n";
    auto b2 = utils::JsonValue::object(); b2["group_id"] = 1; b2["title"] = "t"; b2["content"] = "c";
    CHECK(svc.announce(rahul, b2).status == 403);                                              // students cannot
    CHECK(svc.announce(anjali, b2).status == 404);                                             // Dr. Mehta is not in group 1
    auto b3 = utils::JsonValue::object(); b3["group_id"] = 99999; b3["title"] = "t"; b3["content"] = "c";
    CHECK(svc.announce(admin, b3).status == 404);
    CHECK(svc.announce(admin, b2).status == 200);                                              // an admin can, and is not a member of group 1

    std::cout << "5. marking read only touches your own rows\n";
    r = svc.list(rahul, "");
    const int rahulNote = r.body["notifications"].at(0)["notification_id"].asInt();
    auto mark = utils::JsonValue::object();
    auto ids = utils::JsonValue::array();
    ids.push_back(rahulNote);
    mark["ids"] = ids;
    CHECK(svc.markRead(priya, mark).body["updated"].asInt() == 0);                             // Priya cannot mark Rahul's
    CHECK(svc.markRead(rahul, mark).body["updated"].asInt() == 1);
    CHECK(svc.markRead(rahul, mark).body["updated"].asInt() == 0);                             // already read
    auto all = utils::JsonValue::object();
    all["all"] = true;
    CHECK(svc.markRead(priya, all).body["unread_count"].asInt() == 0);
    CHECK(svc.list(rahul, "2").status == 403 && svc.list(admin, "2").status == 200);

    std::cout << "6. clean up\n";
    db.execute("DELETE FROM announcements WHERE announcement_id > ?", {std::to_string(maxAnn)});   // notifications go with them
    db.execute("UPDATE notifications SET is_read = 0 WHERE user_id = ? AND notification_id IN (2, 3)", {std::to_string(priya.id)});   // restore Priya's seeded unread state
    CHECK(count("SELECT COUNT(*) AS n FROM announcements") == annBefore);
    CHECK(count("SELECT COUNT(*) AS n FROM notifications") == noteBefore);

    if (failures == 0) std::cout << "\nSTAGE 8b OK\n";
    else std::cout << "\nSTAGE 8b FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
