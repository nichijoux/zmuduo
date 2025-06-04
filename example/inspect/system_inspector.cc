#include "system_inspector.h"
#include "zmuduo/base/utils/string_util.h"
#include <iomanip>
#include <sys/statfs.h>
#include <sys/utsname.h>
#include <zmuduo/base/utils/fs_util.h>

using namespace zmuduo::utils;

namespace zmuduo::inspect {
std::string uptime(const Timestamp& now, const Timestamp& start, bool showMicroseconds) {
    char    buf[256];
    int64_t age     = now.getMicroSecondsSinceEpoch() - start.getMicroSecondsSinceEpoch();
    int     seconds = static_cast<int>(age / Timestamp::S_MICRO_SECONDS_PER_SECOND);
    int     days    = seconds / 86400;
    int     hours   = (seconds % 86400) / 3600;
    int     minutes = (seconds % 3600) / 60;
    if (showMicroseconds) {
        int microseconds = static_cast<int>(age % Timestamp::S_MICRO_SECONDS_PER_SECOND);
        snprintf(buf, sizeof buf, "%d days %02d:%02d:%02d.%06d", days, hours, minutes, seconds % 60,
                 microseconds);
    } else {
        snprintf(buf, sizeof buf, "%d days %02d:%02d:%02d", days, hours, minutes, seconds % 60);
    }
    return buf;
}

long getLong(const std::string& procStatus, const char* key) {
    long   result = 0;
    size_t pos    = procStatus.find(key);
    if (pos != std::string::npos) {
        result = lexical_cast<long>(procStatus.c_str() + pos + strlen(key));
    }
    return result;
}

SystemInspector::SystemInspector() : Servlet("SystemInspector") {
    m_dispatcher.addExactServlet({"/loadavg", HttpMethod::GET},
                                 [](auto& req, auto& rep) { SystemInspector::loadavg(req, rep); });
    m_dispatcher.addExactServlet(
        "/version", [](auto& req, auto& rep) { SystemInspector::version(req, rep); },
        HttpMethod::GET);
    m_dispatcher.addExactServlet(
        "/cpuinfo", [](auto& req, auto& rep) { SystemInspector::cpuinfo(req, rep); },
        HttpMethod::GET);
    m_dispatcher.addExactServlet(
        "/meminfo", [](auto& req, auto& rep) { SystemInspector::meminfo(req, rep); },
        HttpMethod::GET);
    m_dispatcher.addExactServlet(
        "/stat", [](auto& req, auto& rep) { SystemInspector::stat(req, rep); }, HttpMethod::GET);
    m_dispatcher.addExactServlet(
        {"/overview"}, [](auto& req, auto& rep) { SystemInspector::overview(req, rep); },
        HttpMethod::GET);
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

void SystemInspector::handle(const HttpRequest& request, HttpResponse& response) {
    m_dispatcher.handle(const_cast<HttpRequest&>(request), response);
}

void SystemInspector::loadavg(const HttpRequest& request, HttpResponse& response) {
    std::string content = FSUtil::ReadText("/proc/loadavg", 65536);

    // /proc/loadavg 内容格式：
    // 0.15 0.17 0.16 1/456 12345
    std::istringstream iss(content);
    std::string        one, five, fifteen, running, lastpid;
    iss >> one >> five >> fifteen >> running >> lastpid;

    std::stringstream html;
    html << R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>System Load Average</title>
  <style>
    body {
      font-family: "Segoe UI", sans-serif;
      background: #f0f4f8;
      color: #333;
      padding: 2em;
    }
    .container {
      max-width: 600px;
      margin: auto;
      background: white;
      padding: 2em;
      border-radius: 10px;
      box-shadow: 0 4px 10px rgba(0,0,0,0.1);
    }
    h1 {
      color: #007acc;
      text-align: center;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      margin-top: 1em;
    }
    td, th {
      padding: 12px 15px;
      border-bottom: 1px solid #ddd;
      text-align: left;
    }
    th {
      background-color: #f7f7f7;
      color: #444;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>System Load Average</h1>
    <table>
      <tr><th>1 Minute</th><td>)"
         << one << R"(</td></tr>
      <tr><th>5 Minutes</th><td>)"
         << five << R"(</td></tr>
      <tr><th>15 Minutes</th><td>)"
         << fifteen << R"(</td></tr>
      <tr><th>Running Processes</th><td>)"
         << running << R"(</td></tr>
      <tr><th>Last PID</th><td>)"
         << lastpid << R"(</td></tr>
    </table>
  </div>
