#include "utils/Json.h"
#include <cctype>
#include <stdexcept>
#include <iomanip>

namespace uniconnect {
namespace utils {

static void escapeString(const std::string& str, std::ostringstream& oss) {
    oss << '"';
    for (char c : str) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
                break;
        }
    }
    oss << '"';
}

std::string JsonValue::serialize(bool pretty, int indent) const {
    std::ostringstream oss;
    std::string indent_str(pretty ? indent * 2 : 0, ' ');
    std::string next_indent_str(pretty ? (indent + 1) * 2 : 0, ' ');
    std::string nl = pretty ? "\n" : "";
    std::string sp = pretty ? " " : "";

    switch (type) {
        case JsonType::NUL:
            oss << "null";
            break;
        case JsonType::BOOLEAN:
            oss << (bool_val ? "true" : "false");
            break;
        case JsonType::NUMBER:
            if (num_val == static_cast<int64_t>(num_val)) {
                oss << static_cast<int64_t>(num_val);
            } else {
                oss << num_val;
            }
            break;
        case JsonType::STRING:
            escapeString(str_val, oss);
            break;
        case JsonType::ARRAY:
            if (arr_val.empty()) {
                oss << "[]";
            } else {
                oss << "[" << nl;
                for (size_t i = 0; i < arr_val.size(); ++i) {
                    oss << next_indent_str << arr_val[i].serialize(pretty, indent + 1);
                    if (i + 1 < arr_val.size()) oss << ",";
                    oss << nl;
                }
                oss << indent_str << "]";
            }
            break;
        case JsonType::OBJECT:
            if (obj_val.empty()) {
                oss << "{}";
            } else {
                oss << "{" << nl;
                size_t i = 0;
                for (const auto& [k, v] : obj_val) {
                    oss << next_indent_str;
                    escapeString(k, oss);
                    oss << ":" << sp << v.serialize(pretty, indent + 1);
                    if (++i < obj_val.size()) oss << ",";
                    oss << nl;
                }
                oss << indent_str << "}";
            }
            break;
    }
    return oss.str();
}

namespace {
class JsonParser {
public:
    explicit JsonParser(const std::string& str) : src_(str), pos_(0) {}

    JsonValue parse() {
        skipWhitespace();
        auto val = parseValue();
        skipWhitespace();
        return val;
    }

private:
    const std::string& src_;
    size_t pos_;

    char peek() const {
        return pos_ < src_.size() ? src_[pos_] : '\0';
    }

    char get() {
        return pos_ < src_.size() ? src_[pos_++] : '\0';
    }

    void skipWhitespace() {
        while (pos_ < src_.size() && (src_[pos_] == ' ' || src_[pos_] == '\t' || src_[pos_] == '\n' || src_[pos_] == '\r')) {
            pos_++;
        }
    }

    JsonValue parseValue() {
        skipWhitespace();
        char c = peek();
        if (c == 'n') return parseNull();
        if (c == 't' || c == 'f') return parseBool();
        if (c == '"') return parseString();
        if (c == '[') return parseArray();
        if (c == '{') return parseObject();
        if (c == '-' || std::isdigit(c)) return parseNumber();
        return JsonValue();
    }

    JsonValue parseNull() {
        if (src_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
            return JsonValue();
        }
        return JsonValue();
    }

    JsonValue parseBool() {
        if (src_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            return JsonValue(true);
        }
        if (src_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            return JsonValue(false);
        }
        return JsonValue(false);
    }

    JsonValue parseNumber() {
        size_t start = pos_;
        if (peek() == '-') get();
        while (std::isdigit(peek())) get();
        if (peek() == '.') {
            get();
            while (std::isdigit(peek())) get();
        }
        if (peek() == 'e' || peek() == 'E') {
            get();
            if (peek() == '+' || peek() == '-') get();
            while (std::isdigit(peek())) get();
        }
        std::string num_str = src_.substr(start, pos_ - start);
        double val = 0.0;
        try {
            val = std::stod(num_str);
        } catch (...) {}
        return JsonValue(val);
    }

    JsonValue parseString() {
        get(); // skip opening "
        std::string str;
        while (pos_ < src_.size()) {
            char c = get();
            if (c == '"') {
                return JsonValue(str);
            }
            if (c == '\\') {
                char esc = get();
                switch (esc) {
                    case '"': str += '"'; break;
                    case '\\': str += '\\'; break;
                    case '/': str += '/'; break;
                    case 'b': str += '\b'; break;
                    case 'f': str += '\f'; break;
                    case 'n': str += '\n'; break;
                    case 'r': str += '\r'; break;
                    case 't': str += '\t'; break;
                    case 'u': {
                        // basic 4 hex digits
                        if (pos_ + 4 <= src_.size()) {
                            std::string hex_str = src_.substr(pos_, 4);
                            pos_ += 4;
                            unsigned int cp = 0;
                            std::stringstream ss;
                            ss << std::hex << hex_str;
                            ss >> cp;
                            if (cp < 128) str += static_cast<char>(cp);
                            else str += '?';
                        }
                        break;
                    }
                    default: str += esc; break;
                }
            } else {
                str += c;
            }
        }
        return JsonValue(str);
    }

    JsonValue parseArray() {
        get(); // skip '['
        JsonValue arr = JsonValue::array();
        skipWhitespace();
        if (peek() == ']') {
            get();
            return arr;
        }
        while (pos_ < src_.size()) {
            arr.push_back(parseValue());
            skipWhitespace();
            char c = get();
            if (c == ']') break;
            if (c != ',') break;
        }
        return arr;
    }

    JsonValue parseObject() {
        get(); // skip '{'
        JsonValue obj = JsonValue::object();
        skipWhitespace();
        if (peek() == '}') {
            get();
            return obj;
        }
        while (pos_ < src_.size()) {
            skipWhitespace();
            if (peek() != '"') break;
            JsonValue keyVal = parseString();
            std::string key = keyVal.asString();
            skipWhitespace();
            if (get() != ':') break;
            JsonValue val = parseValue();
            obj[key] = val;
            skipWhitespace();
            char c = get();
            if (c == '}') break;
            if (c != ',') break;
        }
        return obj;
    }
};
}

JsonValue JsonValue::parse(const std::string& json_str) {
    JsonParser parser(json_str);
    return parser.parse();
}

bool JsonValue::tryParse(const std::string& json_str, JsonValue& out_value) {
    try {
        JsonParser parser(json_str);
        out_value = parser.parse();
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace utils
} // namespace uniconnect
