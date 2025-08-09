// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_BASE_UTILS_FS_UTIL_H
#define ZMUDUO_BASE_UTILS_FS_UTIL_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/base/nomoveable.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/**
 * @namespace fs_util
 * @brief 文件系统工具空间，提供静态方法操作文件和目录。
 *
 * fs_util 基于 std::filesystem
 * 提供文件系统操作，包括文件/目录的创建、删除、复制、移动、读写等功能。
 * 所有方法为静态方法，类不可实例化、拷贝或移动。
 *
 * @note 方法线程安全，依赖 std::filesystem 的实现，但大文件操作可能影响性能。
 * @note 使用 std::error_code 处理错误，确保异常安全。
 *
 * @example
 *
 * @code
 * #include "zmuduo/base/utils/fs_util.h"
 * #include <iostream>
 *
 * int main() {
 *     std::filesystem::path dir = "test_dir";
 *     std::filesystem::path file = dir / "test.txt";
 *     // 创建目录
 *     if (fs_util::CreateDirectories(dir)) {
 *         std::cout << "Directory created" << std::endl;
 *     }
 *     // 写入文本
 *     fs_util::WriteText(file, "Hello, World!");
 *     // 读取文本
 *     std::cout << "Content: " << fs_util::ReadText(file) << std::endl;
 *     // 获取文件大小
 *     std::cout << "Size: " << fs_util::GetFileSize(file) << " bytes" << std::endl;
 *     // 复制文件
 *     fs_util::Copy(file, dir / "test_copy.txt");
 *     // 列出目录内容
 *     for (const auto& p : fs_util::ListFiles(dir)) {
 *         std::cout << "File: " << p << std::endl;
 *     }
 *     // 删除目录
 *     fs_util::Delete(dir);
 *     return 0;
 * }
 * @endcode
 */
namespace zmuduo::utils::fs_util {

/**
 * @brief 检查文件或目录是否存在。
 * @param[in] path 文件或目录路径。
 * @retval true 路径存在。
 * @retval false 路径不存在或无法访问。
 */
bool Exists(const std::filesystem::path& path);

/**
 * @brief 递归创建目录。
 * @param[in] dir 目录路径。
 * @retval true 目录创建成功或已存在。
 * @retval false 创建失败（如权限不足）。
 */
bool CreateDirectories(const std::filesystem::path& dir);

/**
 * @brief 删除文件或目录（递归删除）。
 * @param[in] path 文件或目录路径。
 * @retval true 删除成功。
 * @retval false 删除失败（如路径不存在或权限不足）。
 */
bool Delete(const std::filesystem::path& path);

/**
 * @brief 复制文件或目录（递归复制）。
 * @param[in] src 源路径。
 * @param[in] dst 目标路径。
 * @param[in] overwrite 是否覆盖已有目标（默认 true）。
 * @retval true 复制成功。
 * @retval false 复制失败（如源路径不存在或目标路径不可写）。
 */
bool Copy(const std::filesystem::path& src, const std::filesystem::path& dst, bool overwrite = true);

/**
 * @brief 移动或重命名文件或目录。
 * @param[in] src 源路径。
 * @param[in] dst 目标路径。
 * @retval true 移动成功。
 * @retval false 移动失败（如跨设备移动失败或权限不足）。
 * @note 跨设备移动将尝试复制后删除。
 */
bool Move(const std::filesystem::path& src, const std::filesystem::path& dst);

/**
 * @brief 获取文件大小（字节）。
 * @param[in] file 文件路径。
 * @return 文件大小，失败时返回 0。
 */
uintmax_t GetFileSize(const std::filesystem::path& file);

/**
 * @brief 读取文本文件内容。
 * @param[in] file 文件路径。
 * @param[in] maxBytes 可选的最大读取字节数（默认读取全部）。
 * @return 文件内容字符串，失败时返回空字符串。
 */
std::string ReadText(const std::filesystem::path& file, std::optional<size_t> maxBytes = std::nullopt);

/**
 * @brief 写入字符串到文件（覆盖模式）。
 * @param[in] file 文件路径。
 * @param[in] content 写入内容。
 * @retval true 写入成功。
 * @retval false 写入失败（如文件不可写）。
 */
bool WriteText(const std::filesystem::path& file, const std::string& content);

/**
 * @brief 读取二进制文件内容。
 * @param[in] file 文件路径。
 * @return 二进制数据向量，失败时返回空向量。
 */
std::vector<uint8_t> ReadBinary(const std::filesystem::path& file);

/**
 * @brief 写入二进制数据到文件（覆盖模式）。
 * @param[in] file 文件路径。
 * @param[in] data 二进制数据。
 * @retval true 写入成功。
 * @retval false 写入失败（如文件不可写）。
 */
bool WriteBinary(const std::filesystem::path& file, const std::vector<uint8_t>& data);

/**
 * @brief 获取文件或目录的最后修改时间。
 * @param[in] path 文件或目录路径。
 * @return 最后修改时间戳（std::time_t），失败时返回未定义值。
 */
std::time_t GetLastModifiedTime(const std::filesystem::path& path);

/**
 * @brief 列出目录下的文件和子目录。
 * @param[in] dir 目录路径。
 * @param[in] recursive 是否递归列出子目录（默认 false）。
 * @return 文件和目录路径列表，不存在时返回空列表。
 */
std::vector<std::filesystem::path> ListFiles(const std::filesystem::path& dir, bool recursive = false);

/**
 * @brief 规范化路径（处理 `.`, `..`, 多余分隔符）。
 * @param[in] path 输入路径。
 * @return 规范化后的路径。
 */
std::filesystem::path NormalizePath(const std::filesystem::path& path);

/**
 * @brief 获取文件或目录的父目录路径。
 * @param[in] path 输入路径。
 * @return 父目录路径（如 "/a/b/c.txt" -> "/a/b"）。
 */
std::filesystem::path GetDirectory(const std::filesystem::path& path);

/**
 * @brief 获取文件名（含扩展名）。
 * @param[in] path 输入路径。
 * @return 文件名（如 "/a/b/c.txt" -> "c.txt"）。
 */
std::filesystem::path GetName(const std::filesystem::path& path);

}  // namespace zmuduo::utils::fs_util

#endif