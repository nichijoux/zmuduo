#include "hash_util.h"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <iomanip>
#include <ios>
#include <random>

namespace {
inline static uint32_t F(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) | (~x & z);
}
inline static uint32_t G(uint32_t x, uint32_t y, uint32_t z) {
    return (x & z) | (y & ~z);
}
inline static uint32_t H(uint32_t x, uint32_t y, uint32_t z) {
    return x ^ y ^ z;
}
inline static uint32_t I(uint32_t x, uint32_t y, uint32_t z) {
    return y ^ (x | ~z);
}

inline static uint32_t rotate_left(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

inline static void
FF(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
    a = rotate_left(a + F(b, c, d) + x + ac, s) + b;
}

inline static void
GG(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
    a = rotate_left(a + G(b, c, d) + x + ac, s) + b;
}

inline static void
HH(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
    a = rotate_left(a + H(b, c, d) + x + ac, s) + b;
}

inline static void
II(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
    a = rotate_left(a + I(b, c, d) + x + ac, s) + b;
}

void md5encode(uint8_t output[], const uint32_t input[]) {
    for (size_t i = 0, j = 0; j < 16; ++i, j += 4) {
        output[j]     = static_cast<uint8_t>(input[i] & 0xff);
        output[j + 1] = static_cast<uint8_t>((input[i] >> 8) & 0xff);
        output[j + 2] = static_cast<uint8_t>((input[i] >> 16) & 0xff);
        output[j + 3] = static_cast<uint8_t>((input[i] >> 24) & 0xff);
    }
}

void md5decode(uint32_t output[], const uint8_t input[]) {
    for (size_t i = 0, j = 0; j < 64; ++i, j += 4) {
        output[i] = static_cast<uint32_t>(input[j]) | (static_cast<uint32_t>(input[j + 1]) << 8) |
                    (static_cast<uint32_t>(input[j + 2]) << 16) |
                    (static_cast<uint32_t>(input[j + 3]) << 24);
    }
}

/**
 * @brief 执行 SHA1 的循环左移操作（仅供 SHA1 算法内部使用）。
 *
 * @param[in] bits 左移位数。
 * @param[in] word 原始 32 位整数。
 * @return 左移后的结果。
 */
inline uint32_t SHA1CircularShift(uint32_t bits, uint32_t word) {
    return ((word << bits) | (word >> (32 - bits)));
}

/**
 * @brief 执行一次 SHA1 的核心变换（对 512bit 块进行处理）。
 *
 * @param[in,out] state SHA1 状态数组（5 个 32 位整型）。
 * @param[in] buffer 64 字节输入块。
 */
void SHA1Transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t a, b, c, d, e;
    uint32_t w[80];

    // Initialize the first 16 words in the array W
    for (int i = 0; i < 16; ++i) {
        w[i] = (buffer[i * 4 + 0] << 24) | (buffer[i * 4 + 1] << 16) | (buffer[i * 4 + 2] << 8) |
               (buffer[i * 4 + 3] << 0);
    }

    // Extend the 16 words into 80 words
    for (int i = 16; i < 80; ++i) {
        w[i] = SHA1CircularShift(1, w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]);
    }

    // Initialize working variables
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    // Main loop
    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }

        uint32_t temp = SHA1CircularShift(5, a) + f + e + k + w[i];
        e             = d;
        d             = c;
        c             = SHA1CircularShift(30, b);
        b             = a;
        a             = temp;
    }

    // Update the state
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

/**
 * @brief 将 SHA1 的状态数组转换为最终的哈希字符串。
 *
 * @param[in] state SHA1 的 5 个 32 位状态值。
 * @return SHA1 哈希值的十六进制字符串表示。
 */
std::string SHA1Final(const uint32_t state[5]) {
    std::stringstream oss;
    for (int i = 0; i < 5; ++i) {
        oss << std::hex << std::setw(8) << std::setfill('0') << state[i];
    }
    std::string s = oss.str();
    return s;
}

