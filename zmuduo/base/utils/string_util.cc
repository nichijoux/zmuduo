#include "string_util.h"
#include <sstream>

namespace {
std::string UrlEncodeCore(const std::string& str, bool spaceAsPlus, bool encodeSlash) {
    std::ostringstream oss;
    for (unsigned char c : str) {
        if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || ('0' <= c && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            oss << c;
        } else if (c == '/' && !encodeSlash) {
            oss << '/';
        } else if (c == ' ' && spaceAsPlus) {
            oss << '+';
        } else {
            // 使用查找表加速转换
            static const char hexLookup[] = "0123456789ABCDEF";
            // 单个字符的 URL 编码（转换为 %XX 格式）
            oss << '%' << hexLookup[(c & 0xF0) >> 4]  // 高 4 位
                << hexLookup[c & 0x0F];               // 低 4 位
        }
    }
    return oss.str();
}
}  // namespace

namespace zmuduo::utils::string_util {
std::string UrlEncode(const std::string& url, bool isFullUrl, bool spaceAsPlus) {
    std::string schemeHost, path, query, fragment;
    std::string rest = url;

    // 提取 scheme 和主机部分（仅当 isFullUrl 为 true）
    if (isFullUrl) {
        auto schemeEnd = url.find("://");
        if (schemeEnd != std::string::npos) {
            // 跳过 "://"
            schemeEnd += 3;
            size_t pathStart = url.find('/', schemeEnd);
            if (pathStart == std::string::npos) {
                // 没有路径，仅 scheme+host
                schemeHost = url;
                rest.clear();
            } else {
                schemeHost = url.substr(0, pathStart);
                rest       = url.substr(pathStart);
            }
        } else {
            // 没有发现 "://"，认为不是完整 URL
            rest = url;
        }
    }

    // 分离 fragment（#片段）
    auto hashPosition = rest.find('#');
    if (hashPosition != std::string::npos) {
        fragment = rest.substr(hashPosition + 1);
        rest     = rest.substr(0, hashPosition);
    }

    // 分离 query（?查询参数）
    auto qmarkPosition = rest.find('?');
    if (qmarkPosition != std::string::npos) {
        path  = rest.substr(0, qmarkPosition);
        query = rest.substr(qmarkPosition + 1);
    } else {
        path = rest;
    }

    // 组装最终结果
    std::string encoded = isFullUrl ? schemeHost : "";
    // path 中保留 '/'
    encoded += UrlEncodeCore(path, spaceAsPlus, false);
    if (!query.empty()) {
        // query 编码 '/'
        encoded += '?' + UrlEncodeCore(query, spaceAsPlus, true);
    }
    if (!fragment.empty()) {
        // fragment 编码所有
        encoded += '#' + UrlEncodeCore(fragment, spaceAsPlus, true);
    }
    return encoded;
}

std::string UrlDecode(const std::string& str, bool spaceAsPlus) {
    static auto hexToDec = [](char c) -> uint8_t {
        if ('0' <= c && c <= '9') {
            return c - '0';
        } else if ('A' <= c && c <= 'F') {
            return c - 'A' + 10;
        } else {
            return c - 'a' + 10;
        }
    };
    // 返回
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];

        if (spaceAsPlus && c == '+') {
            result += ' ';
        } else if (c == '%' && i + 2 < str.size()) {
            // 检查后面两个字符是否是十六进制数字
            char hex1 = str[i + 1];
            char hex2 = str[i + 2];
            if (isxdigit(hex1) && isxdigit(hex2)) {
                // 将十六进制字符转换为数值
                uint8_t hexValue = (hexToDec(hex1) << 4) | hexToDec(hex2);
                result += static_cast<char>(hexValue);
                // 跳过已处理的两位十六进制数
                i += 2;
            } else {
                // 不是有效的百分号编码，保持原样
                result += c;
            }
        } else {
            result += c;
        }
    }

    return result;
}

std::string Trim(const std::string& str, const std::string& delimit) {
    auto start = str.find_first_not_of(delimit);
    if (start == std::string::npos) {
        return {};
    }
    auto end = str.find_last_not_of(delimit);
    return str.substr(start, end - start + 1);
}

std::string TrimLeft(const std::string& str, const std::string& delimit) {
    auto start = str.find_first_not_of(delimit);
    return (start == std::string::npos) ? "" : str.substr(start);
}

std::string TrimRight(const std::string& str, const std::string& delimit) {
    auto end = str.find_last_not_of(delimit);
    return (end == std::string::npos) ? "" : str.substr(0, end + 1);
}

std::vector<std::string> Split(const std::string& s, char delimiter) {
    if (s.empty()) {
        return {""};
    }

    std::vector<std::string> result;
    // 分割坐标
    size_t start = 0, end = 0;
    // 不断遍历
    while (end != std::string::npos) {
        // 查找下一个分隔符
        end = s.find(delimiter, start);
        // 将子字符串添加到结果中
        result.emplace_back(s.substr(start, end - start));
        // 移动到下一个可能的起始位置
        start = end + 1;
    }
    return result;
}

std::vector<std::string> Split(const std::string& str, const std::string& delimiter) {
    if (str.empty()) {
        return {""};
    }

    if (delimiter.empty()) {
        return {str};
    }

    std::vector<std::string> result;

    size_t start = 0;
    auto   step  = delimiter.length();

    while (start <= str.length()) {
        auto end = str.find(delimiter, start);
        if (end == std::string::npos) {
            // 剩余部分作为最后一个子串
            result.emplace_back(str.substr(start));
            break;
        }
        // 添加当前子串
        result.emplace_back(str.substr(start, end - start));
        // 移动到分隔符之后
        start = end + step;
    }

    return result;
}

bool StartsWith(const std::string& str, const std::string& prefix) {
    if (prefix.empty()) {
        return true;
    }
    if (str.length() < prefix.length()) {
        return false;
    }
    return str.compare(0, prefix.length(), prefix) == 0;
}
}  // namespace zmuduo::utils::string_util