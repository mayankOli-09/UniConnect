#pragma once

#include <string>
#include "http/HttpRequest.h"

namespace uniconnect {
namespace http {

enum class HttpParseResult {
    COMPLETE,
    INCOMPLETE,
    ERROR
};

class HttpParser {
public:
    static HttpParseResult parse(const std::string& raw_data, HttpRequest& out_request, size_t& bytes_consumed);

private:
    static void parseQueryParams(const std::string& query_str, std::map<std::string, std::string>& out_params);
};

} // namespace http
} // namespace uniconnect
