#pragma once
#include <map>
#include <string>
#include <string_view>
#include <cctype>

namespace cof {
// Stock item definitions: a named block containing data "value". Missing
// optional properties are normal; querying one must not invoke an error parser.
class ItemScript {
    std::string_view bytes;
    size_t position = 0;
    bool good = true;
    std::string token() {
        while (position < bytes.size()) {
            if (std::isspace(static_cast<unsigned char>(bytes[position]))) { ++position; continue; }
            if (bytes[position] == '/' && position + 1 < bytes.size() && bytes[position + 1] == '/') {
                while (position < bytes.size() && bytes[position] != '\n') ++position;
                continue;
            }
            break;
        }
        if (position == bytes.size()) return {};
        char c = bytes[position++];
        if (c == '{' || c == '}') return std::string(1, c);
        std::string result;
        if (c == '"') {
            while (position < bytes.size() && bytes[position] != '"') result += bytes[position++];
            if (position == bytes.size()) good = false; else ++position;
        } else {
            result += c;
            while (position < bytes.size() && !std::isspace(static_cast<unsigned char>(bytes[position])) && bytes[position] != '{' && bytes[position] != '}') result += bytes[position++];
        }
        return result;
    }
public:
    explicit ItemScript(std::string_view data): bytes(data) {}
    bool parse(std::map<std::string, std::string>& values) {
        while (position < bytes.size() && good) {
            auto name = token();
            if (name.empty() && position == bytes.size()) break;
            if (name.empty() || token() != "{") return false;
            int depth = 1;
            while (depth && good) {
                auto property = token();
                if (property.empty() && position == bytes.size()) return false;
                if (property == "{") ++depth;
                else if (property == "}") --depth;
                else if (depth == 1 && property == "data") values.emplace(name, token());
            }
        }
        return good;
    }
};
}
