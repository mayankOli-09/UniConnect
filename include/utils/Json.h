#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <type_traits>
#include <cstdint>

namespace uniconnect {
namespace utils {

enum class JsonType {
    NUL,
    BOOLEAN,
    NUMBER,
    STRING,
    ARRAY,
    OBJECT
};

class JsonValue {
public:
    JsonType type{JsonType::NUL};
    bool bool_val{false};
    double num_val{0.0};
    std::string str_val;
    std::vector<JsonValue> arr_val;
    std::map<std::string, JsonValue> obj_val;

    // Constructors
    JsonValue() : type(JsonType::NUL) {}
    JsonValue(std::nullptr_t) : type(JsonType::NUL) {}
    JsonValue(bool b) : type(JsonType::BOOLEAN), bool_val(b) {}

    // Universal constructor for all arithmetic types except bool (int, uint32_t, int64_t, uint64_t, size_t, float, double, etc.)
    template<typename T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<std::decay_t<T>, bool>, int> = 0>
    JsonValue(T n) : type(JsonType::NUMBER), num_val(static_cast<double>(n)) {}

    JsonValue(const char* s) : type(JsonType::STRING), str_val(s ? s : "") {}
    JsonValue(const std::string& s) : type(JsonType::STRING), str_val(s) {}
    JsonValue(std::vector<JsonValue> arr) : type(JsonType::ARRAY), arr_val(std::move(arr)) {}
    JsonValue(std::map<std::string, JsonValue> obj) : type(JsonType::OBJECT), obj_val(std::move(obj)) {}

    // Copy / Move
    JsonValue(const JsonValue&) = default;
    JsonValue(JsonValue&&) noexcept = default;
    JsonValue& operator=(const JsonValue&) = default;
    JsonValue& operator=(JsonValue&&) noexcept = default;

    // Universal assignment operator for all arithmetic types except bool
    template<typename T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<std::decay_t<T>, bool>, int> = 0>
    JsonValue& operator=(T n) {
        type = JsonType::NUMBER;
        num_val = static_cast<double>(n);
        bool_val = false;
        str_val.clear();
        arr_val.clear();
        obj_val.clear();
        return *this;
    }

    JsonValue& operator=(bool b) {
        type = JsonType::BOOLEAN;
        bool_val = b;
        num_val = 0.0;
        str_val.clear();
        arr_val.clear();
        obj_val.clear();
        return *this;
    }

    JsonValue& operator=(const char* s) {
        type = JsonType::STRING;
        str_val = s ? s : "";
        bool_val = false;
        num_val = 0.0;
        arr_val.clear();
        obj_val.clear();
        return *this;
    }

    JsonValue& operator=(const std::string& s) {
        type = JsonType::STRING;
        str_val = s;
        bool_val = false;
        num_val = 0.0;
        arr_val.clear();
        obj_val.clear();
        return *this;
    }

    // Factory methods
    static JsonValue array() {
        JsonValue v;
        v.type = JsonType::ARRAY;
        return v;
    }

    static JsonValue object() {
        JsonValue v;
        v.type = JsonType::OBJECT;
        return v;
    }

    // Accessors
    bool isNull() const { return type == JsonType::NUL; }
    bool isBool() const { return type == JsonType::BOOLEAN; }
    bool isNumber() const { return type == JsonType::NUMBER; }
    bool isString() const { return type == JsonType::STRING; }
    bool isArray() const { return type == JsonType::ARRAY; }
    bool isObject() const { return type == JsonType::OBJECT; }

    bool asBool(bool default_val = false) const { return isBool() ? bool_val : default_val; }
    int asInt(int default_val = 0) const { return isNumber() ? static_cast<int>(num_val) : default_val; }
    int64_t asInt64(int64_t default_val = 0) const { return isNumber() ? static_cast<int64_t>(num_val) : default_val; }
    uint32_t asUint32(uint32_t default_val = 0) const { return isNumber() ? static_cast<uint32_t>(num_val) : default_val; }
    uint64_t asUint64(uint64_t default_val = 0) const { return isNumber() ? static_cast<uint64_t>(num_val) : default_val; }
    double asDouble(double default_val = 0.0) const { return isNumber() ? num_val : default_val; }
    std::string asString(const std::string& default_val = "") const { return isString() ? str_val : default_val; }

    bool contains(const std::string& key) const {
        return isObject() && obj_val.find(key) != obj_val.end();
    }

    JsonValue& operator[](const std::string& key) {
        if (type != JsonType::OBJECT) {
            type = JsonType::OBJECT;
            obj_val.clear();
        }
        return obj_val[key];
    }

    const JsonValue& operator[](const std::string& key) const {
        static const JsonValue null_val;
        if (!isObject()) return null_val;
        auto it = obj_val.find(key);
        return it != obj_val.end() ? it->second : null_val;
    }

    JsonValue& operator[](size_t index) {
        if (type != JsonType::ARRAY) {
            type = JsonType::ARRAY;
            arr_val.clear();
        }
        if (index >= arr_val.size()) {
            arr_val.resize(index + 1);
        }
        return arr_val[index];
    }

    const JsonValue& operator[](size_t index) const {
        static const JsonValue null_val;
        if (!isArray() || index >= arr_val.size()) return null_val;
        return arr_val[index];
    }

    void push_back(const JsonValue& val) {
        if (type != JsonType::ARRAY) {
            type = JsonType::ARRAY;
            arr_val.clear();
        }
        arr_val.push_back(val);
    }

    std::string serialize(bool pretty = false, int indent = 0) const;
    static JsonValue parse(const std::string& json_str);
    static bool tryParse(const std::string& json_str, JsonValue& out_value);
};

} // namespace utils
} // namespace uniconnect
