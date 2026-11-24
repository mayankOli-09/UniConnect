#pragma once
// Minimal JSON value type: build, parse, serialize. Header-only, standard C++20, Windows/Linux.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace uniconnect {
namespace utils {

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    JsonValue() = default;
    JsonValue(std::nullptr_t) {}
    JsonValue(bool b) : type_(Type::Bool), b_(b) {}
    JsonValue(const char* s) : type_(s ? Type::String : Type::Null), s_(s ? s : "") {}
    JsonValue(std::string s) : type_(Type::String), s_(std::move(s)) {}
    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>>
    JsonValue(T n) : type_(Type::Number) {
        if constexpr (std::is_integral_v<T>) { isInt_ = true; i_ = static_cast<long long>(n); d_ = static_cast<double>(n); }
        else { d_ = static_cast<double>(n); }
    }

    static JsonValue object() { JsonValue v; v.type_ = Type::Object; return v; }
    static JsonValue array()  { JsonValue v; v.type_ = Type::Array;  return v; }

    Type type() const { return type_; }
    bool isNull() const   { return type_ == Type::Null; }
    bool isObject() const { return type_ == Type::Object; }
    bool isArray() const  { return type_ == Type::Array; }
    bool isString() const { return type_ == Type::String; }
    bool isNumber() const { return type_ == Type::Number; }

    // ---- object access
    JsonValue& operator[](const std::string& key) {
        if (type_ == Type::Null) type_ = Type::Object;
        if (type_ != Type::Object) return nullRef();
        for (size_t i = 0; i < keys_.size(); ++i) if (keys_[i] == key) return items_[i];
        keys_.push_back(key);
        items_.emplace_back();
        return items_.back();
    }
    const JsonValue& operator[](const std::string& key) const {
        if (type_ == Type::Object)
            for (size_t i = 0; i < keys_.size(); ++i) if (keys_[i] == key) return items_[i];
        return nullRef();
    }
    JsonValue& operator[](const char* key) { return (*this)[std::string(key)]; }
    const JsonValue& operator[](const char* key) const { return (*this)[std::string(key)]; }
    bool contains(const std::string& key) const {
        if (type_ != Type::Object) return false;
        for (const auto& k : keys_) if (k == key) return true;
        return false;
    }

    // ---- array access
    void push_back(JsonValue v) {
        if (type_ == Type::Null) type_ = Type::Array;
        if (type_ == Type::Array) items_.push_back(std::move(v));
    }
    const JsonValue& at(size_t i) const { return (type_ == Type::Array && i < items_.size()) ? items_[i] : nullRef(); }
    size_t size() const { return (type_ == Type::Array || type_ == Type::Object) ? items_.size() : 0; }

    // ---- typed reads with defaults (never throw)
    std::string asString(const std::string& def = "") const {
        switch (type_) {
            case Type::String: return s_;
            case Type::Number: return numberToString();
            case Type::Bool:   return b_ ? "true" : "false";
            default:           return def;
        }
    }
    int asInt(int def = 0) const {
        if (type_ == Type::Number) return isInt_ ? static_cast<int>(i_) : static_cast<int>(d_);
        if (type_ == Type::Bool) return b_ ? 1 : 0;
        if (type_ == Type::String && !s_.empty()) {
            char* end = nullptr;
            long v = std::strtol(s_.c_str(), &end, 10);
            if (end && *end == '\0') return static_cast<int>(v);
        }
        return def;
    }
    double asDouble(double def = 0) const {
        if (type_ == Type::Number) return isInt_ ? static_cast<double>(i_) : d_;
        if (type_ == Type::String && !s_.empty()) {
            char* end = nullptr;
            double v = std::strtod(s_.c_str(), &end);
            if (end && *end == '\0') return v;
        }
        return def;
    }
    bool asBool(bool def = false) const {
        if (type_ == Type::Bool) return b_;
        if (type_ == Type::Number) return asInt() != 0;
        if (type_ == Type::String) return s_ == "true" || s_ == "1";
        return def;
    }

    // ---- serialize / parse
    std::string serialize(bool pretty = false) const {
        std::string out;
        write(out, pretty, 0);
        return out;
    }
    static bool tryParse(const std::string& text, JsonValue& out) {
        Parser p{text.c_str(), text.c_str() + text.size()};
        JsonValue v;
        p.skipWs();
        if (!p.parseValue(v, 0)) return false;
        p.skipWs();
        if (p.cur != p.end) return false;
        out = std::move(v);
        return true;
    }

private:
    Type type_ = Type::Null;
    bool b_ = false;
    bool isInt_ = false;
    long long i_ = 0;
    double d_ = 0;
    std::string s_;
    std::vector<std::string> keys_;   // objects only (insertion ordered)
    std::vector<JsonValue> items_;    // array elements / object values

    static JsonValue& nullRef() { static thread_local JsonValue n; n = JsonValue(); return n; }

    std::string numberToString() const {
        if (isInt_) return std::to_string(i_);
        if (!std::isfinite(d_)) return "null";
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%.15g", d_);
        return buf;
    }

    static void escape(std::string& o, const std::string& s) {
        o += '"';
        for (unsigned char c : s) {
            switch (c) {
                case '"':  o += "\\\""; break;
                case '\\': o += "\\\\"; break;
                case '\n': o += "\\n"; break;
                case '\r': o += "\\r"; break;
                case '\t': o += "\\t"; break;
                case '\b': o += "\\b"; break;
                case '\f': o += "\\f"; break;
                default:
                    if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
                    else o += static_cast<char>(c);
            }
        }
        o += '"';
    }

