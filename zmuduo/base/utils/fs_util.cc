#include "fs_util.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace zmuduo::utils::fs_util {
bool Exists(const fs::path& path) {
    return fs::exists(path);
}

bool CreateDirectories(const fs::path& dir) {
    std::error_code ec;
    const bool      success = fs::create_directories(dir, ec);
    return success && !ec;
}

bool Delete(const fs::path& path) {
    std::error_code ec;
    fs::remove_all(path, ec);
    return !ec;
}

bool Copy(const fs::path& src, const fs::path& dst, const bool overwrite) {
    std::error_code ec;
    const auto options = overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none;
    fs::copy(src, dst, options | fs::copy_options::recursive, ec);
    return !ec;
}

bool Move(const fs::path& src, const fs::path& dst) {
    std::error_code ec;
    fs::rename(src, dst, ec);
    if (ec) {
        // 跨设备移动需要复制后删除
        if (Copy(src, dst, true)) {
            return Delete(src);
        }
        return false;
    }
    return true;
}

uintmax_t GetFileSize(const fs::path& file) {
    std::error_code ec;
    const auto      size = fs::file_size(file, ec);
    return ec ? 0 : size;
}

bool WriteText(const fs::path& file, const std::string& content) {
    std::ofstream ofs(file, std::ios::out | std::ios::trunc);
    if (!ofs)
        return false;
    ofs << content;
    return !ofs.fail();
}

std::vector<uint8_t> ReadBinary(const fs::path& file) {
    std::ifstream ifs(file, std::ios::binary | std::ios::ate);
    if (!ifs) {
        return {};
    }
    const auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(size);
    ifs.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

bool WriteBinary(const fs::path& file, const std::vector<uint8_t>& data) {
    std::ofstream ofs(file, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        return false;
    }
    ofs.write(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));
    return !ofs.fail();
}

std::time_t GetLastModifiedTime(const fs::path& path) {
    const auto ftime = fs::last_write_time(path);
    // 转换为 system_clock::time_point
    const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
}

std::vector<fs::path> ListFiles(const fs::path& dir, const bool recursive) {
    std::vector<fs::path> files;
    if (!fs::exists(dir))
        return files;
    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            files.emplace_back(entry.path());
        }
    } else {
        for (const auto& entry : fs::directory_iterator(dir)) {
            files.emplace_back(entry.path());
        }
    }
    return files;
}

fs::path NormalizePath(const fs::path& path) {
    return path.lexically_normal();
}

std::string ReadText(const fs::path& file, const std::optional<size_t> maxBytes) {
    std::ifstream ifs(file, std::ios::in | std::ios::binary); // 二进制模式确保准确计数
    if (!ifs) {
        return {};
    }
    // 如果未指定最大字节数，读取全部内容
    if (!maxBytes.has_value()) {
        return {std::istreambuf_iterator(ifs), std::istreambuf_iterator<char>()};
    }

    // 限制读取长度
    std::string content;
    const int   maxSize = maxBytes.has_value() ?
                            static_cast<int>(maxBytes.value()) :
                            static_cast<int>(GetFileSize(file));
    content.resize(maxSize);
    ifs.read(content.data(), maxSize);
    content.resize(ifs.gcount()); // 实际读取的字节数
    return content;
}

fs::path GetDirectory(const fs::path& path) {
    return path.parent_path(); // 直接使用 std::filesystem 的 parent_path()
}

fs::path GetName(const fs::path& path) {
    return path.filename(); // 直接使用 std::filesystem 的 filename()
}
} // namespace zmuduo::utils::fs_util