</body>
</html>
)";

    response.setStatus(HttpStatus::OK);
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    response.setBody(html.str());
}

void SystemInspector::version(const HttpRequest& request, HttpResponse& response) {
    std::string content = FSUtil::ReadText("/proc/version", 65536);

    std::stringstream result;
    result << R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Kernel Version</title>
  <style>
    body {
      font-family: "Segoe UI", sans-serif;
      background-color: #f9f9f9;
      padding: 3em;
      color: #333;
    }
    .container {
      background-color: #fff;
      padding: 2em 2.5em;
      border-radius: 10px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.1);
      max-width: 800px;
      margin: auto;
    }
    h1 {
      text-align: center;
      color: #007acc;
    }
    .version-text {
      font-family: monospace;
      background-color: #f4f4f4;
      padding: 1em;
      border-radius: 6px;
      white-space: pre-wrap;
      word-break: break-word;
      border: 1px solid #ddd;
      margin-top: 1.5em;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Linux Kernel Version</h1>
    <div class="version-text">)"
           << content << R"(</div>
  </div>
</body>
</html>
)";

    response.setStatus(HttpStatus::OK);
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    response.setBody(result.str());
}

void SystemInspector::cpuinfo(const HttpRequest& request, HttpResponse& response) {
    std::string              content = FSUtil::ReadText("/proc/cpuinfo", 65536);
    std::vector<std::string> lines   = StringUtil::Split(content, '\n');

    std::stringstream result;
    result << R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>CPU Info</title>
  <style>
    body {
      font-family: "Segoe UI", sans-serif;
      background-color: #f4f4f4;
      color: #333;
      padding: 2em;
    }
    h1 {
      text-align: center;
      color: #007acc;
    }
    .cpu-block {
      background: #fff;
      border-radius: 8px;
      box-shadow: 0 2px 6px rgba(0,0,0,0.1);
      padding: 1em 1.5em;
      margin: 2em auto;
      width: 60%;
    }
    table {
      width: 100%;
      border-collapse: collapse;
    }
    th, td {
      padding: 6px 10px;
      border-bottom: 1px solid #eee;
    }
    th {
      background-color: #f9f9f9;
      text-align: left;
    }
    tr:hover {
      background-color: #f2f2f2;
    }
    .label {
      font-weight: bold;
      font-family: monospace;
    }
    .value {
      font-family: monospace;
    }
  </style>
</head>
<body>
  <h1>CPU Information (/proc/cpuinfo)</h1>
)";

    std::vector<std::pair<std::string, std::string>> cpu_fields;
    int                                              cpu_index = 0;

    for (const auto& line : lines) {
        if (line.empty()) {
            // 新的处理器段
            if (!cpu_fields.empty()) {
                result << "<div class=\"cpu-block\">\n";
                result << "<h2>Processor " << cpu_index++ << "</h2>\n";
                result << "<table>\n";
                for (const auto& kv : cpu_fields) {
                    result << "<tr><td class='label'>" << kv.first << "</td><td class='value'>"
                           << kv.second << "</td></tr>\n";
                }
                result << "</table>\n</div>\n";
                cpu_fields.clear();
            }
            continue;
        }

        auto pos = line.find(':');
        if (pos == std::string::npos)
            continue;
        std::string key = StringUtil::Trim(line.substr(0, pos));
        std::string val = StringUtil::Trim(line.substr(pos + 1));
        cpu_fields.emplace_back(key, val);
    }

    // 最后一个逻辑 CPU（可能没有空行结尾）
    if (!cpu_fields.empty()) {
        result << "<div class=\"cpu-block\">\n";
        result << "<h2>Processor " << cpu_index++ << "</h2>\n";
        result << "<table>\n";
        for (const auto& kv : cpu_fields) {
            result << "<tr><td class='label'>" << kv.first << "</td><td class='value'>" << kv.second
                   << "</td></tr>\n";
        }
        result << "</table>\n</div>\n";
    }

    result << R"(
