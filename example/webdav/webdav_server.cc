#include "webdav_server.h"
#ifdef ZMUDUO_FIND_ZLIB
#    include "compress_filter.h"
#endif
#include "tinyxml2.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/base/utils/fs_util.h"
#include "zmuduo/base/utils/hash_util.h"
#include "zmuduo/base/utils/string_util.h"
#include <chrono>
#include <fstream>
#include <iomanip>

namespace fs = std::filesystem;
using namespace zmuduo::utils;
using namespace tinyxml2;

namespace {
bool isValidLinuxPath(const std::string& path) {
    // Linux 路径必须以 '/' 开头，不能包含 Windows 驱动器号、反斜杠等
    if (path.empty())
        return false;
    if (path[0] != '/')
        return false;
    if (path.find('\\') != std::string::npos)
        return false;
    return true;
}

// 生成符合 WebDAV 标准的 Last-Modified 时间字符串
std::string getWebdavLastModified(const fs::path& path) {
    std::time_t time = zmuduo::utils::FSUtil::GetLastModifiedTime(path);

    std::tm tm{};
    gmtime_r(&time, &tm);  // 使用 gmtime_r（线程安全）或 gmtime（非线程安全）

    std::ostringstream oss;
    oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
    return oss.str();
}

std::string getMimeType(const std::string& extension) {
    static const std::unordered_map<std::string, std::string> mimeTable = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"txt", "text/plain"},
        {"css", "text/css"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"png", "image/png"},
        {"gif", "image/gif"},
        {"svg", "image/svg+xml"},
        {"webp", "image/webp"},
        {"json", "application/json"},
        {"js", "application/javascript"},
        {"xml", "application/xml"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"tar", "application/x-tar"},
        {"gz", "application/gzip"},
        {"mp4", "video/mp4"},
        {"mp3", "audio/mpeg"},
        {"wav", "audio/wav"},
        {"avi", "video/x-msvideo"},
        {"mov", "video/quicktime"},
        {"doc", "application/msword"},
        {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {"xls", "application/vnd.ms-excel"},
        {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {"ppt", "application/vnd.ms-powerpoint"},
        {"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        {"bin", "application/octet-stream"}};

    auto it = mimeTable.find(extension);
    return it != mimeTable.end() ? it->second : "application/octet-stream";
}

bool isImageFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file)
        return false;

    std::vector<unsigned char> header(12);  // 取前 12 个字节就足够识别主流格式
    file.read(reinterpret_cast<char*>(header.data()), static_cast<int>(header.size()));

    // JPEG: FF D8 FF
    if (header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF) {
        return true;
    }

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    if (header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47 &&
        header[4] == 0x0D && header[5] == 0x0A && header[6] == 0x1A && header[7] == 0x0A) {
        return true;
    }

    // GIF87a or GIF89a
    if (header[0] == 'G' && header[1] == 'I' && header[2] == 'F' && header[3] == '8' &&
        (header[4] == '7' || header[4] == '9') && header[5] == 'a') {
        return true;
    }

    // BMP: 42 4D
    if (header[0] == 0x42 && header[1] == 0x4D) {
        return true;
    }

    // WEBP: RIFF....WEBP
    if (header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F' &&
        header[8] == 'W' && header[9] == 'E' && header[10] == 'B' && header[11] == 'P') {
        return true;
    }

    return false;
}

// 统一添加资源节点函数
void addResourceXml(tinyxml2::XMLDocument&     doc,
                    tinyxml2::XMLElement*      multistatus,
                    const fs::directory_entry& entry,
                    const std::string&         baseRequestPath,
                    const std::string&         relUri) {
    tinyxml2::XMLElement* response = doc.NewElement("D:response");
    // 路径
    tinyxml2::XMLElement* href = doc.NewElement("D:href");
    href->SetText(
        StringUtil::UrlEncode(fs::path(baseRequestPath + relUri).lexically_normal()).c_str());
    response->InsertEndChild(href);

    tinyxml2::XMLElement* propstat = doc.NewElement("D:propstat");
    tinyxml2::XMLElement* prop     = doc.NewElement("D:prop");

    tinyxml2::XMLElement* displayname = doc.NewElement("D:displayname");
    displayname->SetText(entry.path().filename().c_str());
    prop->InsertEndChild(displayname);

    tinyxml2::XMLElement* resourcetype = doc.NewElement("D:resourcetype");
    if (fs::is_directory(entry.path())) {
        tinyxml2::XMLElement* collection = doc.NewElement("D:collection");
        resourcetype->InsertEndChild(collection);
    }
    prop->InsertEndChild(resourcetype);

    if (fs::is_regular_file(entry.path())) {
        tinyxml2::XMLElement* contentlength = doc.NewElement("D:getcontentlength");
        contentlength->SetText(std::to_string(fs::file_size(entry.path())).c_str());
        prop->InsertEndChild(contentlength);
    }

    tinyxml2::XMLElement* lastModifiedTime = doc.NewElement("D:getlastmodified");
    lastModifiedTime->SetText(getWebdavLastModified(entry.path()).c_str());
    prop->InsertEndChild(lastModifiedTime);

    propstat->InsertEndChild(prop);

    tinyxml2::XMLElement* status = doc.NewElement("D:status");
    status->SetText("HTTP/1.1 200 OK");
    propstat->InsertEndChild(status);

    response->InsertEndChild(propstat);
    multistatus->InsertEndChild(response);
}

// 递归添加资源（Depth=Infinity）
void addResourceRecursive(tinyxml2::XMLDocument& doc,
                          tinyxml2::XMLElement*  multistatus,
                          const HttpRequest&     request,
                          const fs::path&        basePath,
                          const fs::path&        currentPath,
                          const std::string&     baseRequestPath) {
    fs::directory_entry entry(currentPath);
    std::string         relUri = fs::relative(currentPath, basePath).generic_string();
    if (!relUri.empty() && relUri[0] != '/')
        relUri = "/" + relUri;
    ZMUDUO_LOG_WARNING << "准备访问: " << relUri;
    addResourceXml(doc, multistatus, entry, baseRequestPath, relUri);

    if (fs::is_directory(currentPath)) {
        for (auto& subentry : fs::directory_iterator(currentPath)) {
            addResourceRecursive(doc, multistatus, request, basePath, subentry.path(),
                                 baseRequestPath);
        }
    }
}

}  // namespace

