#pragma once
#include "database/ChatRepository.h"
#include "database/MySqlDatabase.h"

namespace uniconnect {
namespace database {

class MySqlChatRepository : public ChatRepository {
public:
    explicit MySqlChatRepository(MySqlDatabase& db) : db_(db) {}

    std::vector<models::ChatGroup> groupsForUser(int user_id) override;
    std::optional<models::ChatGroup> groupFor(int group_id, int user_id) override;
    bool isMember(int group_id, int user_id) override;
    std::vector<int> memberIds(int group_id) override;
    std::vector<std::pair<int, std::string>> memberNames(int group_id) override;
    std::string groupType(int group_id) override;

    std::vector<models::ChatMessage> messages(int group_id, int limit, std::optional<int> before_id) override;
    std::optional<models::ChatMessage> addMessage(int group_id, int sender_id, const std::string& content) override;

    std::vector<models::DirectoryUser> searchUsers(int exclude_user, const std::string& query, int limit) override;
    bool usersExist(const std::vector<int>& user_ids) override;
    std::optional<models::ChatGroup> findOrCreateDirect(int user_a, int user_b) override;
    std::optional<models::ChatGroup> createGroup(int creator, const std::string& name, const std::vector<int>& member_ids) override;
    bool leaveGroup(int group_id, int user_id) override;

    void setMuted(int group_id, int user_id, bool muted) override;
    int markRead(int group_id, int user_id, std::optional<int> up_to_message_id) override;

private:
    std::vector<models::ChatGroup> queryGroups(int user_id, std::optional<int> group_id);
    MySqlDatabase& db_;
};

} // namespace database
} // namespace uniconnect