</body>
</html>
)";

    response.setStatus(HttpStatus::OK);
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    response.setBody(result.str());
}

void SystemInspector::meminfo(const HttpRequest& request, HttpResponse& response) {
    std::string              content = FSUtil::ReadText("/proc/meminfo", 65536);
    std::vector<std::string> lines   = StringUtil::Split(content, '\n');

    std::stringstream result;
    result << R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Memory Info</title>
  <style>
    body {
      font-family: "Segoe UI", sans-serif;
      background-color: #f9f9f9;
      color: #333;
      padding: 2em;
    }
    h1 {
      text-align: center;
      color: #007bff;
    }
    table {
      width: 60%;
      margin: 2em auto;
      border-collapse: collapse;
      background: #fff;
      box-shadow: 0 2px 6px rgba(0,0,0,0.1);
      border-radius: 8px;
      overflow: hidden;
    }
    th, td {
      padding: 12px 16px;
      border-bottom: 1px solid #eee;
    }
    th {
      background-color: #f2f2f2;
      text-align: left;
    }
    tr:hover {
      background-color: #f9f9f9;
    }
    .key {
      font-family: monospace;
      font-weight: bold;
    }
    .value {
      text-align: right;
      font-family: monospace;
    }
  </style>
</head>
<body>
  <h1>Memory Information (/proc/meminfo)</h1>
  <table>
    <tr><th>Key</th><th>Value</th></tr>
)";

    for (const auto& line : lines) {
        if (line.empty())
            continue;
        auto pos = line.find(':');
        if (pos == std::string::npos)
            continue;
        std::string key   = StringUtil::Trim(line.substr(0, pos));
        std::string value = StringUtil::Trim(line.substr(pos + 1));
        result << "<tr><td class='key'>" << key << "</td><td class='value'>" << value
               << "</td></tr>\n";
    }

    result << R"(
  </table>
</body>
</html>
)";

    response.setStatus(HttpStatus::OK);
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    response.setBody(result.str());
}