WebDavServer::WebDavServer(EventLoop*          loop,
                           const Address::Ptr& listenAddress,
                           const std::string&  sharePath,
                           const std::string&  prefixPath,
                           std::string         username,
                           std::string         password)
    : m_server(loop, listenAddress, "WebDavServer"),
      m_username(std::move(username)),
      m_password(std::move(password)),
      m_maxChunkSize(50 * 1024 * 1024) {
    assert(isValidLinuxPath(sharePath));
    assert(isValidLinuxPath(prefixPath));
    m_sharePath  = fs::path(sharePath).lexically_normal();
    m_prefixPath = fs::path(prefixPath).lexically_normal();
    // 初始化
    auto& dispatcher = m_server.getServletDispatcher();
    // propfind
#define APP_DISPATCHER(Path, Method, Func)                                                         \
    do {                                                                                           \
        dispatcher.addWildcardServlet(ServletKey{Path, Method},                                    \
                                      [this](const HttpRequest& request, HttpResponse& response) { \
                                          Func(request, response);                                 \
                                      });                                                          \
    } while (false)
    // 不同METHOD的回调
    APP_DISPATCHER(m_prefixPath + "/*", HttpMethod::PROPFIND, handlePropfind);
    APP_DISPATCHER(m_prefixPath + "/*", HttpMethod::MKCOL, handleMkcol);
    APP_DISPATCHER(m_prefixPath + "/*", HttpMethod::PUT, handlePut);
    APP_DISPATCHER(m_prefixPath + "/*", HttpMethod::DELETE, handleDelete);
    APP_DISPATCHER(m_prefixPath + "/*", HttpMethod::GET, handleGet);
    APP_DISPATCHER(m_prefixPath + "/*", HttpMethod::HEAD, handleHead);
    APP_DISPATCHER(m_prefixPath + "/*", HttpMethod::OPTIONS, handleOptions);
    APP_DISPATCHER(m_prefixPath + "/*", HttpMethod::MOVE, handleMove);
    APP_DISPATCHER(m_prefixPath + "/*", HttpMethod::COPY, handleCopy);
    // 首页
    dispatcher.addExactServlet(ServletKey{m_prefixPath, HttpMethod::GET},
                               [this](const HttpRequest& request, HttpResponse& response) {
                                   handleGet(request, response);
                               });
#undef APP_DISPATCHER
    // 拦截器,针对认证拦截
    dispatcher.addInterceptor("authenticate", [this](auto& request, auto& response) {
        return checkAuth(request, response);
    });
    // 过滤器,打印下request
    dispatcher.addFilter("logFilter", [](auto& request) { ZMUDUO_LOG_DEBUG << request; }, nullptr);
#ifdef ZMUDUO_FIND_ZLIB
    // 过滤器,对于gzip、deflate进行压缩
    dispatcher.addFilter(std::make_shared<CompressFilter>());
#endif
}

