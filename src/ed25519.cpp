#include "ed25519.h"
#include "sha256.h"
#include <cstring>

namespace {

static const char B64_TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

typedef int64_t fe[5];

static const int64_t D[5] = {-10913610, 13857413, -15372611, 6949391, 114729};
static const int64_t D2[5] = {-21827219, -5839606, -30745221, 13898782, 229458};
static const int64_t SQRTM1[5] = {-32595792, -7943725, 9377950, 3500415, 123894719};

static void feCopy(fe h, const fe f) { for (int i = 0; i < 5; i++) h[i] = f[i]; }
static void feNeg(fe h, const fe f) { for (int i = 0; i < 5; i++) h[i] = -f[i]; }
static void feAdd(fe h, const fe f, const fe g) { for (int i = 0; i < 5; i++) h[i] = f[i] + g[i]; }
static void feSub(fe h, const fe f, const fe g) { for (int i = 0; i < 5; i++) h[i] = f[i] - g[i]; }

static void feReduce(fe h) {
    for (int i = 0; i < 5; i++) {
        h[i] += (1LL << 47); h[i] &= 0x1FFFFFFFFFFFFLL;
        h[i] -= (1LL << 47);
    }
    int64_t c;
    c = (h[0] + (1LL << 47)) >> 51; h[0] &= 0x1FFFFFFFFFFFFLL; h[1] += c;
    c = (h[1] + (1LL << 47)) >> 51; h[1] &= 0x1FFFFFFFFFFFFLL; h[2] += c;
    c = (h[2] + (1LL << 47)) >> 51; h[2] &= 0x1FFFFFFFFFFFFLL; h[3] += c;
    c = (h[3] + (1LL << 47)) >> 51; h[3] &= 0x1FFFFFFFFFFFFLL; h[4] += c;
    c = (h[4] + (1LL << 47)) >> 51; h[4] &= 0x1FFFFFFFFFFFFLL; h[0] += c * 19;
    c = (h[0] + (1LL << 47)) >> 51; h[0] &= 0x1FFFFFFFFFFFFLL; h[1] += c;
}

static void feMul(fe h, const fe f, const fe g) {
    int64_t f0=f[0],f1=f[1],f2=f[2],f3=f[3],f4=f[4];
    int64_t g0=g[0],g1=g[1],g2=g[2],g3=g[3],g4=g[4];
    int64_t g1_19=19*g1, g2_19=19*g2, g3_19=19*g3, g4_19=19*g4;
    h[0] = f0*g0 + f1*g4_19 + f2*g3_19 + f3*g2_19 + f4*g1_19;
    h[1] = f0*g1 + f1*g0   + f2*g4_19 + f3*g3_19 + f4*g2_19;
    h[2] = f0*g2 + f1*g1   + f2*g0   + f3*g4_19 + f4*g3_19;
    h[3] = f0*g3 + f1*g2   + f2*g1   + f3*g0   + f4*g4_19;
    h[4] = f0*g4 + f1*g3   + f2*g2   + f3*g1   + f4*g0;
    feReduce(h);
}

static void feSq(fe h, const fe f) {
    int64_t f0=f[0],f1=f[1],f2=f[2],f3=f[3],f4=f[4];
    int64_t f0_2=2*f0, f1_2=2*f1, f2_2=2*f2;
    h[0] = f0*f0   + 38*f1*f4 + 38*f2*f3;
    h[1] = f0_2*f1 + 38*f2*f4 + 19*f3*f3;
    h[2] = f0_2*f2 + f1_2*f1  + 19*f4*f4;
    h[3] = f0_2*f3 + f1_2*f2  + f2*f4*38;
    h[4] = f0_2*f4 + f1_2*f3  + f2_2*f2;
    feReduce(h);
}

static void feSq2(fe h, const fe f) { feSq(h,f); for(int i=0;i<5;i++) h[i]*=2; feReduce(h); }

static void feInvert(fe h, const fe f) {
    fe t0,t1,t2,t3;
    feSq(t0,f); feMul(t1,t0,f);
    feSq(t0,t1); feMul(t1,t0,f);
    feSq(t0,t1); feSq(t2,t0); feMul(t0,t2,t1);
    feSq(t2,t0); for(int i=1;i<5;i++) feSq(t2,t2); feMul(t2,t2,t0);
    feSq(t3,t2); for(int i=1;i<10;i++) feSq(t3,t3); feMul(t2,t3,t2);
    feSq(t3,t2); for(int i=1;i<25;i++) feSq(t3,t3); feMul(t2,t3,t2);
    feSq(t3,t2); for(int i=1;i<50;i++) feSq(t3,t3); feMul(t2,t3,t2);
    feSq(t3,t2); for(int i=1;i<125;i++) feSq(t3,t3); feMul(t2,t3,t2);
    feSq(t2,t2); feSq(t2,t2); feMul(h,t2,t1);
}

static void fePow22523(fe h, const fe f) {
    fe t0,t1,t2;
    feSq(t0,f); for(int i=1;i<5;i++) feSq(t0,t0); feMul(t1,t0,f);
    feSq(t0,t1); for(int i=1;i<10;i++) feSq(t0,t0); feMul(t0,t0,t1);
    feSq(t2,t0); for(int i=1;i<20;i++) feSq(t2,t2); feMul(t0,t2,t0);
    feSq(t0,t0); for(int i=1;i<10;i++) feSq(t0,t0); feMul(t0,t0,t1);
    feSq(t0,t0); for(int i=1;i<50;i++) feSq(t0,t0); feMul(t0,t0,t1);
    feSq(t0,t0); for(int i=1;i<100;i++) feSq(t0,t0); feMul(t0,t0,t1);
    feSq(t0,t0); for(int i=1;i<50;i++) feSq(t0,t0); feMul(t0,t0,t1);
    feSq(t0,t0); for(int i=1;i<5;i++) feSq(t0,t0); feMul(h,t0,f);
}

static int feIsNegative(const fe f) {
    fe h; feCopy(h,f); feReduce(h);
    return (int)(h[0] & 1);
}

static void feFromBytes(fe h, const uint8_t s[32]) {
    uint64_t v = (uint64_t)s[31] & 0x7F;
    for(int i=30;i>=0;i--) v = (v<<8) | (uint64_t)s[i];
    h[0] = (int64_t)(v & 0x1FFFFFFFFFFFFULL);
    h[1] = (int64_t)((v >> 51) & 0x1FFFFFFFFFFFFULL);
    h[2] = 0; h[3] = 0; h[4] = 0;
}

static void feToBytes(uint8_t s[32], const fe h) {
    fe f; feCopy(f,h); feReduce(f);
    int64_t c;
    c = (f[0]+(1LL<<47))>>51; f[0] &= 0x1FFFFFFFFFFFFLL; f[1] += c;
    c = (f[1]+(1LL<<47))>>51; f[1] &= 0x1FFFFFFFFFFFFLL; f[2] += c;
    c = (f[2]+(1LL<<47))>>51; f[2] &= 0x1FFFFFFFFFFFFLL; f[3] += c;
    c = (f[3]+(1LL<<47))>>51; f[3] &= 0x1FFFFFFFFFFFFLL; f[4] += c;
    c = (f[4]+(1LL<<47))>>51; f[4] &= 0x1FFFFFFFFFFFFLL; f[0] += c*19;
    c = (f[0]+(1LL<<47))>>51; f[0] &= 0x1FFFFFFFFFFFFLL; f[1] += c;
    uint64_t m0=(uint64_t)f[0],m1=(uint64_t)f[1],m2=(uint64_t)f[2],m3=(uint64_t)f[3],m4=(uint64_t)f[4];
    s[0]=(uint8_t)m0;      s[1]=(uint8_t)(m0>>8);   s[2]=(uint8_t)(m0>>16);
    s[3]=(uint8_t)((m0>>24)|(m1<<4));
    s[4]=(uint8_t)(m1>>4); s[5]=(uint8_t)(m1>>12);  s[6]=(uint8_t)(m1>>20);
    s[7]=(uint8_t)((m1>>28)|(m2<<2));
    s[8]=(uint8_t)(m2>>6); s[9]=(uint8_t)(m2>>14);  s[10]=(uint8_t)(m2>>22);
    s[11]=(uint8_t)((m2>>30)|(m3<<1));
    s[12]=(uint8_t)(m3>>7);s[13]=(uint8_t)(m3>>15);
    s[14]=(uint8_t)((m3>>23)|(m4<<3));
    s[15]=(uint8_t)(m4>>5);s[16]=(uint8_t)(m4>>13); s[17]=(uint8_t)(m4>>21);
    s[18]=(uint8_t)(m4>>29);
    for(int i=19;i<32;i++) s[i]=0;
}

static const uint8_t BASEPOINT[32] = {
    9,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

struct geP3 { fe X, Y, Z, T; };

static void geP3ToBytes(uint8_t s[32], const geP3* h) {
    fe recip,x,y;
    feInvert(recip,h->Z);
    feMul(x,h->X,recip);
    feMul(y,h->Y,recip);
    feToBytes(s,y);
    s[31] ^= (uint8_t)(feIsNegative(x) << 7);
}

static void geDouble(geP3* r, const geP3* p) {
    fe t0,t1,t2,t3;
    feSq(t0,p->Y); feMul(t3,p->X,p->Y);
    feSq(t1,p->Z); feAdd(t2,t0,t1);
    feSub(t3,t1,t0); feSub(t1,p->Y,p->X);
    feAdd(t0,p->Y,p->X); feMul(t0,t0,t1);
    feMul(r->T,t2,t3); feSub(r->X,t0,t3);
    feSq(r->Z,t2); feSq(r->Y,t3);
    feAdd(t3,t0,t3); feSq2(t0,r->X);
    feMul(r->X,r->X,r->Z); feMul(r->Z,t3,r->Z);
    feMul(r->Y,t2,r->Y);
}

static void geAdd(geP3* r, const geP3* p, const geP3* q) {
    fe a,b,c,d,e,f,g,h;
    feSub(a,p->Y,p->X); feSub(b,q->Y,q->X); feMul(a,a,b);
    feAdd(b,p->Y,p->X); feAdd(h,q->Y,q->X); feMul(b,b,h);
    feMul(c,p->T,q->T); feMul(c,c,D2);
    feMul(d,p->Z,q->Z); feAdd(d,d,d);
    feSub(e,b,a); feSub(f,d,c);
    feAdd(g,d,c); feAdd(h,b,a);
    feMul(r->X,e,f); feSub(c,g,h);
    feMul(r->Y,h,g); feMul(r->Z,f,g);
    feMul(r->T,e,c);
}

static void geScalarMultBase(geP3* r, const uint8_t scalar[32]) {
    geP3 p;
    feFromBytes(p.Y, BASEPOINT);
    fe t; fe1(t); feCopy(p.Z, t);
    feSq(p.X, p.Y); feSub(p.X, p.X, p.Z); feInvert(p.X, p.X); feMul(p.X, p.X, p.Y);
    feMul(p.T, p.X, p.Y);

    uint8_t s[32]; memcpy(s, scalar, 32);
    s[0] &= 248; s[31] &= 127; s[31] |= 64;

    fe x1,y1,z1,t1;
    fe0(x1); fe1(y1); fe1(z1); fe0(t1);
    bool init = false;

    for (int i = 254; i >= 0; i--) {
        if (init) {
            fe nx,ny,nz,nt;
            feSq(nx,y1); feMul(nz,z1,y1);
            feSq(ny,x1); feSub(nt,nx,ny);
            feAdd(nx,nx,ny); feMul(ny,nt,nz);
            feSq(nz,z1); feMul(nz,nz,D2);
            feSub(nz,nx,nz); feAdd(nx,nx,nz);
            feCopy(x1,nx); feCopy(y1,ny); feCopy(z1,nz); feCopy(t1,nt);
        }
        int bit = (s[i/8] >> (i%8)) & 1;
        if (bit) {
            if (!init) {
                feCopy(x1,p.X); feCopy(y1,p.Y); feCopy(z1,p.Z); feCopy(t1,p.T);
                init = true;
            } else {
                geP3 cur; feCopy(cur.X,x1); feCopy(cur.Y,y1); feCopy(cur.Z,z1); feCopy(cur.T,t1);
                geP3 res; geAdd(&res, &cur, &p);
                feCopy(x1,res.X); feCopy(y1,res.Y); feCopy(z1,res.Z); feCopy(t1,res.T);
            }
        }
    }
    if (!init) { fe0(r->X); fe1(r->Y); fe1(r->Z); fe0(r->T); return; }
    feCopy(r->X,x1); feCopy(r->Y,y1); feCopy(r->Z,z1); feCopy(r->T,t1);
}

static void geScalarMult(geP3* r, const uint8_t scalar[32], const geP3* p) {
    uint8_t s[32]; memcpy(s,scalar,32);
    s[0] &= 248; s[31] &= 127; s[31] |= 64;

    fe x1,y1,z1,t1;
    fe0(x1); fe1(y1); fe1(z1); fe0(t1);
    bool init = false;

    for (int i = 254; i >= 0; i--) {
        if (init) {
            feSq(x1,y1); feMul(z1,z1,y1);
            feSq(y1,x1); feSub(t1,x1,y1);
            feAdd(x1,x1,y1); feMul(y1,t1,z1);
            feSq(z1,z1); feMul(z1,z1,D2);
            feSub(z1,x1,z1); feAdd(x1,x1,z1);
        }
        int bit = (s[i/8] >> (i%8)) & 1;
        if (bit) {
            if (!init) {
                feCopy(x1,p->X); feCopy(y1,p->Y); feCopy(z1,p->Z); feCopy(t1,p->T);
                init = true;
            } else {
                geP3 cur; feCopy(cur.X,x1); feCopy(cur.Y,y1); feCopy(cur.Z,z1); feCopy(cur.T,t1);
                geP3 res; geAdd(&res, &cur, p);
                feCopy(x1,res.X); feCopy(y1,res.Y); feCopy(z1,res.Z); feCopy(t1,res.T);
            }
        }
    }
    if (!init) { fe0(r->X); fe1(r->Y); fe1(r->Z); fe0(r->T); return; }
    feCopy(r->X,x1); feCopy(r->Y,y1); feCopy(r->Z,z1); feCopy(r->T,t1);
}

static bool geFromBytesNegateVartime(geP3* r, const uint8_t s[32]) {
    fe u,v,vxx,check;
    feFromBytes(r->Y, s);
    fe t; fe1(t); feCopy(r->Z, t);
    feSq(u, r->Y);
    feMul(v, u, D);
    feSub(u, u, r->Z);
    feAdd(v, v, r->Z);

    fe v3;
    feMul(v3, v, v); feMul(v3, v3, v);
    feMul(r->X, v3, u);
    fePow22523(r->X, r->X);
    feMul(r->X, r->X, v3);
    feMul(r->X, r->X, u);

    feSq(vxx, r->X); feMul(vxx, vxx, v); feSub(check, vxx, u);
    if (feIsNegative(check) != 0) feMul(r->X, r->X, SQRTM1);
    if (feIsNegative(r->X) == (s[31] >> 7)) feNeg(r->X, r->X);
    feMul(r->T, r->X, r->Y);
    return true;
}

static void fe1(fe h) { h[0]=1; h[1]=0; h[2]=0; h[3]=0; h[4]=0; }
static void fe0(fe h) { h[0]=0; h[1]=0; h[2]=0; h[3]=0; h[4]=0; }

}

std::vector<uint8_t> Ed25519::base64Decode(const std::string& encoded) {
    std::vector<uint8_t> result;
    result.reserve(encoded.size() * 3 / 4);
    int val = 0, bits = 0;
    for (char c : encoded) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        int idx = -1;
        if (c >= 'A' && c <= 'Z') idx = c - 'A';
        else if (c >= 'a' && c <= 'z') idx = c - 'a' + 26;
        else if (c >= '0' && c <= '9') idx = c - '0' + 52;
        else if (c == '+') idx = 62;
        else if (c == '/') idx = 63;
        else continue;
        val = (val << 6) | idx;
        bits += 6;
        if (bits >= 8) { bits -= 8; result.push_back(static_cast<uint8_t>((val >> bits) & 0xFF)); }
    }
    return result;
}

