#include "mini_json.h"
#include <cstdlib>

static const MiniJson JSON_NULL;

void MiniJson::skipWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
        pos++;
}

uint16_t MiniJson::parseHex4(const std::string& json, size_t& pos) {
    uint16_t code = 0;
    for (int i = 0; i < 4 && pos < json.size(); i++) {
        char c = json[pos++];
        code <<= 4;
        if (c >= '0' && c <= '9') code |= (c - '0');
        else if (c >= 'a' && c <= 'f') code |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') code |= (c - 'A' + 10);
    }
    return code;
}

std::string MiniJson::unicodeToUtf8(uint32_t cp) {
    std::string result;
    if (cp <= 0x7F) {
        result += (char)cp;
    } else if (cp <= 0x7FF) {
        result += (char)(0xC0 | (cp >> 6));
        result += (char)(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        result += (char)(0xE0 | (cp >> 12));
        result += (char)(0x80 | ((cp >> 6) & 0x3F));
        result += (char)(0x80 | (cp & 0x3F));
    } else {
        result += (char)(0xF0 | (cp >> 18));
        result += (char)(0x80 | ((cp >> 12) & 0x3F));
        result += (char)(0x80 | ((cp >> 6) & 0x3F));
        result += (char)(0x80 | (cp & 0x3F));
    }
    return result;
}

std::string MiniJson::parseString(const std::string& json, size_t& pos) {
    std::string result;
    pos++;
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '"') return result;
        if (c == '\\') {
            if (pos >= json.size()) break;
            char esc = json[pos++];
            switch (esc) {
                case '"':  result += '"'; break;
                case '\\': result += '\\'; break;
                case '/':  result += '/'; break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case 'u': {
                    uint32_t cp = parseHex4(json, pos);
                    if (cp >= 0xD800 && cp <= 0xDBFF && pos < json.size() && json[pos] == '\\') {
                        pos++;
                        if (pos < json.size() && json[pos] == 'u') {
                            pos++;
                            uint32_t cp2 = parseHex4(json, pos);
                            if (cp2 >= 0xDC00 && cp2 <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (cp2 - 0xDC00);
                            }
                        }
                    }
                    result += unicodeToUtf8(cp);
                    break;
                }
                default: result += esc; break;
            }
        } else {
            result += c;
        }
    }
    return result;
}

MiniJson MiniJson::parseNumber(const std::string& json, size_t& pos) {
    size_t start = pos;
    if (json[pos] == '-') pos++;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') pos++;
    bool isFloat = false;
    if (pos < json.size() && json[pos] == '.') {
        isFloat = true;
        pos++;
        while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') pos++;
    }
    if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
        isFloat = true;
        pos++;
        if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) pos++;
        while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') pos++;
    }
    std::string numStr = json.substr(start, pos - start);
    double val = strtod(numStr.c_str(), nullptr);
    return MiniJson(val);
}

MiniJson MiniJson::parseLiteral(const std::string& json, size_t& pos) {
    if (json.compare(pos, 4, "true") == 0) { pos += 4; return MiniJson(true); }
    if (json.compare(pos, 5, "false") == 0) { pos += 5; return MiniJson(false); }
    if (json.compare(pos, 4, "null") == 0) { pos += 4; return MiniJson(); }
    return MiniJson();
}

MiniJson MiniJson::parseArray(const std::string& json, size_t& pos) {
    MiniJson arr;
    arr.type_ = ARRAY;
    pos++;
    skipWhitespace(json, pos);
    if (pos < json.size() && json[pos] == ']') { pos++; return arr; }
    while (pos < json.size()) {
        skipWhitespace(json, pos);
        arr.arrVal_.push_back(parseValue(json, pos));
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ']') { pos++; return arr; }
        if (pos < json.size() && json[pos] == ',') pos++;
    }
    return arr;
}

MiniJson MiniJson::parseObject(const std::string& json, size_t& pos) {
    MiniJson obj;
    obj.type_ = OBJECT;
    pos++;
    skipWhitespace(json, pos);
    if (pos < json.size() && json[pos] == '}') { pos++; return obj; }
    while (pos < json.size()) {
        skipWhitespace(json, pos);
        if (json[pos] != '"') break;
        std::string key = parseString(json, pos);
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ':') pos++;
        skipWhitespace(json, pos);
        obj.objVal_[key] = parseValue(json, pos);
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '}') { pos++; return obj; }
        if (pos < json.size() && json[pos] == ',') pos++;
    }
    return obj;
}

MiniJson MiniJson::parseValue(const std::string& json, size_t& pos) {
    skipWhitespace(json, pos);
    if (pos >= json.size()) return MiniJson();
    char c = json[pos];
    if (c == '"') return MiniJson(parseString(json, pos));
    if (c == '{') return parseObject(json, pos);
    if (c == '[') return parseArray(json, pos);
    if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(json, pos);
    if (c == 't' || c == 'f' || c == 'n') return parseLiteral(json, pos);
    return MiniJson();
}

MiniJson MiniJson::parse(const std::string& json, size_t& pos) {
    return parseValue(json, pos);
}

MiniJson MiniJson::parse(const std::string& json) {
    size_t pos = 0;
    return parseValue(json, pos);
}

const MiniJson& MiniJson::operator[](const std::string& key) const {
    auto it = objVal_.find(key);
    if (it != objVal_.end()) return it->second;
    return JSON_NULL;
}

const MiniJson& MiniJson::operator[](size_t index) const {
    if (index < arrVal_.size()) return arrVal_[index];
    return JSON_NULL;
}

size_t MiniJson::size() const {
    if (type_ == ARRAY) return arrVal_.size();
    if (type_ == OBJECT) return objVal_.size();
    return 0;
}

bool MiniJson::has(const std::string& key) const {
    return objVal_.find(key) != objVal_.end();
}
