#ifndef MINI_JSON_H
#define MINI_JSON_H

#include <string>
#include <unordered_map>
#include <vector>

class MiniJson {
public:
    enum Type { NUL, BOOL, NUMBER, STRING, OBJECT, ARRAY };

    MiniJson() : type_(NUL) {}
    MiniJson(bool v) : type_(BOOL), boolVal_(v) {}
    MiniJson(double v) : type_(NUMBER), numVal_(v) {}
    MiniJson(const std::string& v) : type_(STRING), strVal_(v) {}
    MiniJson(const char* v) : type_(STRING), strVal_(v) {}

    static MiniJson parse(const std::string& json);
    static MiniJson parse(const std::string& json, size_t& pos);

    Type type() const { return type_; }
    bool isNull() const { return type_ == NUL; }
    bool isBool() const { return type_ == BOOL; }
    bool isNumber() const { return type_ == NUMBER; }
    bool isString() const { return type_ == STRING; }
    bool isObject() const { return type_ == OBJECT; }
    bool isArray() const { return type_ == ARRAY; }

    bool asBool() const { return boolVal_; }
    double asNumber() const { return numVal_; }
    long asLong() const { return (long)numVal_; }
    const std::string& asString() const { return strVal_; }

    const MiniJson& operator[](const std::string& key) const;
    const MiniJson& operator[](size_t index) const;
    size_t size() const;
    bool has(const std::string& key) const;
    const std::unordered_map<std::string, MiniJson>& asObject() const { return objVal_; }
    const std::vector<MiniJson>& asArray() const { return arrVal_; }

private:
    Type type_;
    bool boolVal_ = false;
    double numVal_ = 0;
    std::string strVal_;
    std::unordered_map<std::string, MiniJson> objVal_;
    std::vector<MiniJson> arrVal_;

    static void skipWhitespace(const std::string& json, size_t& pos);
    static std::string parseString(const std::string& json, size_t& pos);
    static MiniJson parseValue(const std::string& json, size_t& pos);
    static MiniJson parseObject(const std::string& json, size_t& pos);
    static MiniJson parseArray(const std::string& json, size_t& pos);
    static MiniJson parseNumber(const std::string& json, size_t& pos);
    static MiniJson parseLiteral(const std::string& json, size_t& pos);
    static uint16_t parseHex4(const std::string& json, size_t& pos);
    static std::string unicodeToUtf8(uint32_t codepoint);
};

#endif