    void write(std::string& o, bool pretty, int depth) const {
        auto nl = [&](int d) { if (pretty) { o += '\n'; o.append(static_cast<size_t>(d) * 2, ' '); } };
        switch (type_) {
            case Type::Null:   o += "null"; break;
            case Type::Bool:   o += b_ ? "true" : "false"; break;
            case Type::Number: o += numberToString(); break;
            case Type::String: escape(o, s_); break;
            case Type::Array:
                if (items_.empty()) { o += "[]"; break; }
                o += '[';
                for (size_t i = 0; i < items_.size(); ++i) {
                    if (i) o += ',';
                    nl(depth + 1);
                    items_[i].write(o, pretty, depth + 1);
                }
                nl(depth); o += ']';
                break;
            case Type::Object:
                if (items_.empty()) { o += "{}"; break; }
                o += '{';
                for (size_t i = 0; i < items_.size(); ++i) {
                    if (i) o += ',';
                    nl(depth + 1);
                    escape(o, keys_[i]);
                    o += pretty ? ": " : ":";
                    items_[i].write(o, pretty, depth + 1);
                }
                nl(depth); o += '}';
                break;
        }
    }

    struct Parser {
        const char* cur;
        const char* end;
        void skipWs() { while (cur < end && (*cur == ' ' || *cur == '\t' || *cur == '\n' || *cur == '\r')) ++cur; }
        bool lit(const char* w, size_t n) {
            if (static_cast<size_t>(end - cur) < n) return false;
            for (size_t i = 0; i < n; ++i) if (cur[i] != w[i]) return false;
            cur += n; return true;
        }
        static void utf8(std::string& o, unsigned cp) {
            if (cp < 0x80) o += static_cast<char>(cp);
            else if (cp < 0x800) { o += static_cast<char>(0xC0 | (cp >> 6)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
            else if (cp < 0x10000) { o += static_cast<char>(0xE0 | (cp >> 12)); o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
            else { o += static_cast<char>(0xF0 | (cp >> 18)); o += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
        }
        bool hex4(unsigned& v) {
            if (end - cur < 4) return false;
            v = 0;
            for (int i = 0; i < 4; ++i) {
                char c = *cur++; v <<= 4;
                if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
                else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
                else return false;
            }
            return true;
        }
        bool parseString(std::string& out) {
            if (cur >= end || *cur != '"') return false;
            ++cur;
            while (cur < end) {
                unsigned char c = static_cast<unsigned char>(*cur++);
                if (c == '"') return true;
                if (c < 0x20) return false;
                if (c != '\\') { out += static_cast<char>(c); continue; }
                if (cur >= end) return false;
                char e = *cur++;
                switch (e) {
                    case '"': out += '"'; break;   case '\\': out += '\\'; break;  case '/': out += '/'; break;
                    case 'b': out += '\b'; break;  case 'f': out += '\f'; break;   case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;  case 't': out += '\t'; break;
                    case 'u': {
                        unsigned cp;
                        if (!hex4(cp)) return false;
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            unsigned lo;
                            if (!lit("\\u", 2) || !hex4(lo) || lo < 0xDC00 || lo > 0xDFFF) return false;
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        utf8(out, cp);
                        break;
                    }
                    default: return false;
                }
            }
            return false;
        }
        bool parseValue(JsonValue& v, int depth) {
            if (depth > 64 || cur >= end) return false;
            char c = *cur;
            if (c == '{') {
                ++cur; v = object(); skipWs();
                if (cur < end && *cur == '}') { ++cur; return true; }
                while (true) {
                    skipWs();
                    std::string key;
                    if (!parseString(key)) return false;
                    skipWs();
                    if (cur >= end || *cur++ != ':') return false;
                    skipWs();
                    JsonValue child;
                    if (!parseValue(child, depth + 1)) return false;
                    v[key] = std::move(child);
                    skipWs();
                    if (cur >= end) return false;
                    if (*cur == ',') { ++cur; continue; }
                    if (*cur == '}') { ++cur; return true; }
                    return false;
                }
            }
            if (c == '[') {
                ++cur; v = array(); skipWs();
                if (cur < end && *cur == ']') { ++cur; return true; }
                while (true) {
                    skipWs();
                    JsonValue child;
                    if (!parseValue(child, depth + 1)) return false;
                    v.push_back(std::move(child));
                    skipWs();
                    if (cur >= end) return false;
                    if (*cur == ',') { ++cur; continue; }
                    if (*cur == ']') { ++cur; return true; }
                    return false;
                }
            }
            if (c == '"') { std::string s; if (!parseString(s)) return false; v = JsonValue(std::move(s)); return true; }
            if (c == 't') { if (!lit("true", 4)) return false; v = JsonValue(true); return true; }
            if (c == 'f') { if (!lit("false", 5)) return false; v = JsonValue(false); return true; }
            if (c == 'n') { if (!lit("null", 4)) return false; v = JsonValue(); return true; }
            if (c == '-' || (c >= '0' && c <= '9')) {
                const char* start = cur;
                bool isFloat = false;
                if (*cur == '-') ++cur;
                while (cur < end && ((*cur >= '0' && *cur <= '9') || *cur == '.' || *cur == 'e' || *cur == 'E' || *cur == '+' || *cur == '-')) {
                    if (*cur == '.' || *cur == 'e' || *cur == 'E') isFloat = true;
                    ++cur;
                }
                std::string num(start, cur);
                char* e2 = nullptr;
                if (isFloat) { double d = std::strtod(num.c_str(), &e2); if (!e2 || *e2) return false; v = JsonValue(d); }
                else { long long n = std::strtoll(num.c_str(), &e2, 10); if (!e2 || *e2) return false; v = JsonValue(n); }
                return true;
            }
            return false;
        }
    };
};

} // namespace utils
} // namespace uniconnect
