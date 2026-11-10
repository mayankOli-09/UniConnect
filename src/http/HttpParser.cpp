#include "http/HttpParser.h"
#include <sstream>
#include <algorithm>

namespace uniconnect {
namespace http {

void HttpParser::parseQueryParams(const std::string& query_str, std::map<std::string, std::string>& out_params) {
    std::istringstream stream(query_str);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        auto eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string val = pair.substr(eq_pos + 1);
            out_params[key] = val;
        } else if (!pair.empty()) {
            out_params[pair] = "";
        }
    }
}

HttpParseResult HttpParser::parse(const std::string& raw_data, HttpRequest& out_request, size_t& bytes_consumed) {
    bytes_consumed = 0;
    auto header_end_pos = raw_data.find("\r\n\r\n");
    if (header_end_pos == std::string::npos) {
        // Headers not fully received yet
        return HttpParseResult::INCOMPLETE;
    }

    std::string header_section = raw_data.substr(0, header_end_pos);
    std::istringstream header_stream(header_section);

    // Request line
    std::string request_line;
    if (!std::getline(header_stream, request_line)) {
        return HttpParseResult::ERROR;
    }
    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    std::istringstream line_stream(request_line);
    std::string method_str, raw_path, version;
    if (!(line_stream >> method_str >> raw_path >> version)) {
        return HttpParseResult::ERROR;
    }

    out_request.method = stringToHttpMethod(method_str);
    out_request.raw_path = raw_path;
    out_request.version = version;

    // Parse path vs query params
    auto query_pos = raw_path.find('?');
    if (query_pos != std::string::npos) {
        out_request.path = raw_path.substr(0, query_pos);
        out_request.query_string = raw_path.substr(query_pos + 1);
        parseQueryParams(out_request.query_string, out_request.query_params);
    } else {
        out_request.path = raw_path;
        out_request.query_string.clear();
        out_request.query_params.clear();
    }

    // Headers
    out_request.headers.clear();
    std::string line;
    while (std::getline(header_stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) continue;

        auto colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string val = line.substr(colon_pos + 1);

            // Trim leading spaces from val
            size_t start = val.find_first_not_of(" \t");
            if (start != std::string::npos) {
                val = val.substr(start);
            }
            out_request.headers[key] = val;
        }
    }

    // Body check via Content-Length
    size_t body_start_pos = header_end_pos + 4;
    size_t content_length = 0;
    std::string cl_str = out_request.getHeader("Content-Length");
    if (!cl_str.empty()) {
        try {
            content_length = std::stoul(cl_str);
        } catch (...) {
            content_length = 0;
        }
    }

    if (raw_data.size() < body_start_pos + content_length) {
        // Need more body bytes
        return HttpParseResult::INCOMPLETE;
    }

    out_request.body = raw_data.substr(body_start_pos, content_length);
    bytes_consumed = body_start_pos + content_length;
    return HttpParseResult::COMPLETE;
}

} // namespace http
} // namespace uniconnect
