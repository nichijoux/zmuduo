#include "string_util.h"
#include <iomanip>
#include <sstream>

namespace zmuduo::utils {
std::string StringUtil::UrlEncode(const std::string& url, bool isFullUrl, bool spaceAsPlus) {
    std::string scheme_host, path, query, fragment;
    std::string rest = url;

    // 提取 scheme 和主机部分（仅当 isFullUrl 为 true）
    if (isFullUrl) {
        size_t scheme_end = url.find("://");
        if (scheme_end != std::string::npos) {
            scheme_end += 3;  // 跳过 "://"
            size_t path_start = url.find('/', scheme_end);
            if (path_start == std::string::npos) {
                scheme_host = url;  // 没有路径，仅 scheme+host
                rest.clear();
            } else {
                scheme_host = url.substr(0, path_start);
                rest        = url.substr(path_start);
            }
        } else {
            // 没有发现 "://"，认为不是完整 URL
            rest = url;
        }
    }

    // 分离 fragment（#片段）
    size_t hash_pos = rest.find('#');
    if (hash_pos != std::string::npos) {
        fragment = rest.substr(hash_pos + 1);
        rest     = rest.substr(0, hash_pos);
    }

    // 分离 query（?查询参数）
    size_t qmark_pos = rest.find('?');
    if (qmark_pos != std::string::npos) {
        path  = rest.substr(0, qmark_pos);
        query = rest.substr(qmark_pos + 1);
    } else {
        path = rest;
    }

    // 组装最终结果
    std::string encoded = isFullUrl ? scheme_host : "";
    encoded += UrlEncodeCore(path, spaceAsPlus, false);  // path 中保留 '/'
    if (!query.empty()) {
        encoded += '?' + UrlEncodeCore(query, spaceAsPlus, true);  // query 编码 '/'
    }
    if (!fragment.empty()) {
        encoded += '#' + UrlEncodeCore(fragment, spaceAsPlus, true);  // fragment 编码所有
    }

    return encoded;
}

std::string StringUtil::UrlDecode(const std::string& str, bool spaceAsPlus) {
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
                std::istringstream hexStream(std::string() + hex1 + hex2);
                int                hexValue;
                hexStream >> std::hex >> hexValue;

                result += static_cast<char>(hexValue);
                i += 2;  // 跳过已处理的两位十六进制数
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

std::string StringUtil::Trim(const std::string& str, const std::string& delimit) {
    auto start = str.find_first_not_of(delimit);
    if (start == std::string::npos) {
        return {};
    }
    auto end = str.find_last_not_of(delimit);
    return str.substr(start, end - start + 1);
}

std::string StringUtil::TrimLeft(const std::string& str, const std::string& delimit) {
    auto start = str.find_first_not_of(delimit);
    return (start == std::string::npos) ? "" : str.substr(start);
}

std::string StringUtil::TrimRight(const std::string& str, const std::string& delimit) {
    auto end = str.find_last_not_of(delimit);
    return (end == std::string::npos) ? "" : str.substr(0, end + 1);
}

std::vector<std::string> StringUtil::Split(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::istringstream       iss(str);
    std::string              token;
    while (std::getline(iss, token, delimiter)) {
        result.emplace_back(token);
    }
    return result;
}

std::vector<std::string> StringUtil::Split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t                   start = 0, end = str.find(delimiter);
    while (end != std::string::npos) {
        tokens.emplace_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end   = str.find(delimiter, start);
    }
    tokens.emplace_back(str.substr(start));
    return tokens;
}
bool StringUtil::StartsWith(const std::string& str, const std::string& prefix) {
    if (prefix.empty()) {
        return true;
    }
    if (str.length() < prefix.length()) {
        return false;
    }
    return str.compare(0, prefix.length(), prefix) == 0;
}

std::string StringUtil::UrlEncodeCore(const std::string& str, bool spaceAsPlus, bool encodeSlash) {
    std::ostringstream oss;
    for (unsigned char c : str) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            oss << c;
        } else if (c == '/' && !encodeSlash) {
            oss << '/';
        } else if (c == ' ' && spaceAsPlus) {
            oss << '+';
        } else {
            oss << '%' << std::uppercase << std::setw(2) << std::setfill('0') << std::hex
                << static_cast<int>(c);
        }
    }
    return oss.str();
}
}  // namespace zmuduo::utils