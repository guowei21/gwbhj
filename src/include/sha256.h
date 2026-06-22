#ifndef SHA256_H
#define SHA256_H

#include <cstdint>
#include <string>
#include <vector>

class SHA256 {
public:
    static std::string hash(const std::string& input);
    static std::string hash(const uint8_t* data, size_t len);
    static std::vector<uint8_t> hashBytes(const std::string& input);
    static std::vector<uint8_t> hashBytes(const uint8_t* data, size_t len);

private:
    static constexpr size_t BLOCK_SIZE = 64;
    static constexpr size_t DIGEST_SIZE = 32;

    struct State {
        uint32_t state[8];
        uint64_t bitcount;
        uint8_t buffer[BLOCK_SIZE];
        uint32_t buflen;
    };

    static void init(State* ctx);
    static void update(State* ctx, const uint8_t* data, size_t len);
    static void finalize(State* ctx, uint8_t digest[DIGEST_SIZE]);
    static void transform(State* ctx, const uint8_t block[BLOCK_SIZE]);

    static inline uint32_t rotr(uint32_t x, uint32_t n) {
        return (x >> n) | (x << (32 - n));
    }
    static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
        return (x & y) ^ (~x & z);
    }
    static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
        return (x & y) ^ (x & z) ^ (y & z);
    }
    static inline uint32_t sigma0(uint32_t x) {
        return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
    }
    static inline uint32_t sigma1(uint32_t x) {
        return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
    }
    static inline uint32_t gamma0(uint32_t x) {
        return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
    }
    static inline uint32_t gamma1(uint32_t x) {
        return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
    }
};

#endif
