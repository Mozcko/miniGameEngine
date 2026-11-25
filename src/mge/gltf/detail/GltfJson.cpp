#include <mge/gltf/detail/GltfJson.h>

#include <cstdlib>

namespace mge::detail {

    const std::string& GltfJsonValue::asString() const {
        static const std::string kEmpty;
        return isString() ? std::get<std::string>(m_value) : kEmpty;
    }

    const GltfJsonArray& GltfJsonValue::asArray() const {
        static const GltfJsonArray kEmpty;
        return isArray() ? std::get<GltfJsonArray>(m_value) : kEmpty;
    }

    const GltfJsonObject& GltfJsonValue::asObject() const {
        static const GltfJsonObject kEmpty;
        return isObject() ? std::get<GltfJsonObject>(m_value) : kEmpty;
    }

    const GltfJsonValue* GltfJsonValue::find(const std::string& key) const {
        if (!isObject()) return nullptr;
        const GltfJsonObject& obj = std::get<GltfJsonObject>(m_value);
        const auto it = obj.find(key);
        return it == obj.end() ? nullptr : &it->second;
    }

    namespace {
        constexpr int kMaxJsonDepth = 128;

        void appendUtf8(std::string& out, std::uint32_t codepoint) {
            if (codepoint <= 0x7Fu) {
                out.push_back(static_cast<char>(codepoint));
            } else if (codepoint <= 0x7FFu) {
                out.push_back(static_cast<char>(0xC0u | (codepoint >> 6)));
                out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
            } else if (codepoint <= 0xFFFFu) {
                out.push_back(static_cast<char>(0xE0u | (codepoint >> 12)));
                out.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
            } else if (codepoint <= 0x10FFFFu) {
                out.push_back(static_cast<char>(0xF0u | (codepoint >> 18)));
                out.push_back(static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
            }
        }

        class JsonParser {
        public:
            explicit JsonParser(std::string_view input) : m_input(input) {}

            std::optional<GltfJsonValue> parse() {
                skipWhitespace();
                auto value = parseValue();
                if (!value) return std::nullopt;
                skipWhitespace();
                if (m_pos != m_input.size()) {
                    fail("unexpected trailing characters");
                    return std::nullopt;
                }
                return value;
            }

            const std::string& error() const { return m_error; }

        private:
            std::string_view m_input;
            std::size_t m_pos = 0;
            int m_depth = 0;
            std::string m_error;

            void fail(const std::string& msg) {
                if (m_error.empty()) m_error = msg;
            }

            bool eof() const { return m_pos >= m_input.size(); }

            char peek() const { return eof() ? '\0' : m_input[m_pos]; }

            void skipWhitespace() {
                while (!eof()) {
                    const char c = m_input[m_pos];
                    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                        m_pos++;
                    } else {
                        break;
                    }
                }
            }

            std::optional<GltfJsonValue> parseValue() {
                skipWhitespace();
                if (eof()) {
                    fail("unexpected end of JSON input");
                    return std::nullopt;
                }
                const char c = peek();
                if (c == '{') return parseObject();
                if (c == '[') return parseArray();
                if (c == '"') {
                    auto str = parseString();
                    if (!str) return std::nullopt;
                    return GltfJsonValue(std::move(*str));
                }
                if (c == 't') return parseLiteral("true", GltfJsonValue(true));
                if (c == 'f') return parseLiteral("false", GltfJsonValue(false));
                if (c == 'n') return parseLiteral("null", GltfJsonValue());
                if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();

                fail("unexpected token in JSON");
                return std::nullopt;
            }

            std::optional<GltfJsonValue> parseLiteral(std::string_view literal, GltfJsonValue result) {
                if (m_input.substr(m_pos, literal.size()) != literal) {
                    fail("invalid JSON literal");
                    return std::nullopt;
                }
                m_pos += literal.size();
                return result;
            }

            bool parseHex4(std::uint32_t& out) {
                if (m_pos + 4 > m_input.size()) {
                    fail("incomplete unicode escape");
                    return false;
                }
                std::uint32_t value = 0;
                for (int i = 0; i < 4; i++) {
                    const char c = m_input[m_pos++];
                    value <<= 4;
                    if (c >= '0' && c <= '9') {
                        value |= static_cast<std::uint32_t>(c - '0');
                    } else if (c >= 'a' && c <= 'f') {
                        value |= static_cast<std::uint32_t>(c - 'a' + 10);
                    } else if (c >= 'A' && c <= 'F') {
                        value |= static_cast<std::uint32_t>(c - 'A' + 10);
                    } else {
                        fail("invalid hex digit in unicode escape");
                        return false;
                    }
                }
                out = value;
                return true;
            }

            std::optional<std::string> parseString() {
                if (eof() || m_input[m_pos] != '"') {
                    fail("expected '\"'");
                    return std::nullopt;
                }
                m_pos++;
                std::string result;
                while (!eof()) {
                    const char c = m_input[m_pos++];
                    if (c == '"') return result;
                    if (static_cast<unsigned char>(c) < 0x20u) {
                        fail("unescaped control character in string");
                        return std::nullopt;
                    }
                    if (c == '\\') {
                        if (eof()) {
                            fail("unterminated escape sequence");
                            return std::nullopt;
                        }
                        const char esc = m_input[m_pos++];
                        switch (esc) {
                        case '"': result.push_back('"'); break;
                        case '\\': result.push_back('\\'); break;
                        case '/': result.push_back('/'); break;
                        case 'b': result.push_back('\b'); break;
                        case 'f': result.push_back('\f'); break;
                        case 'n': result.push_back('\n'); break;
                        case 'r': result.push_back('\r'); break;
                        case 't': result.push_back('\t'); break;
                        case 'u': {
                            std::uint32_t cp = 0;
                            if (!parseHex4(cp)) return std::nullopt;
                            if (cp >= 0xD800u && cp <= 0xDBFFu) {
                                if (m_pos + 2 <= m_input.size() && m_input[m_pos] == '\\' &&
                                    m_input[m_pos + 1] == 'u') {
                                    m_pos += 2;
                                    std::uint32_t low = 0;
                                    if (!parseHex4(low)) return std::nullopt;
                                    if (low >= 0xDC00u && low <= 0xDFFFu) {
                                        cp = 0x10000u + (((cp - 0xD800u) << 10) | (low - 0xDC00u));
                                    }
                                }
                            }
                            appendUtf8(result, cp);
                            break;
                        }
                        default: fail("invalid escape sequence"); return std::nullopt;
                        }
                    } else {
                        result.push_back(c);
                    }
                }
                fail("unterminated string");
                return std::nullopt;
            }

            std::optional<GltfJsonValue> parseNumber() {
                const std::size_t start = m_pos;
                if (peek() == '-') m_pos++;
                if (eof()) {
                    fail("invalid number");
                    return std::nullopt;
                }
                if (peek() == '0') {
                    m_pos++;
                } else if (peek() >= '1' && peek() <= '9') {
                    while (!eof() && peek() >= '0' && peek() <= '9') m_pos++;
                } else {
                    fail("invalid number");
                    return std::nullopt;
                }
                if (!eof() && peek() == '.') {
                    m_pos++;
                    if (eof() || peek() < '0' || peek() > '9') {
                        fail("invalid fractional number");
                        return std::nullopt;
                    }
                    while (!eof() && peek() >= '0' && peek() <= '9') m_pos++;
                }
                if (!eof() && (peek() == 'e' || peek() == 'E')) {
                    m_pos++;
                    if (!eof() && (peek() == '+' || peek() == '-')) m_pos++;
                    if (eof() || peek() < '0' || peek() > '9') {
                        fail("invalid exponent in number");
                        return std::nullopt;
                    }
                    while (!eof() && peek() >= '0' && peek() <= '9') m_pos++;
                }
                const std::string token(m_input.substr(start, m_pos - start));
                char* endPtr = nullptr;
                const double value = std::strtod(token.c_str(), &endPtr);
                if (endPtr != token.c_str() + token.size()) {
                    fail("failed to parse number");
                    return std::nullopt;
                }
                return GltfJsonValue(value);
            }

            std::optional<GltfJsonValue> parseArray() {
                if (m_depth >= kMaxJsonDepth) {
                    fail("JSON nesting depth exceeded");
                    return std::nullopt;
                }
                m_depth++;
                m_pos++; // '['
                GltfJsonArray array;
                skipWhitespace();
                if (!eof() && peek() == ']') {
                    m_pos++;
                    m_depth--;
                    return GltfJsonValue(std::move(array));
                }
                while (true) {
                    auto elem = parseValue();
                    if (!elem) return std::nullopt;
                    array.push_back(std::move(*elem));
                    skipWhitespace();
                    if (eof()) {
                        fail("unterminated array");
                        return std::nullopt;
                    }
                    if (peek() == ']') {
                        m_pos++;
                        m_depth--;
                        return GltfJsonValue(std::move(array));
                    }
                    if (peek() != ',') {
                        fail("expected ',' or ']' in array");
                        return std::nullopt;
                    }
                    m_pos++;
                }
            }

            std::optional<GltfJsonValue> parseObject() {
                if (m_depth >= kMaxJsonDepth) {
                    fail("JSON nesting depth exceeded");
                    return std::nullopt;
                }
                m_depth++;
                m_pos++; // '{'
                GltfJsonObject object;
                skipWhitespace();
                if (!eof() && peek() == '}') {
                    m_pos++;
                    m_depth--;
                    return GltfJsonValue(std::move(object));
                }
                while (true) {
                    skipWhitespace();
                    auto key = parseString();
                    if (!key) return std::nullopt;
                    skipWhitespace();
                    if (eof() || peek() != ':') {
                        fail("expected ':' after object key");
                        return std::nullopt;
                    }
                    m_pos++;
                    auto val = parseValue();
                    if (!val) return std::nullopt;
                    object.insert_or_assign(std::move(*key), std::move(*val));
                    skipWhitespace();
                    if (eof()) {
                        fail("unterminated object");
                        return std::nullopt;
                    }
                    if (peek() == '}') {
                        m_pos++;
                        m_depth--;
                        return GltfJsonValue(std::move(object));
                    }
                    if (peek() != ',') {
                        fail("expected ',' or '}' in object");
                        return std::nullopt;
                    }
                    m_pos++;
                }
            }
        };
    }

    std::optional<GltfJsonValue> parseGltfJson(std::string_view input, std::string* errorOut) {
        JsonParser parser(input);
        auto result = parser.parse();
        if (!result && errorOut) *errorOut = parser.error();
        return result;
    }
}
