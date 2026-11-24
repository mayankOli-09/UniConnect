#pragma once
#include <string>

#include "utils/Json.h"

namespace uniconnect {
namespace services {

// Outcome of a service call: a body on success, or an HTTP status >= 400 with a message.
struct ServiceResult {
    int status = 200;
    std::string error;
    utils::JsonValue body;
};

} // namespace services
} // namespace uniconnect
