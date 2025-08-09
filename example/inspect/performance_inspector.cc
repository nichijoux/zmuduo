#include "performance_inspector.h"

#include <csignal>
#include <gperftools/malloc_extension.h>
#include <gperftools/profiler.h>
#include <zmuduo/base/utils/fs_util.h>
#include <zmuduo/base/utils/system_util.h>

using namespace zmuduo::utils::fs_util;
using namespace zmuduo::utils::system_util;

namespace {
std::string getProcStatus() {
    return ReadText("/proc/self/status", 65536);
}

std::string getProcName(const std::string& stat) {
    size_t lp = stat.find('(');
    size_t rp = stat.rfind(')');
    if (lp != std::string::npos && rp != std::string::npos && lp < rp) {
        return {stat.data() + lp + 1, static_cast<size_t>(rp - lp - 1)};
    }
    return "";
}

std::string htmlEscape(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (char c : input) {
        switch (c) {
            case '&': output.append("&amp;"); break;
            case '<': output.append("&lt;"); break;
            case '>': output.append("&gt;"); break;
            case '"': output.append("&quot;"); break;
            case '\'': output.append("&#39;"); break;
            default: output.push_back(c); break;
        }
    }
    return output;
}

}  // namespace

namespace zmuduo::inspect {
PerformanceInspector::PerformanceInspector() : Servlet("PerformanceInspector") {
    // 注册
    m_dispatcher.addExactServlet("/heap", [](const HttpRequest& request, HttpResponse& response) {
        PerformanceInspector::heap(request, response);
    });
    m_dispatcher.addExactServlet("/growth", [](const HttpRequest& request, HttpResponse& response) {
        PerformanceInspector::growth(request, response);
    });
    m_dispatcher.addExactServlet("/profile",
                                 [](const HttpRequest& request, HttpResponse& response) {
                                     PerformanceInspector::profile(request, response);
                                 });
    m_dispatcher.addExactServlet("/memstats",
                                 [](const HttpRequest& request, HttpResponse& response) {
                                     PerformanceInspector::memstats(request, response);
                                 });
    m_dispatcher.addExactServlet("/memhistogram",
                                 [](const HttpRequest& request, HttpResponse& response) {
                                     PerformanceInspector::memhistogram(request, response);
                                 });
    m_dispatcher.addExactServlet("/releaseFreeMemory",
                                 [](const HttpRequest& request, HttpResponse& response) {
                                     PerformanceInspector::releaseFreeMemory(request, response);
                                 });
    // 添加一个过滤器用于去除path
    m_dispatcher.addFilter(
        "pathConvert",
        [](auto& request) {
            // 首先获取完整的path
            auto path = request.getPath();
            request.setPath(path.substr(path.find_last_of('/')));
        },
        nullptr);
}

void PerformanceInspector::handle(const HttpRequest& request, HttpResponse& response) {
    m_dispatcher.handle(const_cast<HttpRequest&>(request), response);
}

void PerformanceInspector::heap(const HttpRequest& request, HttpResponse& response) {
    std::string raw;
    MallocExtension::instance()->GetHeapSample(&raw);

    std::stringstream html;
    html << R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Heap Sample - Performance Inspector</title>
  <style>
    body {
      font-family: "Courier New", monospace;
      background-color: #f8f9fa;
      color: #212529;
      margin: 0;
      padding: 2rem;
    }
    .container {
      max-width: 960px;
      margin: auto;
      background: #fff;
      border-radius: 8px;
      padding: 2rem;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
    }
    h1 {
      font-family: "Segoe UI", sans-serif;
      font-size: 1.8rem;
      margin-bottom: 1rem;
      color: #0d6efd;
    }
    pre {
      background: #f1f3f5;
      padding: 1rem;
      border-radius: 5px;
      overflow-x: auto;
      white-space: pre-wrap;
      line-height: 1.4;
      border: 1px solid #dee2e6;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Heap Sample Report</h1>
    <pre>)";

    html << raw;

    html << R"(</pre>
  </div>
</body>
</html>
)";

