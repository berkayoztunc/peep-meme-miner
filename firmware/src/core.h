// Portable core (no Arduino deps): base58, base64, hex, sha256, PDA derivation.
// Host-testable: see firmware/test/test_core.cpp
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace core {

typedef std::vector<uint8_t> Bytes;

// ---------- base58 ----------
static const char* B58ALPHA = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

inline std::string b58enc(const uint8_t* in, size_t len) {
  size_t zeros = 0;
  while (zeros < len && in[zeros] == 0) zeros++;
  size_t size = (len - zeros) * 138 / 100 + 1;
  std::vector<uint8_t> b(size, 0);
  size_t length = 0;
  for (size_t k = zeros; k < len; k++) {
    int carry = in[k];
    size_t i = 0;
    for (int p = (int)size - 1; (carry != 0 || i < length) && p >= 0; p--, i++) {
      carry += 256 * b[p];
      b[p] = carry % 58;
      carry /= 58;
    }
    length = i;
  }
  size_t start = size - length;
  while (start < size && b[start] == 0) start++;
  std::string s(zeros, '1');
  for (; start < size; start++) s += B58ALPHA[b[start]];
  return s;
}

inline bool b58dec(const std::string& s, Bytes& out) {
  size_t zeros = 0;
  while (zeros < s.size() && s[zeros] == '1') zeros++;
  size_t size = s.size() * 733 / 1000 + 1;
  std::vector<uint8_t> b(size, 0);
  size_t length = 0;
  for (size_t k = zeros; k < s.size(); k++) {
    const char* p0 = strchr(B58ALPHA, s[k]);
    if (!p0 || s[k] == 0) return false;
    int carry = (int)(p0 - B58ALPHA);
    size_t i = 0;
    for (int p = (int)size - 1; (carry != 0 || i < length) && p >= 0; p--, i++) {
      carry += 58 * b[p];
      b[p] = carry % 256;
      carry /= 256;
    }
    length = i;
  }
  size_t start = size - length;
  while (start < size && b[start] == 0) start++;
  out.assign(zeros, 0);
  out.insert(out.end(), b.begin() + start, b.end());
  return true;
}

// ---------- hex ----------
inline bool hexdec(const std::string& h, Bytes& out) {
  if (h.size() % 2) return false;
  out.clear();
  auto v = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (size_t i = 0; i < h.size(); i += 2) {
    int a = v(h[i]), b = v(h[i + 1]);
    if (a < 0 || b < 0) return false;
    out.push_back((uint8_t)(a * 16 + b));
  }
  return true;
}

// ---------- base64 ----------
inline std::string b64enc(const uint8_t* d, size_t n) {
  static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  o.reserve((n + 2) / 3 * 4);
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)d[i] << 16;
    if (i + 1 < n) v |= (uint32_t)d[i + 1] << 8;
    if (i + 2 < n) v |= d[i + 2];
    o += T[(v >> 18) & 63];
    o += T[(v >> 12) & 63];
    o += (i + 1 < n) ? T[(v >> 6) & 63] : '=';
    o += (i + 2 < n) ? T[v & 63] : '=';
  }
  return o;
}

inline bool b64dec(const std::string& s, Bytes& out) {
  out.clear();
  uint32_t acc = 0;
  int bits = 0;
  for (char c : s) {
    int v;
    if (c >= 'A' && c <= 'Z') v = c - 'A';
    else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
    else if (c >= '0' && c <= '9') v = c - '0' + 52;
    else if (c == '+') v = 62;
    else if (c == '/') v = 63;
    else if (c == '=') break;
    else if (c == '\n' || c == '\r' || c == ' ') continue;
    else return false;
    acc = (acc << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back((uint8_t)((acc >> bits) & 0xFF));
    }
  }
  return true;
}

// ---------- sha256 ----------
struct Sha256 {
  uint32_t h[8];
  uint8_t buf[64];
  uint64_t total = 0;
  size_t fill = 0;
  Sha256() {
    static const uint32_t i0[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(h, i0, sizeof(h));
  }
  static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
  void block(const uint8_t* p) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
      w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) | ((uint32_t)p[i * 4 + 2] << 8) | p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
      uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
      uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      uint32_t ch = (e & f) ^ (~e & g);
      uint32_t t1 = hh + S1 + ch + K[i] + w[i];
      uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t t2 = S0 + mj;
      hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  void update(const uint8_t* d, size_t n) {
    total += n;
    while (n) {
      size_t k = 64 - fill;
      if (k > n) k = n;
      memcpy(buf + fill, d, k);
      fill += k; d += k; n -= k;
      if (fill == 64) { block(buf); fill = 0; }
    }
  }
  void final(uint8_t out[32]) {
    uint64_t bits = total * 8;
    uint8_t pad = 0x80;
    update(&pad, 1);
    uint8_t z = 0;
    while (fill != 56) update(&z, 1);
    uint8_t l[8];
    for (int i = 0; i < 8; i++) l[i] = (uint8_t)(bits >> (56 - 8 * i));
    update(l, 8);
    for (int i = 0; i < 8; i++) {
      out[i * 4] = h[i] >> 24; out[i * 4 + 1] = h[i] >> 16; out[i * 4 + 2] = h[i] >> 8; out[i * 4 + 3] = h[i];
    }
  }
};

