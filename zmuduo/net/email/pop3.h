// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_EMAIL_POP3_H
#define ZMUDUO_NET_EMAIL_POP3_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace zmuduo::net::email {

/**
 * @struct Pop3Response
 * @brief POP3 响应基类，表示服务器对命令的响应。
 *
 * 所有 POP3 命令响应的基类，包含成功标志和消息内容。
 *
 * @note 派生类表示特定命令的响应（如 Pop3StatResponse, Pop3RetrResponse）。
 */
struct Pop3Response {
    using Ptr = std::shared_ptr<Pop3Response>;  ///< 智能指针类型

    /**
     * @brief 默认构造函数。
     * @note 初始化为失败状态。
     */
    Pop3Response() : m_success(false) {}

    /**
     * @brief 构造函数，指定成功状态。
     * @param[in] success 是否成功。
     */
    explicit Pop3Response(bool success) : m_success(success), m_message() {}

    /**
     * @brief 构造函数，指定成功状态和消息。
     * @param[in] success 是否成功。
     * @param[in] message 响应消息。
     */
    Pop3Response(bool success, std::string message)
        : m_success(success), m_message(std::move(message)) {}

    /**
     * @brief 虚析构函数。
     * @note 确保派生类正确析构。
     */
    virtual ~Pop3Response() = default;

    bool        m_success;  ///< 响应是否成功（+OK）
    std::string m_message;  ///< 响应消息内容
};

/**
 * @struct Pop3StatResponse
 * @brief STAT 命令响应，表示邮箱状态。
 */
struct Pop3StatResponse : public Pop3Response {
    using Ptr = std::shared_ptr<Pop3StatResponse>;

    /**
     * @brief 默认构造函数。
     */
    Pop3StatResponse() : Pop3Response(), m_size(0), m_num(0) {}

    /**
     * @brief 构造函数，指定邮件总数和总大小。
     * @param[in] num 邮件总数。
     * @param[in] size 总字节数。
     */
    Pop3StatResponse(int num, size_t size) : Pop3Response(true), m_num(num), m_size(size) {}

    int    m_num;   ///< 邮件总数
    size_t m_size;  ///< 总字节数
};

/**
 * @struct Pop3UidlResponse
 * @brief 无参数 UIDL 命令响应，列出所有邮件的唯一标识符。
 */
struct Pop3UidlResponse : public Pop3Response {
    using Ptr = std::shared_ptr<Pop3UidlResponse>;

    struct UidlItem {
        UidlItem(int num, std::string id) : m_num(num), m_id(std::move(id)) {}

        int         m_num;
        std::string m_id;
    };

    /**
     * @brief 默认构造函数。
     */
    Pop3UidlResponse() : Pop3Response(), m_uids() {}

    /**
     * @brief 构造函数，指定 UID 列表。
     * @param[in] uids 邮件编号和 UID 列表。
     */
    explicit Pop3UidlResponse(std::vector<UidlItem> uids)
        : Pop3Response(true), m_uids(std::move(uids)) {}

    std::vector<UidlItem> m_uids;  ///< 邮件编号和 UID 列表
};

/**
 * @struct Pop3UidlNResponse
 * @brief 带参数 UIDL 命令响应，表示指定邮件的唯一标识符。
 */
struct Pop3UidlNResponse : public Pop3Response {
    using Ptr = std::shared_ptr<Pop3UidlNResponse>;

    /**
     * @brief 默认构造函数。
     */
    Pop3UidlNResponse() : Pop3Response(), m_num(0), m_id() {}

    /**
     * @brief 构造函数，指定邮件编号和 UID。
     * @param[in] num 邮件编号。
     * @param[in] id 邮件唯一标识符。
     */
    Pop3UidlNResponse(int num, std::string id)
        : Pop3Response(true), m_num(num), m_id(std::move(id)) {}

    int         m_num;  ///< 邮件编号
    std::string m_id;   ///< 邮件唯一标识符
};

/**
 * @struct Pop3ListResponse
 * @brief 无参数 LIST 命令响应，列出所有邮件的大小。
 */
struct Pop3ListResponse : public Pop3Response {
    using Ptr = std::shared_ptr<Pop3ListResponse>;

    struct ListItem {
        ListItem(int num, size_t size) : m_num(num), m_size(size) {}

        int    m_num;
        size_t m_size;
    };

    /**
     * @brief 默认构造函数。
     */
    Pop3ListResponse() = default;

    /**
     * @brief 构造函数，指定邮件大小列表。
     * @param[in] entries 邮件编号和大小列表。
     */
    explicit Pop3ListResponse(std::vector<ListItem> entries)
        : Pop3Response(true), m_entries(std::move(entries)) {}

    std::vector<ListItem> m_entries;  ///< 邮件编号和大小列表
};

/**
 * @struct Pop3ListNResponse
 * @brief 带参数 LIST 命令响应，表示指定邮件的大小。
 */