    response.setStatus(HttpStatus::OK);
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    response.setBody(html.str());
}

void PerformanceInspector::growth(const HttpRequest& request, HttpResponse& response) {
    std::string raw;
    MallocExtension::instance()->GetHeapGrowthStacks(&raw);

    std::stringstream html;
    html << R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Heap Growth Stacks - Performance Inspector</title>
  <style>
    body {
      font-family: "Courier New", monospace;
      background-color: #f4f6f8;
      color: #212529;
      margin: 0;
      padding: 2rem;
    }
    .container {
      max-width: 960px;
      margin: auto;
      background: #fff;
      border-radius: 8px;
      padding: 2rem;
      box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1);
    }
    h1 {
      font-family: "Segoe UI", sans-serif;
      font-size: 1.8rem;
      margin-bottom: 1rem;
      color: #d63384;
    }
    pre {
      background-color: #f1f3f5;
      padding: 1rem;
      border-radius: 5px;
      overflow-x: auto;
      white-space: pre-wrap;
      line-height: 1.4;
      border: 1px solid #dee2e6;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Heap Growth Stack Report</h1>
    <pre>)";

    html << raw;

    html << R"(</pre>
  </div>
</body>
</html>
)";

    response.setStatus(HttpStatus::OK);
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    response.setBody(html.str());
}

void PerformanceInspector::profile(const HttpRequest& request, HttpResponse& response) {
    std::string filename = "/tmp/" + getProcName(getProcStatus());
    filename += "." + std::to_string(::getpid());
    filename += "." + Timestamp::Now().toString();
    filename += ".profile";

    std::string profile;
    bool        success = false;
    if (ProfilerStart(filename.c_str())) {
        SleepUsec(30 * 1000 * 1000);
        ProfilerStop();
        profile = ReadText(filename, 1024 * 1024);
        ::unlink(filename.c_str());
        success = true;
    }

    std::stringstream html;
    html << R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>CPU Profile Result</title>
  <style>
    body { font-family: sans-serif; background: #f9f9f9; padding: 2rem; }
    .container {
      max-width: 800px;
      margin: auto;
      background: #fff;
      padding: 2rem;
      border-radius: 10px;
      box-shadow: 0 0 10px rgba(0,0,0,0.1);
    }
    h1 { color: #0d6efd; }
    pre {
      background: #f1f1f1;
      padding: 1rem;
      border-radius: 8px;
      overflow-x: auto;
      max-height: 500px;
    }
    .meta {
      background: #e9ecef;
      padding: 0.5rem 1rem;
      border-radius: 6px;
      margin-bottom: 1rem;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>CPU Profile Result</h1>
)";

    if (success) {
        html << "<div class='meta'><strong>Profile Duration:</strong> 30s<br>"
             << "<strong>File Size:</strong> " << profile.size() << " bytes</div>" << "<pre>"
             << htmlEscape(profile) << "</pre>";
    } else {
        html << "<p style='color: red;'>❌ Failed to start profiler. Ensure `gperftools` is "
                "properly initialized.</p>";
    }

    html << R"(
  </div>
</body>
</html>
)";

    response.setStatus(HttpStatus::OK);
    response.setHeader("Content-Type", "text/html");
    response.setBody(html.str());
}

void PerformanceInspector::memstats(const HttpRequest& request, HttpResponse& response) {
    char buffer[1024 * 64];
    MallocExtension::instance()->GetStats(buffer, sizeof(buffer));

    std::stringstream html;
    html << R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Memory Statistics</title>
  <style>
    body {
      font-family: "Segoe UI", sans-serif;
      background-color: #f8f9fa;
      padding: 2rem;
    }
    .container {
      max-width: 960px;
      margin: auto;
      background: #ffffff;
      padding: 2rem;
      border-radius: 8px;
      box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1);
    }
    h1 {
      color: #0d6efd;
      font-size: 1.75rem;
      margin-bottom: 1.5rem;
    }
    pre {
      background: #f1f3f5;
      padding: 1rem;
      border-radius: 6px;
      overflow-x: auto;
      white-space: pre-wrap;
      line-height: 1.5;
      font-family: "Courier New", monospace;
      border: 1px solid #dee2e6;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>tcmalloc Memory Statistics</h1>
    <pre>)";

    html << buffer;

    html << R"(</pre>
  </div>
</body>
</html>
)";

    response.setStatus(HttpStatus::OK);
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    response.setBody(html.str());
}