std::string Ed25519::base64Encode(const uint8_t* data, size_t len) {
    std::string result;
    result.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= data[i + 2];
        result.push_back(B64_TABLE[(n >> 18) & 0x3F]);
        result.push_back(B64_TABLE[(n >> 12) & 0x3F]);
        result.push_back((i + 1 < len) ? B64_TABLE[(n >> 6) & 0x3F] : '=');
        result.push_back((i + 2 < len) ? B64_TABLE[n & 0x3F] : '=');
    }
    return result;
}

bool Ed25519::verify(const std::string& message, const std::string& signatureBase64, const std::string& publicKeyBase64) {
    auto sigBytes = base64Decode(signatureBase64);
    auto pubBytes = base64Decode(publicKeyBase64);
    if (sigBytes.size() != 64 || pubBytes.size() != 32) return false;
    return verify(reinterpret_cast<const uint8_t*>(message.data()), message.size(), sigBytes.data(), pubBytes.data());
}

bool Ed25519::verify(const uint8_t* msg, size_t msgLen, const uint8_t* sig, const uint8_t* pubKey) {
    if ((sig[63] & 224) != 0) return false;

    uint8_t pk[32]; memcpy(pk, pubKey, 32);
    pk[31] &= 127;

    geP3 A;
    if (!geFromBytesNegateVartime(&A, pk)) return false;

    std::vector<uint8_t> hInput;
    hInput.reserve(32 + 32 + msgLen);
    hInput.insert(hInput.end(), sig, sig + 32);
    hInput.insert(hInput.end(), pubKey, pubKey + 32);
    hInput.insert(hInput.end(), msg, msg + msgLen);

    auto hDigest = SHA256::hashBytes(hInput.data(), hInput.size());

    uint8_t h[32]; memcpy(h, hDigest.data(), 32);
    h[0] &= 248; h[31] &= 127; h[31] |= 64;

    geP3 R;
    geScalarMultBase(&R, sig + 32);

    geP3 Ah;
    geScalarMult(&Ah, h, &A);

    geP3 Rcalc;
    geAdd(&Rcalc, &R, &Ah);

    uint8_t RcalcBytes[32], RexpectedBytes[32];
    geP3ToBytes(RcalcBytes, &Rcalc);

    geP3 Rpoint;
    if (!geFromBytesNegateVartime(&Rpoint, sig)) return false;
    geP3ToBytes(RexpectedBytes, &Rpoint);

    return memcmp(RcalcBytes, RexpectedBytes, 32) == 0;
}