bool WebDavServer::checkBasicAuth(const HttpRequest& request) {
    // 如果账号密码有一个没有设置
    if (m_username.empty() || m_password.empty()) {
        return true;
    }
    // 获取authorization
    auto auth = request.getHeader("Authorization");
    if (auth.substr(0, 6) != "Basic ") {
        return false;
    }
    // 对除去basic后的字符串进行base64解码
    std::string encoded = auth.substr(6);
    std::string decoded = HashUtil::Base64decode(encoded);
    // 查找：分割用户名和密码
    auto sep = decoded.find(':');
    if (sep == std::string::npos) {
        return false;
    }
    // 获取用户传递的用户名密码
    std::string user = decoded.substr(0, sep);
    std::string pass = decoded.substr(sep + 1);
    return user == m_username && pass == m_password;
}

bool WebDavServer::checkAuth(const HttpRequest& request, HttpResponse& response) {
    if (!checkBasicAuth(request)) {
        response.setStatus(HttpStatus::UNAUTHORIZED);
        response.setHeader("WWW-Authenticate", "Basic realm=\"WebDAV\"");
        response.setBody("Unauthorized");
        return false;
    }
    return true;
}

void WebDavServer::handleOptions(const HttpRequest& request, HttpResponse& response) {
    response.setStatus(HttpStatus::OK);
    response.setHeader("Allow", "OPTIONS, GET, PUT, POST, DELETE, MKCOL, PROPFIND, HEAD");
    response.setHeader("DAV", "1,2");
    response.setHeader("MS-Author-Via", "DAV");
}

void WebDavServer::handlePropfind(const HttpRequest& request, HttpResponse& response) {
    // 获取想访问的资源名称
    fs::path fullPath = fs::weakly_canonical(
                            fs::path(m_sharePath) /
                            fs::path(request.getPath().substr(m_prefixPath.size())).relative_path())
                            .lexically_normal();
    ZMUDUO_LOG_DEBUG << fullPath;
    if (!fs::exists(fullPath)) {
        setDavError(response, HttpStatus::NOT_FOUND, "The requested resource does not exist.");
        return;
    }
    // 解析xml
    XMLDocument     doc;
    XMLDeclaration* decl = doc.NewDeclaration();
    doc.InsertFirstChild(decl);
    // 生成要返回的xml结构
    XMLElement* multistatus = doc.NewElement("D:multistatus");
    multistatus->SetAttribute("xmlns:D", "DAV:");
    doc.InsertEndChild(multistatus);
    // 解析depth属性
    WebDavDepth depth = parseDepthHeader(request);

    if (depth == WebDavDepth::Zero) {
        fs::directory_entry entry(fullPath);
        addResourceXml(doc, multistatus, entry, request.getPath(), "/");
    } else if (depth == WebDavDepth::One) {
        fs::directory_entry rootEntry(fullPath);
        addResourceXml(doc, multistatus, rootEntry, request.getPath(), "/");
        // 如果是一个目录
        if (fs::is_directory(fullPath)) {
            for (auto& entry : fs::directory_iterator(fullPath)) {
                std::string relPath = "/" + entry.path().filename().string();
                addResourceXml(doc, multistatus, entry, request.getPath(), relPath);
            }
        }
    } else {  // Depth::Infinity
        addResourceRecursive(doc, multistatus, request, fullPath, fullPath, request.getPath());
    }

    XMLPrinter printer;
    doc.Print(&printer);
    response.setStatus(HttpStatus::OK);
    response.setReason("Multi-Status");
    response.setContentType("text/xml; charset=utf-8");
    response.setHeader("DAV", "1,2");
    response.setHeader("Depth", request.getHeader("Depth").empty() ? "Infinity" :
                                                                     request.getHeader("Depth"));
    response.setBody(printer.CStr());
}