void PerformanceInspector::memhistogram(const HttpRequest& request, HttpResponse& response) {
    int    blocks                          = 0;
    size_t total                           = 0;
    int    histogram[kMallocHistogramSize] = {0};

    MallocExtension::instance()->MallocMemoryStats(&blocks, &total, histogram);

    std::stringstream html;
    html << R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Memory Histogram</title>
  <style>
    body {
      font-family: "Segoe UI", sans-serif;
      background-color: #f4f6f8;
      margin: 2rem;
    }
    .container {
      background: #fff;
      border-radius: 8px;
      padding: 2rem;
      max-width: 800px;
      margin: auto;
      box-shadow: 0 2px 12px rgba(0, 0, 0, 0.1);
    }
    h1 {
      color: #0d6efd;
      margin-bottom: 1rem;
    }
    .summary {
      margin-bottom: 1.5rem;
      font-size: 1.1rem;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      margin-top: 1rem;
    }
    th, td {
      border: 1px solid #dee2e6;
      padding: 0.6rem 0.8rem;
      text-align: right;
    }
    th {
      background-color: #e9ecef;
    }
    tr:nth-child(even) {
      background-color: #f8f9fa;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Memory Allocation Histogram</h1>
    <div class="summary">
      <strong>Total Blocks:</strong> )"
         << blocks << R"( <br/>
      <strong>Total Bytes:</strong> )"
         << total << R"( bytes
    </div>
    <table>
      <thead>
        <tr>
          <th>Size Class (Index)</th>
          <th>Block Count</th>
        </tr>
      </thead>
      <tbody>
)";

    for (int i = 0; i < kMallocHistogramSize; ++i) {
        if (histogram[i] > 0) {
            html << "        <tr><td>" << i << "</td><td>" << histogram[i] << "</td></tr>\n";
        }
    }

    html << R"(      </tbody>
    </table>
  </div>
</body>
</html>
)";

    response.setStatus(HttpStatus::OK);
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    response.setBody(html.str());
}

void PerformanceInspector::releaseFreeMemory(const HttpRequest& request, HttpResponse& response) {
    double releaseRate = MallocExtension::instance()->GetMemoryReleaseRate();
    MallocExtension::instance()->ReleaseFreeMemory();

    std::stringstream html;
    html << R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Memory Release</title>
  <style>
    body {
      font-family: "Segoe UI", sans-serif;
      background-color: #f4f6f8;
      padding: 2rem;
    }
    .card {
      background: #ffffff;
      border-radius: 10px;
      padding: 2rem;
      max-width: 600px;
      margin: auto;
      box-shadow: 0 0 12px rgba(0,0,0,0.1);
    }
    h1 {
      color: #0d6efd;
      font-size: 1.8rem;
      margin-bottom: 1rem;
    }
    p {
      font-size: 1.1rem;
      color: #333;
    }
    .highlight {
      font-weight: bold;
      color: #28a745;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>Free Memory Released</h1>
    <p><span class="highlight">Memory Release Rate:</span> )"
         << releaseRate << R"(</p>
    <p>✅ All free memory has been successfully released.</p>
  </div>
</body>
</html>
)";

    response.setStatus(HttpStatus::OK);
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    response.setBody(html.str());
}

}  // namespace zmuduo::inspect