/**
 * @brief MD5 核心压缩函数，对每个 64 字节块进行处理。
 *
 * @param block 64 字节输入块（512 位），已按 MD5 填充规则填充。
 * @param A 初始 A 寄存器值，处理后累加更新。
 * @param B 初始 B 寄存器值，处理后累加更新。
 * @param C 初始 C 寄存器值，处理后累加更新。
 * @param D 初始 D 寄存器值，处理后累加更新。
 *
 * @note 这是 MD5 的主处理函数，包含四轮迭代（每轮 16 次），共 64 次操作，
 *       每轮使用不同的逻辑函数（F/G/H/I）和常量。
 */
void md5Process(const uint8_t* block, uint32_t& A, uint32_t& B, uint32_t& C, uint32_t& D) {
    // 初始化工作变量，复制当前累加器状态
    uint32_t a = A, b = B, c = C, d = D;

    // 将 64 字节块解析为 16 个 32 位小端整数
    uint32_t x[16];
    md5decode(x, block);

    // ===================
    // 第一轮（Round 1）
    // 使用 F 逻辑函数，每步依赖前一步结果
    // ===================
    FF(a, b, c, d, x[0], 7, 0xd76aa478);  // 每步包含常量、旋转量和输入块数据
    FF(d, a, b, c, x[1], 12, 0xe8c7b756);
    FF(c, d, a, b, x[2], 17, 0x242070db);
    FF(b, c, d, a, x[3], 22, 0xc1bdceee);
    FF(a, b, c, d, x[4], 7, 0xf57c0faf);
    FF(d, a, b, c, x[5], 12, 0x4787c62a);
    FF(c, d, a, b, x[6], 17, 0xa8304613);
    FF(b, c, d, a, x[7], 22, 0xfd469501);
    FF(a, b, c, d, x[8], 7, 0x698098d8);
    FF(d, a, b, c, x[9], 12, 0x8b44f7af);
    FF(c, d, a, b, x[10], 17, 0xffff5bb1);
    FF(b, c, d, a, x[11], 22, 0x895cd7be);
    FF(a, b, c, d, x[12], 7, 0x6b901122);
    FF(d, a, b, c, x[13], 12, 0xfd987193);
    FF(c, d, a, b, x[14], 17, 0xa679438e);
    FF(b, c, d, a, x[15], 22, 0x49b40821);

    // ===================
    // 第二轮（Round 2）
    // 使用 G 逻辑函数，不同访问顺序和常量
    // ===================
    GG(a, b, c, d, x[1], 5, 0xf61e2562);
    GG(d, a, b, c, x[6], 9, 0xc040b340);
    GG(c, d, a, b, x[11], 14, 0x265e5a51);
    GG(b, c, d, a, x[0], 20, 0xe9b6c7aa);
    GG(a, b, c, d, x[5], 5, 0xd62f105d);
    GG(d, a, b, c, x[10], 9, 0x02441453);
    GG(c, d, a, b, x[15], 14, 0xd8a1e681);
    GG(b, c, d, a, x[4], 20, 0xe7d3fbc8);
    GG(a, b, c, d, x[9], 5, 0x21e1cde6);
    GG(d, a, b, c, x[14], 9, 0xc33707d6);
    GG(c, d, a, b, x[3], 14, 0xf4d50d87);
    GG(b, c, d, a, x[8], 20, 0x455a14ed);
    GG(a, b, c, d, x[13], 5, 0xa9e3e905);
    GG(d, a, b, c, x[2], 9, 0xfcefa3f8);
    GG(c, d, a, b, x[7], 14, 0x676f02d9);
    GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

    // ===================
    // 第三轮（Round 3）
    // 使用 H 逻辑函数，采用异或操作
    // ===================
    HH(a, b, c, d, x[5], 4, 0xfffa3942);
    HH(d, a, b, c, x[8], 11, 0x8771f681);
    HH(c, d, a, b, x[11], 16, 0x6d9d6122);
    HH(b, c, d, a, x[14], 23, 0xfde5380c);
    HH(a, b, c, d, x[1], 4, 0xa4beea44);
    HH(d, a, b, c, x[4], 11, 0x4bdecfa9);
    HH(c, d, a, b, x[7], 16, 0xf6bb4b60);
    HH(b, c, d, a, x[10], 23, 0xbebfbc70);
    HH(a, b, c, d, x[13], 4, 0x289b7ec6);
    HH(d, a, b, c, x[0], 11, 0xeaa127fa);
    HH(c, d, a, b, x[3], 16, 0xd4ef3085);
    HH(b, c, d, a, x[6], 23, 0x04881d05);
    HH(a, b, c, d, x[9], 4, 0xd9d4d039);
    HH(d, a, b, c, x[12], 11, 0xe6db99e5);
    HH(c, d, a, b, x[15], 16, 0x1fa27cf8);
    HH(b, c, d, a, x[2], 23, 0xc4ac5665);

    // ===================
    // 第四轮（Round 4）
    // 使用 I 逻辑函数，最终轮
    // ===================
    II(a, b, c, d, x[0], 6, 0xf4292244);
    II(d, a, b, c, x[7], 10, 0x432aff97);
    II(c, d, a, b, x[14], 15, 0xab9423a7);
    II(b, c, d, a, x[5], 21, 0xfc93a039);
    II(a, b, c, d, x[12], 6, 0x655b59c3);
    II(d, a, b, c, x[3], 10, 0x8f0ccc92);
    II(c, d, a, b, x[10], 15, 0xffeff47d);
    II(b, c, d, a, x[1], 21, 0x85845dd1);
    II(a, b, c, d, x[8], 6, 0x6fa87e4f);
    II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
    II(c, d, a, b, x[6], 15, 0xa3014314);
    II(b, c, d, a, x[13], 21, 0x4e0811a1);
    II(a, b, c, d, x[4], 6, 0xf7537e82);
    II(d, a, b, c, x[11], 10, 0xbd3af235);
    II(c, d, a, b, x[2], 15, 0x2ad7d2bb);
    II(b, c, d, a, x[9], 21, 0xeb86d391);

    // 将本次处理的结果加到当前的摘要值中（累加）
    A += a;
    B += b;
    C += c;
    D += d;
}
}  // namespace