void SystemInspector::stat(const HttpRequest& request, HttpResponse& response) {
    std::string       content = FSUtil::ReadText("/proc/stat", 65536);
    std::stringstream result;

    std::vector<std::string> lines = StringUtil::Split(content, '\n');

    result << R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>/proc/stat Overview</title>
  <style>
    body { font-family: "Segoe UI", sans-serif; margin: 2em; background: #f9f9f9; color: #333; }
    h1 { text-align: center; color: #007bff; }
    .section { margin-bottom: 2em; background: #fff; border-radius: 8px; padding: 1em; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
    h2 { margin-top: 0; font-size: 1.2em; color: #444; }
    table { width: 100%; border-collapse: collapse; margin-top: 1em; }
    th, td { border: 1px solid #ccc; padding: 0.5em; text-align: left; }
    th { background: #f0f0f0; }
    .mono { font-family: monospace; white-space: pre-wrap; }
  </style>
</head>
<body>
  <h1>/proc/stat</h1>
  <div class="section">
    <h2>CPU Statistics</h2>
    <table>
      <tr><th>CPU</th><th>User</th><th>Nice</th><th>System</th><th>Idle</th><th>I/O Wait</th><th>IRQ</th><th>SoftIRQ</th><th>Steal</th><th>Guest</th><th>Guest Nice</th></tr>
)";

    // 输出 CPU 相关信息
    for (const auto& line : lines) {
        if (!StringUtil::StartsWith(line, "cpu"))
            break;
        auto tokens = StringUtil::Split(line, ' ');
        tokens.erase(std::remove_if(tokens.begin(), tokens.end(),
                                    [](const std::string& s) { return s.empty(); }),
                     tokens.end());
        if (tokens.size() < 5)
            continue;

        result << "<tr>";
        for (const auto& token : tokens) {
            result << "<td>" << token << "</td>";
        }
        // 如果字段不足，填充空列
        for (size_t i = tokens.size(); i < 11; ++i) {
            result << "<td>-</td>";
        }
        result << "</tr>\n";
    }

    result << R"(
    </table>
  </div>
)";

    // 输出其他键值对
    result << R"(<div class="section"><h2>Other Statistics</h2><table>)";
    result << "<tr><th>Key</th><th>Value(s)</th></tr>\n";

    for (const auto& line : lines) {
        if (line.empty() || StringUtil::StartsWith(line, "cpu"))
            continue;
        auto space_pos = line.find(' ');
        if (space_pos == std::string::npos)
            continue;
        std::string key   = line.substr(0, space_pos);
        std::string value = StringUtil::Trim(line.substr(space_pos));
        result << "<tr><td class=\"mono\">" << key << "</td><td class=\"mono\">" << value
               << "</td></tr>\n";
    }

    result << R"(</table></div></body></html>)";

    response.setStatus(HttpStatus::OK);
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    response.setBody(result.str());
}

void SystemInspector::overview(const HttpRequest& request, HttpResponse& response) {
    std::stringstream result;

    auto now = Timestamp::Now();

    // 获取基本信息
    std::string hostname, machine, os, version;
    {
        struct utsname un {};
        if (::uname(&un) == 0) {
            hostname = un.nodename;
            machine  = un.machine;
            os       = un.sysname;
            version  = std::string(un.release) + " " + un.version;
        }
    }

    std::string stat = FSUtil::ReadText("/proc/stat", 65536);
    Timestamp   bootTime(Timestamp::S_MICRO_SECONDS_PER_SECOND * getLong(stat, "btime "));
    std::string upTime = uptime(now, bootTime, false);

    std::string loadavg      = FSUtil::ReadText("/proc/loadavg", 65536);
    long        processCount = getLong(stat, "process ");

    auto meminfo    = FSUtil::ReadText("/proc/meminfo", 65536);
    long total_kb   = getLong(meminfo, "MemTotal:");
    long free_kb    = getLong(meminfo, "MemFree:");
    long buffers_kb = getLong(meminfo, "Buffers:");
    long cached_kb  = getLong(meminfo, "Cached:");

    long realUsed = (total_kb - free_kb - buffers_kb - cached_kb) / 1024;
    long realFree = (free_kb + buffers_kb + cached_kb) / 1024;

    auto mounts      = FSUtil::ReadText("/proc/mounts", 65536);
    auto mount_lines = StringUtil::Split(mounts, '\n');

    auto netdev    = FSUtil::ReadText("/proc/net/dev", 65536);
    auto net_lines = StringUtil::Split(netdev, '\n');

    // 构建 HTML 页面
    result << R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <title>System Overview</title>
  <style>
    body { font-family: "Segoe UI", sans-serif; margin: 2em; background: #f5f5f5; color: #333; }
    h1 { text-align: center; color: #007bff; }
    .section { margin-bottom: 2em; background: #fff; border-radius: 8px; padding: 1em; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
    h2 { margin-top: 0; }
    table { width: 100%; border-collapse: collapse; margin-top: 0.5em; }
    th, td { border: 1px solid #ccc; padding: 0.5em; text-align: left; }
    th { background: #e9ecef; }
    .mono { font-family: monospace; white-space: pre-wrap; }
  </style>
</head>
<body>
  <h1>System Overview</h1>
)";

    // 时间与系统信息
    result << "<div class='section'><h2>Time</h2>" << "<p><strong>Generated:</strong> "
           << now.toString() << "</p>" << "<p><strong>Boot Time:</strong> " << bootTime.toString()
           << "</p>" << "<p><strong>Up Time:</strong> " << upTime << "</p></div>";

    result << "<div class='section'><h2>System Info</h2>" << "<p><strong>Hostname:</strong> "
           << hostname << "</p>" << "<p><strong>Machine:</strong> " << machine << "</p>"
           << "<p><strong>OS:</strong> " << os << " " << version << "</p></div>";

    // CPU
    result << "<div class='section'><h2>CPU</h2>" << "<p><strong>Processes Created:</strong> "
           << processCount << "</p>" << "<p><strong>Load Average:</strong> "
           << StringUtil::Trim(loadavg) << "</p></div>";

    // Memory
    result << "<div class='section'><h2>Memory</h2><table>"
           << "<tr><th>Total</th><th>Buffers</th><th>Free</th><th>Real "
              "Used</th><th>Cached</th><th>Real Free</th></tr>"
           << "<tr><td>" << total_kb / 1024 << " MiB</td>" << "<td>" << buffers_kb / 1024
           << " MiB</td>" << "<td>" << free_kb / 1024 << " MiB</td>" << "<td>" << realUsed
           << " MiB</td>" << "<td>" << cached_kb / 1024 << " MiB</td>" << "<td>" << realFree
           << " MiB</td></tr></table></div>";

    // Disk
    result << "<div class='section'><h2>Disk Usage</h2><table>"
           << "<tr><th>Filesystem</th><th>Size</th><th>Used</th><th>Avail</th><th>Use%</"
              "th><th>Mounted on</th></tr>";
    for (const auto& line : mount_lines) {
        if (line.empty() || line.find("/dev/") != 0)
            continue;
        auto fields = StringUtil::Split(line, ' ');
        if (fields.size() < 2) {
            continue;
        }
        std::string device      = fields[0];
        std::string mount_point = fields[1];

        struct statfs fs {};
        if (::statfs(mount_point.c_str(), &fs) != 0) {
            continue;
        }

        uint64_t bs      = fs.f_bsize;
        uint64_t total   = fs.f_blocks * bs;
        uint64_t free    = fs.f_bfree * bs;
        uint64_t avail   = fs.f_bavail * bs;
        uint64_t used    = total - free;
        int      percent = total > 0 ? static_cast<int>((used * 100) / total) : 0;

        result << "<tr><td>" << device << "</td><td>" << total / (1024 * 1024) << " MB</td><td>"
               << used / (1024 * 1024) << " MB</td><td>" << avail / (1024 * 1024) << " MB</td><td>"
               << percent << "%</td><td>" << mount_point << "</td></tr>";
    }
    result << "</table></div>";

    // Network
    result << "<div class='section'><h2>Network Interfaces</h2><table>"
           << "<tr><th>Interface</th><th>RX (bytes)</th><th>TX (bytes)</th></tr>";
    for (const auto& line : net_lines) {
        if (line.empty() || line.find(":") == std::string::npos) {
            continue;
        }

        auto parts = StringUtil::Split(line, ':');
        if (parts.size() < 2) {
            continue;
        }
        std::string iface = StringUtil::Trim(parts[0]);
        std::string stats = StringUtil::Trim(parts[1]);

        auto fields = StringUtil::Split(stats, ' ');
        if (fields.size() < 16) {
            continue;
        }

        auto rx = lexical_cast<uint64_t>(fields[0]);
        auto tx = lexical_cast<uint64_t>(fields[8]);

        result << "<tr><td>" << iface << "</td><td>" << rx << "</td><td>" << tx << "</td></tr>";
    }
    result << "</table></div>";

    result << "</body></html>";

    // 设置响应
    response.setStatus(HttpStatus::OK);
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    response.setBody(result.str());
}

}  // namespace zmuduo::inspect
