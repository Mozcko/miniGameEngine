#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mge::detail {

    class GltfJsonValue;

    using GltfJsonArray = std::vector<GltfJsonValue>;
    using GltfJsonObject = std::unordered_map<std::string, GltfJsonValue>;

    // Minimal, dependency-free RFC 8259 JSON value used internally by GltfLoader.
    class GltfJsonValue {
    public:
        using Storage =
            std::variant<std::nullptr_t, bool, double, std::string, GltfJsonArray, GltfJsonObject>;

        GltfJsonValue() : m_value(nullptr) {}
        explicit GltfJsonValue(bool value) : m_value(value) {}
        explicit GltfJsonValue(double value) : m_value(value) {}
        explicit GltfJsonValue(std::string value) : m_value(std::move(value)) {}
        explicit GltfJsonValue(GltfJsonArray value) : m_value(std::move(value)) {}
        explicit GltfJsonValue(GltfJsonObject value) : m_value(std::move(value)) {}

        bool isNull() const { return std::holds_alternative<std::nullptr_t>(m_value); }
        bool isBool() const { return std::holds_alternative<bool>(m_value); }
        bool isNumber() const { return std::holds_alternative<double>(m_value); }
        bool isString() const { return std::holds_alternative<std::string>(m_value); }
        bool isArray() const { return std::holds_alternative<GltfJsonArray>(m_value); }
        bool isObject() const { return std::holds_alternative<GltfJsonObject>(m_value); }

        bool asBool(bool fallback = false) const { return isBool() ? std::get<bool>(m_value) : fallback; }
        double asNumber(double fallback = 0.0) const {
            return isNumber() ? std::get<double>(m_value) : fallback;
        }
        int asInt(int fallback = 0) const {
            return isNumber() ? static_cast<int>(std::get<double>(m_value)) : fallback;
        }
        std::size_t asSize(std::size_t fallback = 0) const {
            return isNumber() && std::get<double>(m_value) >= 0.0
                       ? static_cast<std::size_t>(std::get<double>(m_value))
                       : fallback;
        }
        const std::string& asString() const;
        const GltfJsonArray& asArray() const;
        const GltfJsonObject& asObject() const;

        // Returns nullptr if not an object or key is absent
        const GltfJsonValue* find(const std::string& key) const;

    private:
        Storage m_value;
    };

    // Parses a UTF-8 JSON document. On failure returns std::nullopt and writes a reason to errorOut.
    std::optional<GltfJsonValue> parseGltfJson(std::string_view input, std::string* errorOut = nullptr);
}
