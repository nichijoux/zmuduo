#include "compress_filter.h"
#include <algorithm>
#include <stdexcept>
#include <zlib.h>
#include <zmuduo/base/logger.h>
#include <zmuduo/base/utils/string_util.h>

using namespace zmuduo::utils::string_util;

void CompressFilter::beforeHandle(HttpRequest& request) {
    m_encoding = Encoding::None;

    auto rawHeader = request.getHeader("Accept-Encoding");
    if (rawHeader.empty())
        return;

    auto                       encodings = Split(rawHeader, ",");
    std::vector<EncodingEntry> clientPreferred;

    for (auto& enc : encodings) {
        auto parts = Split(Trim(enc), ";");

        std::string name = Trim(parts[0]);
        double      q    = 1.0;

        if (parts.size() > 1) {
            auto qPos = parts[1].find("q=");
            if (qPos != std::string::npos) {
                try {
                    q = std::stod(parts[1].substr(qPos + 2));
                } catch (...) { q = 0.0; }
            }
        }

        Encoding type = Encoding::None;
        if (name == "gzip") {
            type = Encoding::Gzip;
        } else if (name == "deflate") {
            type = Encoding::Deflate;
        } else if (name == "identity") {
            type = Encoding::None;
        } else {
            continue;
        }
        clientPreferred.push_back({type, q});
    }

    if (clientPreferred.empty()) {
        return;
    }

    std::sort(clientPreferred.begin(), clientPreferred.end(),
              [](const EncodingEntry& a, const EncodingEntry& b) { return a.q > b.q; });

    for (const auto& entry : clientPreferred) {
        if (entry.q <= 0.0) {
            continue;
        }

        if (entry.type == Encoding::Gzip || entry.type == Encoding::Deflate ||
            entry.type == Encoding::None) {
            m_encoding = entry.type;
            break;
        }
    }
}

void CompressFilter::afterHandle(HttpResponse& response) {
    if (m_encoding == Encoding::None)
        return;

    try {
        std::string compressed;
        if (m_encoding == Encoding::Gzip) {
            compressed = CompressGzip(response.getBody());
            response.setHeader("Content-Encoding", "gzip");
        } else if (m_encoding == Encoding::Deflate) {
            compressed = CompressDeflate(response.getBody());
            response.setHeader("Content-Encoding", "deflate");
        }
        response.setBody(compressed);
        response.setHeader("Vary", "Accept-Encoding");
    } catch (const std::exception& e) { ZMUDUO_LOG_ERROR << "压缩失败: " << e.what(); }
}

std::string CompressFilter::CompressGzip(const std::string& data, int level) {
    z_stream zs{};
    if (deflateInit2(&zs, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        throw std::runtime_error("deflateInit2 (gzip) failed");

    zs.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    char        outbuffer[32768];
    std::string outstring;

    int ret;
    do {
        zs.next_out  = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        ret          = deflate(&zs, Z_FINISH);
        if (outstring.size() < zs.total_out)
            outstring.append(outbuffer, zs.total_out - outstring.size());
    } while (ret == Z_OK);

    deflateEnd(&zs);
    if (ret != Z_STREAM_END)
        throw std::runtime_error("gzip compression failed");

    return outstring;
}

std::string CompressFilter::CompressDeflate(const std::string& data, int level) {
    z_stream zs{};
    if (deflateInit(&zs, level) != Z_OK)
        throw std::runtime_error("deflateInit failed");

    zs.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    char        outbuffer[32768];
    std::string outstring;

    int ret;
    do {
        zs.next_out  = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        ret          = deflate(&zs, Z_FINISH);
        if (outstring.size() < zs.total_out)
            outstring.append(outbuffer, zs.total_out - outstring.size());
    } while (ret == Z_OK);

    deflateEnd(&zs);
    if (ret != Z_STREAM_END)
        throw std::runtime_error("deflate compression failed");

    return outstring;
}