struct Pop3ListNResponse : public Pop3Response {
    using Ptr = std::shared_ptr<Pop3ListNResponse>;

    /**
     * @brief 默认构造函数。
     */
    Pop3ListNResponse() : Pop3Response(), m_num(0), m_size(0) {}

    /**
     * @brief 构造函数，指定邮件编号和大小。
     * @param[in] num 邮件编号。
     * @param[in] size 邮件大小（字节）。
     */
    Pop3ListNResponse(int num, size_t size) : Pop3Response(true), m_num(num), m_size(size) {}

    int    m_num;   ///< 邮件编号
    size_t m_size;  ///< 邮件大小（字节）
};

/**
 * @struct Pop3RetrResponse
 * @brief RETR 命令响应，表示检索的邮件内容。
 */
struct Pop3RetrResponse : public Pop3Response {
    using Ptr = std::shared_ptr<Pop3RetrResponse>;

    /**
     * @brief 默认构造函数。
     */
    Pop3RetrResponse() = default;

    /**
     * @brief 构造函数，指定邮件内容。
     * @param[in] message 邮件内容。
     */
    explicit Pop3RetrResponse(std::string message) : Pop3Response(true, std::move(message)) {}
};

/**
 * @struct Pop3DeleResponse
 * @brief DELE 命令响应，表示删除标记的结果。
 */
struct Pop3DeleResponse : public Pop3Response {
    using Ptr = std::shared_ptr<Pop3DeleResponse>;

    /**
     * @brief 默认构造函数。
     */
    Pop3DeleResponse() = default;

    /**
     * @brief 构造函数，指定响应消息。
     * @param[in] message 响应消息。
     */
    explicit Pop3DeleResponse(std::string message) : Pop3Response(true, std::move(message)) {}
};

/**
 * @struct Pop3RsetResponse
 * @brief RSET 命令响应，表示会话重置的结果。
 */
struct Pop3RsetResponse : public Pop3Response {
    using Ptr = std::shared_ptr<Pop3RsetResponse>;

    /**
     * @brief 默认构造函数。
     */
    Pop3RsetResponse() = default;

    /**
     * @brief 构造函数，指定响应消息。
     * @param[in] message 响应消息。
     */
    explicit Pop3RsetResponse(std::string message) : Pop3Response(true, std::move(message)) {}
};

/**
 * @struct Pop3TopResponse
 * @brief TOP 命令响应，表示邮件头部和部分正文。
 */
struct Pop3TopResponse : public Pop3Response {
    using Ptr = std::shared_ptr<Pop3TopResponse>;

    /**
     * @brief 默认构造函数。
     */
    Pop3TopResponse() = default;

    /**
     * @brief 构造函数，指定头部和正文预览。
     * @param[in] header 邮件头部。
     * @param[in] bodyPreview 正文预览。
     */
    explicit Pop3TopResponse(std::string header, std::string bodyPreview)
        : Pop3Response(true), m_header(std::move(header)), m_bodyPreview(std::move(bodyPreview)) {}

    std::string m_header;       ///< 邮件头部
    std::string m_bodyPreview;  ///< 正文预览
};

/**
 * @struct Pop3NoopResponse
 * @brief NOOP 命令响应，表示空操作结果。
 */
struct Pop3NoopResponse : public Pop3Response {
    using Ptr = std::shared_ptr<Pop3NoopResponse>;

    /**
     * @brief 构造函数。
     */
    explicit Pop3NoopResponse() : Pop3Response(true) {}
};

/**
 * @struct Pop3CapaResponse
 * @brief CAPA 命令响应，表示服务器支持的功能。
 */
struct Pop3CapaResponse : public Pop3Response {
    using Ptr = std::shared_ptr<Pop3CapaResponse>;

    /**
     * @brief 默认构造函数。
     */
    Pop3CapaResponse() = default;

    /**
     * @brief 构造函数，指定功能列表。
     * @param[in] capabilities 服务器支持的功能列表。
     */
    explicit Pop3CapaResponse(std::vector<std::string> capabilities)
        : Pop3Response(true), m_capabilities(std::move(capabilities)) {}

    std::vector<std::string> m_capabilities;  ///< 服务器支持的功能列表
};

/**
 * @struct Pop3QuitResponse
 * @brief QUIT 命令响应，表示会话结束结果。
 */
struct Pop3QuitResponse : public Pop3Response {
    using Ptr = std::shared_ptr<Pop3QuitResponse>;

    /**
     * @brief 默认构造函数。
     */
    Pop3QuitResponse() = default;

    /**
     * @brief 构造函数，指定响应消息。
     * @param[in] message 响应消息。
     */
    explicit Pop3QuitResponse(std::string message) : Pop3Response(true, std::move(message)) {}
};

}  // namespace zmuduo::net::email

#endif