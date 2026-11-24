// Stage 11b check: admin notices on the real MySQL database (needs database/part3_notices.sql applied).
#include <iostream>
#include <string>

#include "database/MySqlChatRepository.h"
#include "database/MySqlNotificationRepository.h"
#include "database/MySqlUserRepository.h"
#include "services/NotificationService.h"

using namespace uniconnect;
using database::rowInt;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

static database::MySqlDatabase* g_db = nullptr;
static int count(const std::string& sql, const std::vector<database::DbValue>& p = {}) {
    const auto r = g_db->execute(sql, p);
    return r.ok && !r.rows.empty() ? rowInt(r.rows[0], "n") : -1;
}
static utils::JsonValue body(const std::string& audience, const std::string& title, const std::string& content) {
    auto b = utils::JsonValue::object();
    b["audience"] = audience; b["title"] = title; b["content"] = content;
    return b;
}

int main() {
    database::DbConfig cfg = database::DbConfig::fromEnvironment();
    if (cfg.password.empty()) cfg.password = "UniConnect#Dev2026";   // documented local development password
    database::MySqlDatabase db(cfg);
    g_db = &db;
    std::string err;
    if (!db.connect(err)) { std::cout << "CANNOT CONNECT: " << err << "\n"; return 1; }
    if (count("SELECT COUNT(*) AS n FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'notifications' AND COLUMN_NAME = 'kind'") != 1) {
        std::cout << "The notifications.kind column is missing. Run:  mysql -u root uniconnect -e \"source database/part3_notices.sql\"\n";
        return 1;
    }
    database::MySqlUserRepository users(db);
    database::MySqlChatRepository chat(db);
    database::MySqlNotificationRepository repo(db);
    services::NotificationService svc(repo, chat);
    const auto admin = *users.findByEmail("admin@uniconnect.com");
    const auto rahul = *users.findByEmail("rahul@uniconnect.com");
    const auto rajesh = *users.findByEmail("rajesh@uniconnect.com");

    const int students = count("SELECT COUNT(*) AS n FROM users WHERE role = 'STUDENT'");
    const int faculty = count("SELECT COUNT(*) AS n FROM users WHERE role = 'FACULTY'");
    const int annBefore = count("SELECT COUNT(*) AS n FROM announcements");
    const int noteBefore = count("SELECT COUNT(*) AS n FROM notifications");
    const int maxAnn = count("SELECT COALESCE(MAX(announcement_id), 0) AS n FROM announcements");

    std::cout << "1. existing notifications are announcements\n";
    auto seeded = svc.list(rahul, "");
    CHECK(seeded.body["notifications"].size() >= 1 && seeded.body["notifications"].at(0)["kind"].asString() == "ANNOUNCEMENT");

    std::cout << "2. notices reach exactly the chosen roles (" << students << " students, " << faculty << " faculty)\n";
    std::vector<int> pushed;
    svc.setListener([&](const std::vector<models::Notification>& made) { for (const auto& n : made) pushed.push_back(n.user_id); });
    auto r = svc.notice(admin, body("STUDENTS", "Exam timetable", "Exams start on Monday."));
    CHECK(r.status == 200 && r.body["recipients"].asInt() == students && static_cast<int>(pushed.size()) == students);
    const int annId = r.body["announcement_id"].asInt();
    CHECK(annId > maxAnn);
    CHECK(count("SELECT COUNT(*) AS n FROM notifications WHERE announcement_id = ? AND kind = 'NOTICE'", {std::to_string(annId)}) == students);
    CHECK(count("SELECT COUNT(*) AS n FROM notifications n JOIN users u ON u.user_id = n.user_id WHERE n.announcement_id = ? AND u.role <> 'STUDENT'", {std::to_string(annId)}) == 0);
    CHECK(count("SELECT COUNT(*) AS n FROM announcements WHERE announcement_id = ? AND created_by = ? AND course_id IS NULL AND section_id IS NULL AND department_id IS NULL",
                {std::to_string(annId), std::to_string(admin.id)}) == 1);
    auto mine = svc.list(rahul, "");
    CHECK(mine.body["notifications"].at(0)["kind"].asString() == "NOTICE" && mine.body["notifications"].at(0)["title"].asString() == "Exam timetable" && !mine.body["notifications"].at(0)["is_read"].asBool());

    pushed.clear();
    r = svc.notice(admin, body("FACULTY", "Staff meeting", "Friday 4 pm."));
    CHECK(r.status == 200 && r.body["recipients"].asInt() == faculty && static_cast<int>(pushed.size()) == faculty);
    CHECK(svc.list(rajesh, "").body["notifications"].at(0)["title"].asString() == "Staff meeting");
    pushed.clear();
    r = svc.notice(admin, body("both", "Holiday", "Campus closed on Friday."));
    CHECK(r.status == 200 && r.body["recipients"].asInt() == students + faculty);
    CHECK(count("SELECT COUNT(*) AS n FROM notifications WHERE user_id = ? AND kind = 'NOTICE'", {std::to_string(admin.id)}) == 0);   // the sender is not notified

    std::cout << "3. who may send, and what is refused\n";
    CHECK(svc.notice(rahul, body("BOTH", "t", "c")).status == 403 && svc.notice(rajesh, body("BOTH", "t", "c")).status == 403);
    CHECK(svc.notice(admin, body("EVERYONE", "t", "c")).status == 400 && svc.notice(admin, body("BOTH", " ", "c")).status == 400);

    std::cout << "4. awkward text is stored exactly, and the notice can be read and marked\n";
    const std::string tricky = "Don't miss it; \"final\" \xF0\x9F\x8E\x93 '); DROP TABLE notifications;--";
    r = svc.notice(admin, body("STUDENTS", "Careful", tricky));
    CHECK(r.status == 200);
    mine = svc.list(rahul, "");
    CHECK(mine.body["notifications"].at(0)["content"].asString() == tricky);
    auto ids = utils::JsonValue::array();
    ids.push_back(mine.body["notifications"].at(0)["notification_id"].asInt());
    auto mark = utils::JsonValue::object();
    mark["ids"] = ids;
    CHECK(svc.markRead(rahul, mark).body["updated"].asInt() == 1);
    CHECK(svc.list(rahul, "").body["notifications"].at(0)["kind"].asString() == "NOTICE");

    std::cout << "5. clean up\n";
    db.execute("DELETE FROM announcements WHERE announcement_id > ?", {std::to_string(maxAnn)});    // their notifications go with them
    CHECK(count("SELECT COUNT(*) AS n FROM announcements") == annBefore);
    CHECK(count("SELECT COUNT(*) AS n FROM notifications") == noteBefore);

    if (failures == 0) std::cout << "\nSTAGE 11b OK\n";
    else std::cout << "\nSTAGE 11b FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}
