#pragma once
#include <optional>
#include <string>
#include <vector>

#include "models/Chat.h"

namespace uniconnect {
namespace database {

class ChatRepository {
public:
    virtual ~ChatRepository() = default;

    // ---- groups
    virtual std::vector<models::ChatGroup> groupsForUser(int user_id) = 0;                 // newest activity first
    virtual std::optional<models::ChatGroup> groupFor(int group_id, int user_id) = 0;      // one group as this person sees it
    virtual bool isMember(int group_id, int user_id) = 0;
    virtual std::vector<int> memberIds(int group_id) = 0;
    virtual std::vector<std::pair<int, std::string>> memberNames(int group_id) = 0;   // (user id, full name)
    virtual std::string groupType(int group_id) = 0;                                       // "" when it does not exist

    // ---- messages
    // Newest first, at most `limit`; when before_id is set only messages with a smaller id.
    virtual std::vector<models::ChatMessage> messages(int group_id, int limit, std::optional<int> before_id) = 0;
    virtual std::optional<models::ChatMessage> addMessage(int group_id, int sender_id, const std::string& content) = 0;

    // ---- people and new chats
    virtual std::vector<models::DirectoryUser> searchUsers(int exclude_user, const std::string& query, int limit) = 0;
    virtual bool usersExist(const std::vector<int>& user_ids) = 0;
    virtual std::optional<models::ChatGroup> findOrCreateDirect(int user_a, int user_b) = 0;    // the same chat every time for the same two people
    virtual std::optional<models::ChatGroup> createGroup(int creator, const std::string& name, const std::vector<int>& member_ids) = 0;
    virtual bool leaveGroup(int group_id, int user_id) = 0;

    // ---- per-person settings
    virtual void setMuted(int group_id, int user_id, bool muted) = 0;
    virtual int markRead(int group_id, int user_id, std::optional<int> up_to_message_id) = 0;   // returns the new read position
};

} // namespace database
} // namespace uniconnect
