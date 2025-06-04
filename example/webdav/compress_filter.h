#pragma once

#include <string>
#include <zlib.h>
#include <zmuduo/net/http/http_filter.h>

using namespace zmuduo;
using namespace zmuduo::net;
using namespace zmuduo::net::http;

class CompressFilter : public HttpFilter {
  public:
    CompressFilter() : HttpFilter("Compress") {}

    void beforeHandle(HttpRequest& request) override;
    void afterHandle(HttpResponse& response) override;

  private:
    enum class Encoding { None, Gzip, Deflate };

    struct EncodingEntry {
        Encoding type;
        double q;
    };

    static std::string CompressGzip(const std::string& data, int level = Z_BEST_COMPRESSION);
    static std::string CompressDeflate(const std::string& data, int level = Z_BEST_COMPRESSION);

    Encoding m_encoding{Encoding::None};
};
