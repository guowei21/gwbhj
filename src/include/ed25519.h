#ifndef ED25519_H
#define ED25519_H

#include <cstdint>
#include <string>
#include <vector>

class Ed25519 {
public:
    static bool verify(const std::string& message, const std::string& signatureBase64, const std::string& publicKeyBase64);
    static bool verify(const uint8_t* msg, size_t msgLen, const uint8_t* sig, const uint8_t* pubKey);
    static std::vector<uint8_t> base64Decode(const std::string& encoded);
    static std::string base64Encode(const uint8_t* data, size_t len);

private:
    static void scalarMultiply(uint8_t* result, const uint8_t* scalar, const uint8_t* point);
    static void scalarMultiplyBase(uint8_t* result, const uint8_t* scalar);
    static bool pointValidate(const uint8_t* point);
    static void geDoubleScalarMultVartime(uint8_t* result, const uint8_t* a, const uint8_t* A, const uint8_t* b);

    static const int64_t FIELD_D[5];
    static const int64_t FIELD_D2[5];
    static const int64_t SQRTM1[5];
};

#endif