void WebDavServer::handleMkcol(const HttpRequest& request, HttpResponse& response) {
    // 获取想访问的资源名称
    fs::path fullPath = fs::weakly_canonical(
                            fs::path(m_sharePath) /
                            fs::path(request.getPath().substr(m_prefixPath.size())).relative_path())
                            .lexically_normal();
    ZMUDUO_LOG_DEBUG << fullPath;
    if (fs::create_directories(fullPath)) {
        response.setStatus(HttpStatus::CREATED);
    } else {
        response.setStatus(HttpStatus::METHOD_NOT_ALLOWED);
    }
}

void WebDavServer::handlePut(const HttpRequest& request, HttpResponse& response) {
    // 拼接物理路径
    fs::path fullPath = fs::weakly_canonical(
                            fs::path(m_sharePath) /
                            fs::path(request.getPath().substr(m_prefixPath.size())).relative_path())
                            .lexically_normal();
    ZMUDUO_LOG_DEBUG << fullPath;
    // 检查路径是否在允许范围内（防止路径穿越）
    if (fullPath.string().find(m_sharePath) != 0) {
        setDavError(response, HttpStatus::FORBIDDEN);
        return;
    }

    // 检查父目录是否存在
    fs::path parent = fullPath.parent_path();
    if (!fs::exists(parent)) {
        setDavError(response, HttpStatus::CONFLICT, "Parent directory does not exist");
        return;
    }

    // 写入文件内容（覆盖）
    std::ofstream ofs(fullPath, std::ios::binary);
    if (!ofs.is_open()) {
        setDavError(response, HttpStatus::INTERNAL_SERVER_ERROR, "Failed to open file");
        return;
    }

    const auto& body = request.getBody();  // 获取 PUT 请求体
    ofs.write(body.data(), static_cast<int>(body.size()));
    ofs.close();

    // 判断是否是新建还是覆盖
    if (fs::exists(fullPath) && fs::file_size(fullPath) == body.size()) {
        response.setStatus(HttpStatus::NO_CONTENT);  // 覆盖
    } else {
        response.setStatus(HttpStatus::CREATED);  // 新建
    }
    response.setReason("OK");
}

void WebDavServer::handleDelete(const HttpRequest& request, HttpResponse& response) {
    fs::path fullPath = fs::weakly_canonical(
                            fs::path(m_sharePath) /
                            fs::path(request.getPath().substr(m_prefixPath.size())).relative_path())
                            .lexically_normal();
    ZMUDUO_LOG_DEBUG << fullPath;
    if (fullPath.string().find(m_sharePath) != 0) {
        setDavError(response, HttpStatus::FORBIDDEN);
        return;
    }

    std::error_code ec;
    if (!fs::exists(fullPath)) {
        response.setStatus(HttpStatus::NOT_FOUND);
    } else if (fs::is_directory(fullPath)) {
        fs::remove_all(fullPath, ec);
        response.setStatus(ec ? HttpStatus::INTERNAL_SERVER_ERROR : HttpStatus::NO_CONTENT);
    } else {
        fs::remove(fullPath, ec);
        response.setStatus(ec ? HttpStatus::INTERNAL_SERVER_ERROR : HttpStatus::NO_CONTENT);
    }
}