namespace zmuduo::utils::hash_util {
std::string Base64decode(std::string_view src) {
    static const signed char decodingTable[] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62,
        -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0,
        1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
        39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };

    size_t len = src.size();
    if (len % 4 != 0) {
        return {};
    }

    size_t padding = 0;
    if (len >= 1 && src[len - 1] == '=') {
        padding++;
    }
    if (len >= 2 && src[len - 2] == '=') {
        padding++;
    }

    std::string decoded;
    decoded.reserve((len / 4) * 3 - padding);

    for (size_t i = 0; i < len; i += 4) {
        uint32_t sextet_a = src[i] == '=' ? 0 : decodingTable[static_cast<unsigned char>(src[i])];
        uint32_t sextet_b =
            src[i + 1] == '=' ? 0 : decodingTable[static_cast<unsigned char>(src[i + 1])];
        uint32_t sextet_c =
            src[i + 2] == '=' ? 0 : decodingTable[static_cast<unsigned char>(src[i + 2])];
        uint32_t sextet_d =
            src[i + 3] == '=' ? 0 : decodingTable[static_cast<unsigned char>(src[i + 3])];

        if (sextet_a == -1 || sextet_b == -1 || (src[i + 2] != '=' && sextet_c == -1) ||
            (src[i + 3] != '=' && sextet_d == -1)) {
            return {};
        }

        uint32_t triple = (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;

        decoded.push_back(static_cast<char>((triple >> 16) & 0xFF));
        if (src[i + 2] != '=')
            decoded.push_back(static_cast<char>((triple >> 8) & 0xFF));
        if (src[i + 3] != '=')
            decoded.push_back(static_cast<char>(triple & 0xFF));
    }

    return decoded;
}

std::string Base64encode(const void* data, size_t length) {
    static const char encodingTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const auto* bytes = reinterpret_cast<const unsigned char*>(data);
    // 边界处理
    if (length == 0) {
        return {};
    }
    std::string encoded;
    encoded.reserve(((length + 2) / 3) * 4);

    for (size_t i = 0; i < length;) {
        uint32_t octet_a = i < length ? bytes[i++] : 0;
        uint32_t octet_b = i < length ? bytes[i++] : 0;
        uint32_t octet_c = i < length ? bytes[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded.push_back(encodingTable[(triple >> 18) & 0x3F]);
        encoded.push_back(encodingTable[(triple >> 12) & 0x3F]);
        encoded.push_back(i > length + 1 ? '=' : encodingTable[(triple >> 6) & 0x3F]);
        encoded.push_back(i > length ? '=' : encodingTable[triple & 0x3F]);
    }

    size_t mod = length % 3;
    if (mod > 0) {
        encoded[encoded.size() - 1] = '=';
        if (mod == 1) {
            encoded[encoded.size() - 2] = '=';
        }
    }
    return encoded;
}

std::string HexToBinary(std::string_view hex) {
    std::vector<unsigned char> binary;
    binary.reserve(hex.size() / 2);  // 预分配空间

    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned char byte = 0;
        auto [ptr, ec]     = std::from_chars(hex.data() + i, hex.data() + i + 2, byte, 16);
        if (ec != std::errc() || ptr != hex.data() + i + 2) {
            return "";
        }
        binary.push_back(byte);
    }
    return {binary.begin(), binary.end()};
}

std::string SHA1sum(const void* data, size_t length) {
    uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint8_t  buffer[64];
    uint64_t message_len_bits = length * 8;

    const auto* ptr       = static_cast<const uint8_t*>(data);
    size_t      remaining = length;

    // Process full 64-byte blocks
    while (remaining >= 64) {
        memcpy(buffer, ptr, 64);
        SHA1Transform(state, buffer);
        ptr += 64;
        remaining -= 64;
    }

    // Process the remaining bytes (less than 64)
    if (remaining > 0) {
        memcpy(buffer, ptr, remaining);
    }

    // Pad the remaining block (must be <= 56 bytes to fit the 8-byte length)
    if (remaining < 56) {
        buffer[remaining] = 0x80;                           // Append '1' bit
        std::fill(buffer + remaining + 1, buffer + 56, 0);  // Pad with zeros
    } else {
        // If remaining >= 56, we need an extra block
        buffer[remaining] = 0x80;
        std::fill(buffer + remaining + 1, buffer + 64, 0);
        SHA1Transform(state, buffer);
        std::fill(buffer, buffer + 56, 0);  // New block starts with zeros
    }

    // Append the original message length (in bits) as a 64-bit big-endian integer
    for (int i = 0; i < 8; ++i) {
        buffer[56 + i] = (message_len_bits >> (56 - 8 * i)) & 0xFF;
    }

    SHA1Transform(state, buffer);
    return SHA1Final(state);
}

std::string MD5(const std::string& input, int bitLength /* = 32 */, bool toUpper /* = false */) {
    // 初始化
    uint32_t A = 0x67452301;
    uint32_t B = 0xefcdab89;
    uint32_t C = 0x98badcfe;
    uint32_t D = 0x10325476;

    // 消息填充
    size_t               inputLen = input.size();
    size_t               newLen   = ((inputLen + 8) / 64 + 1) * 64;
    std::vector<uint8_t> buffer(newLen, 0);
    std::memcpy(buffer.data(), input.c_str(), inputLen);
    buffer[inputLen] = 0x80;
    uint64_t bitLen  = inputLen * 8;
    std::memcpy(&buffer[newLen - 8], &bitLen, 8);

    for (size_t i = 0; i < newLen; i += 64) {
        md5Process(buffer.data() + i, A, B, C, D);
    }

    // 输出摘要
    uint8_t digest[16];
    md5encode(digest, std::vector<uint32_t>{A, B, C, D}.data());

    std::ostringstream oss;
    for (int i = 0; i < 16; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }

    std::string md5str = oss.str();

    if (bitLength == 16) {
        md5str = md5str.substr(8, 16);
    }

    if (toUpper) {
        std::transform(md5str.begin(), md5str.end(), md5str.begin(), ::toupper);
    }

    return md5str;
}

std::string RandomString(size_t length) {
    // 使用随机设备作为种子
    std::random_device rd;
    // 使用Mersenne Twister引擎
    std::mt19937 gen(rd());
    // 定义分布范围：0-127
    std::uniform_int_distribution<int> dist(0, 127);

    std::string result;
    // 预分配空间提高效率
    result.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        // 生成随机字符并添加到字符串
        result.push_back(static_cast<char>(dist(gen)));
    }

    return result;
}
}  // namespace zmuduo::utils::hash_util