// ---------- ed25519 on-curve test (tweetnacl unpackneg port) ----------
namespace curve {
typedef int64_t gf[16];
static const gf gf1 = {1};
static const gf D = {0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070,
                     0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203};
static const gf I = {0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43,
                     0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83};
inline void set25519(gf r, const gf a) { for (int i = 0; i < 16; i++) r[i] = a[i]; }
inline void car25519(gf o) {
  for (int i = 0; i < 16; i++) {
    o[i] += (1LL << 16);
    int64_t c = o[i] >> 16;
    o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
    o[i] -= c << 16;
  }
}
inline void sel25519(gf p, gf q, int b) {
  int64_t c = ~(int64_t)(b - 1);
  for (int i = 0; i < 16; i++) {
    int64_t t = c & (p[i] ^ q[i]);
    p[i] ^= t; q[i] ^= t;
  }
}
inline void pack25519(uint8_t* o, const gf n) {
  gf m, t;
  for (int i = 0; i < 16; i++) t[i] = n[i];
  car25519(t); car25519(t); car25519(t);
  for (int j = 0; j < 2; j++) {
    m[0] = t[0] - 0xffed;
    for (int i = 1; i < 15; i++) {
      m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
      m[i - 1] &= 0xffff;
    }
    m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
    int b = (int)((m[15] >> 16) & 1);
    m[14] &= 0xffff;
    sel25519(t, m, 1 - b);
  }
  for (int i = 0; i < 16; i++) { o[2 * i] = (uint8_t)(t[i] & 0xff); o[2 * i + 1] = (uint8_t)(t[i] >> 8); }
}
inline int neq25519(const gf a, const gf b) {
  uint8_t c[32], d[32];
  pack25519(c, a); pack25519(d, b);
  return memcmp(c, d, 32) != 0;
}
inline void unpack25519(gf o, const uint8_t* n) {
  for (int i = 0; i < 16; i++) o[i] = n[2 * i] + ((int64_t)n[2 * i + 1] << 8);
  o[15] &= 0x7fff;
}
inline void A(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] + b[i]; }
inline void Z(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] - b[i]; }
inline void M(gf o, const gf a, const gf b) {
  int64_t t[31];
  for (int i = 0; i < 31; i++) t[i] = 0;
  for (int i = 0; i < 16; i++) for (int j = 0; j < 16; j++) t[i + j] += a[i] * b[j];
  for (int i = 0; i < 15; i++) t[i] += 38 * t[i + 16];
  for (int i = 0; i < 16; i++) o[i] = t[i];
  car25519(o); car25519(o);
}
inline void S(gf o, const gf a) { M(o, a, a); }
inline void pow2523(gf o, const gf i) {
  gf c;
  for (int a = 0; a < 16; a++) c[a] = i[a];
  for (int a = 250; a >= 0; a--) {
    S(c, c);
    if (a != 1) M(c, c, i);
  }
  for (int a = 0; a < 16; a++) o[a] = c[a];
}
// true if the 32 bytes decode to a valid ed25519 point
inline bool onCurve(const uint8_t p[32]) {
  gf r0, r1, r2, t, chk, num, den, den2, den4, den6;
  set25519(r2, gf1);
  unpack25519(r1, p);
  S(num, r1);
  M(den, num, D);
  Z(num, num, r2);
  A(den, r2, den);
  S(den2, den); S(den4, den2); M(den6, den4, den2);
  M(t, den6, num); M(t, t, den);
  pow2523(t, t);
  M(t, t, num); M(t, t, den); M(t, t, den);
  M(r0, t, den);
  S(chk, r0); M(chk, chk, den);
  if (neq25519(chk, num)) M(r0, r0, I);
  S(chk, r0); M(chk, chk, den);
  return !neq25519(chk, num);
}
}  // namespace curve

// ---------- Pubkey + PDA ----------
struct Pk {
  uint8_t b[32];
  Pk() { memset(b, 0, 32); }
  static bool parse(const std::string& s, Pk& out) {
    Bytes v;
    if (!b58dec(s, v) || v.size() != 32) return false;
    memcpy(out.b, v.data(), 32);
    return true;
  }
  static Pk of(const char* s) { Pk p; parse(s, p); return p; }
  std::string str() const { return b58enc(b, 32); }
  bool operator==(const Pk& o) const { return memcmp(b, o.b, 32) == 0; }
};

typedef std::vector<Bytes> Seeds;

inline Bytes seedStr(const char* s) { return Bytes((const uint8_t*)s, (const uint8_t*)s + strlen(s)); }
inline Bytes seedPk(const Pk& p) { return Bytes(p.b, p.b + 32); }

inline bool findPda(const Seeds& seeds, const Pk& program, Pk& out, uint8_t* bumpOut = nullptr) {
  static const char* MARK = "ProgramDerivedAddress";
  for (int bump = 255; bump >= 0; bump--) {
    Sha256 h;
    for (auto& s : seeds) h.update(s.data(), s.size());
    uint8_t bb = (uint8_t)bump;
    h.update(&bb, 1);
    h.update(program.b, 32);
    h.update((const uint8_t*)MARK, strlen(MARK));
    uint8_t d[32];
    h.final(d);
    if (!curve::onCurve(d)) {
      memcpy(out.b, d, 32);
      if (bumpOut) *bumpOut = bb;
      return true;
    }
  }
  return false;
}

}  // namespace core