void WebDavServer::handleGet(const HttpRequest& request, HttpResponse& response) {
    // 获取本地资源路径
    fs::path fullPath = fs::weakly_canonical(
                            fs::path(m_sharePath) /
                            fs::path(request.getPath().substr(m_prefixPath.size())).relative_path())
                            .lexically_normal();

    ZMUDUO_LOG_DEBUG << "handleGet: " << fullPath;

    if (fullPath.string().find(m_sharePath) != 0) {
        setDavError(response, HttpStatus::FORBIDDEN);
        return;
    }

    if (!fs::exists(fullPath)) {
        setDavError(response, HttpStatus::NOT_FOUND);
        return;
    }
    // 如果是GET一个目录,则进入propfind,兼容浏览器
    if (fs::is_directory(fullPath)) {
        handlePropfind(request, response);
        return;
    }
    // 打开文件
    std::ifstream ifs(fullPath, std::ios::binary);
    if (!ifs.is_open()) {
        setDavError(response, HttpStatus::INTERNAL_SERVER_ERROR);
        return;
    }

    const size_t fileSize = fs::file_size(fullPath);
    // 获取要下载的范围
    size_t start = 0, end = fileSize - 1;
    bool   isRange = false;

    // 判断是否为range请求
    std::string rangeHeader = request.getHeader("Range");
    if (!rangeHeader.empty() && rangeHeader.find("bytes=") == 0) {
        isRange                = true;
        std::string rangeValue = rangeHeader.substr(6);  // after "bytes="
        auto        dash       = rangeValue.find('-');
        // 获取range代表的起始和终止字节
        if (dash != std::string::npos) {
            std::string startStr = rangeValue.substr(0, dash);
            std::string endStr   = rangeValue.substr(dash + 1);
            try {
                if (!startStr.empty())
                    start = std::stoull(startStr);
                if (!endStr.empty())
                    end = std::stoull(endStr);
                else {
                    // 如果end为空,一次最多发送maxChunkSize字节
                    end = std::min(start + m_maxChunkSize - 1, fileSize - 1);
                }
            } catch (...) {
                setDavError(response, HttpStatus::BAD_REQUEST);
                return;
            }
        }

        if (start > end || end >= fileSize) {
            response.setStatus(HttpStatus::RANGE_NOT_SATISFIABLE);
            response.setHeader("Content-Range", "bytes */" + std::to_string(fileSize));
            return;
        }
    } else if (fileSize > m_maxChunkSize) {
        // 自动切片：如果是图片默认返回前 m_maxChunkSize MB,否则返回2字节
        start   = 0;
        end     = m_maxChunkSize - 1;
        isRange = true;
    }
    // 读取文件内容
    size_t      contentLength = end - start + 1;
    std::string content(contentLength, '\0');
    ifs.seekg(static_cast<std::streamoff>(start));
    ifs.read(&content[0], static_cast<std::streamsize>(contentLength));
    if (ifs.gcount() != static_cast<std::streamsize>(contentLength)) {
        setDavError(response, HttpStatus::INTERNAL_SERVER_ERROR);
        return;
    }

    response.setBody(content);

    // 获取扩展名并查 MIME 类型
    std::string mimeType = "application/octet-stream";
    auto        ext      = fullPath.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);  // 去掉前导点
    }
    if (!ext.empty()) {
        mimeType = getMimeType(ext);
    }
    // 设置内容类型
    response.setContentType(mimeType);
    // 根据是否为range设置状态
    if (isRange) {
        response.setStatus(HttpStatus::PARTIAL_CONTENT);  // 206
        response.setHeader("Content-Range", "bytes " + std::to_string(start) + "-" +
                                                std::to_string(end) + "/" +
                                                std::to_string(fileSize));
    } else {
        response.setStatus(HttpStatus::OK);
    }
    response.setHeader("Last-Modified", getWebdavLastModified(fullPath));
    response.setHeader("Accept-Ranges", "bytes");
}

void WebDavServer::handleHead(const HttpRequest& request, HttpResponse& response) {
    fs::path fullPath = fs::weakly_canonical(
                            fs::path(m_sharePath) /
                            fs::path(request.getPath().substr(m_prefixPath.size())).relative_path())
                            .lexically_normal();
    ZMUDUO_LOG_DEBUG << fullPath;
    if (!fs::exists(fullPath) || fs::is_directory(fullPath)) {
        setDavError(response, HttpStatus::NOT_FOUND);
        return;
    }

    response.setStatus(HttpStatus::OK);
    response.setHeader("content-length", std::to_string(fs::file_size(fullPath)));
    response.setContentType("application/octet-stream");
}

void WebDavServer::handleCopy(const HttpRequest& request, HttpResponse& response) {
    const std::string& destHeader = request.getHeader("Destination");
    if (destHeader.empty()) {
        setDavError(response, HttpStatus::BAD_REQUEST, "Missing Destination header");
        return;
    }
    // 源文件路径
    fs::path srcPath =
        fs::weakly_canonical(fs::path(m_sharePath) / fs::path(request.getPath()).relative_path());
    // 目标文件路径
    fs::path destPath = getDestinationPath(destHeader);
    if (destPath.empty() || !fs::exists(srcPath)) {
        setDavError(response, HttpStatus::BAD_REQUEST, "Invalid source or destination");
        return;
    }

    // Depth
    WebDavDepth depth = parseDepthHeader(request);
    bool        isDir = fs::is_directory(srcPath);
    if (depth == WebDavDepth::Zero && isDir) {
        // Only create an empty dir (no children)
        std::error_code ec;
        fs::create_directories(destPath, ec);
        if (ec) {
            setDavError(response, HttpStatus::INTERNAL_SERVER_ERROR,
                        "Failed to create empty directory");
            return;
        }
        response.setStatus(HttpStatus::CREATED);
        return;
    }

    // 是否覆盖
    bool               overwrite = true;
    const std::string& ov        = request.getHeader("Overwrite");
    if (!ov.empty()) {
        overwrite = (ov == "T" || ov == "t" || ov == "true" || ov == "True");
    }
    // 如果目标文件存在且不需要覆盖
    if (fs::exists(destPath) && !overwrite) {
        setDavError(response, HttpStatus::PRECONDITION_FAILED,
                    "Target Exists and overwrite is false");
        return;
    }
    // 拷贝
    std::error_code ec;
    if (isDir) {
        fs::copy(srcPath, destPath,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    } else {
        fs::copy_file(srcPath, destPath,
                      overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none,
                      ec);
    }
    // 根据是否有错误进行回应
    if (ec) {
        setDavError(response, HttpStatus::INTERNAL_SERVER_ERROR, "Copy failed: " + ec.message());
    } else {
        response.setStatus(HttpStatus::CREATED);
    }
}

void WebDavServer::handleMove(const HttpRequest& request, HttpResponse& response) {
    // 调用拷贝
    handleCopy(request, response);

    if (static_cast<int>(response.getStatus()) >= 300) {
        return;  // 失败就放弃删除
    }
    const std::string& destHeader = request.getHeader("Destination");
    if (destHeader.empty())
        return;
    // 构建源文件路径
    fs::path srcPath =
        fs::weakly_canonical(fs::path(m_sharePath) / fs::path(request.getPath()).relative_path());
    std::error_code ec;
    fs::remove_all(srcPath, ec);
    if (ec) {
        ZMUDUO_LOG_ERROR << m_server.getName() << " Move failed because: " << ec;
    }
}

fs::path WebDavServer::getDestinationPath(const std::string& requestPath) const {
    const auto& destPath = requestPath;

    // 例如：Destination: http://host:port/webdav/dir/file
    size_t pos = destPath.find(m_prefixPath);
    if (pos == std::string::npos) {
        return {};
    }
    // 获取相对路径
    std::string relativePath = destPath.substr(pos + m_prefixPath.size());
    relativePath             = fs::path(relativePath).lexically_normal();
    // 构建完整的目标路径
    fs::path fullPath = fs::weakly_canonical(fs::path(m_sharePath) / relativePath);
    if (fullPath.string().find(m_sharePath) != 0) {
        return {};
    }

    return fullPath;
}

void WebDavServer::setDavError(HttpResponse&      response,
                               HttpStatus         status,
                               const std::string& message) {
    XMLDocument     doc;
    XMLDeclaration* decl = doc.NewDeclaration();
    doc.InsertFirstChild(decl);

    XMLElement* errorElem = doc.NewElement("D:error");
    errorElem->SetAttribute("xmlns:D", "DAV:");
    doc.InsertEndChild(errorElem);

    XMLElement* statusElem = doc.NewElement("D:status");
    std::string statusLine =
        "HTTP/1.1 " + std::to_string(static_cast<int>(status)) + " " + HttpStatusToString(status);
    statusElem->SetText(statusLine.c_str());
    errorElem->InsertEndChild(statusElem);

    if (!message.empty()) {
        XMLElement* msgElem = doc.NewElement("D:message");
        msgElem->SetText(message.c_str());
        errorElem->InsertEndChild(msgElem);
    }

    XMLPrinter printer;
    doc.Print(&printer);

    response.setStatus(status);
    response.setReason(HttpStatusToString(status));
    response.setContentType("text/xml; charset=utf-8");
    response.setBody(printer.CStr());
}
