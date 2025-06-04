// Original code - Copyright (c) sylar-yin —— sylar
// Modified by nichijoux

#ifndef ZMUDUO_NET_EMAIL_H
#define ZMUDUO_NET_EMAIL_H

#include "zmuduo/base/copyable.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace zmuduo::net::email {
#define ZMUDUO_EMAIL_END_TAG "\r\n.\r\n"

class EMailEntity;
class EMail;

/**
 * @class EMailEntity
 * @brief 表示 MIME 内容，如邮件正文或附件。
 *
 * EMailEntity 用于构建邮件的 MIME 内容，支持自定义 MIME 头部和内容体，常用于邮件正文、HTML
 * 内容或文件附件。 遵循 MIME 标准（RFC 2045-2049），支持 Base64 编码的附件。
 *
 * @note EMailEntity 是可拷贝的（继承自 Copyable），通过智能指针（Ptr）管理实例。
 * @note 附件内容应注意大小限制，以避免 SMTP 服务器拒绝。
 *
 * @example
 *
 * @code
 * auto entity = EMailEntity::CreateAttachment("example.pdf");
 * entity->setHeader("Content-Type", "application/pdf");
 * std::cout << entity->toString() << std::endl;
 * @endcode
 */
class EMailEntity : public Copyable {
  public:
    /**
     * @typedef std::shared_ptr&lt;EMailEntity&gt;
     * @brief EMailEntity 智能指针类型。
     */
    using Ptr = std::shared_ptr<EMailEntity>;

  public:
    /**
     * @brief 默认构造函数，初始化空实体。
     */
    EMailEntity() = default;

    /**
     * @brief 默认析构函数，清理资源。
     */
    ~EMailEntity() = default;

    /**
     * @brief 创建一个附件实体。
     * @param[in] path 文件路径。
     * @return 指向 EMailEntity 的智能指针。
     * @note 文件内容将以 Base64 编码，并设置适当的 MIME 头部。
     * @warning 如果文件无法打开，返回nullptr
     */
    static EMailEntity::Ptr CreateAttachment(const std::string& path);

    /**
     * @brief 添加 MIME 头字段。
     * @param[in] key 头部键名（如 "Content-Type"）。
     * @param[in] val 头部值。
     */
    void addHeader(const std::string& key, const std::string& val) {
        m_headers[key] = val;
    }

    /**
     * @brief 获取指定头字段的值。
     * @param[in] key 头部键名。
     * @param[in] defaultValue 如果未找到键，返回的默认值。
     * @return 头部值或默认值。
     */
    std::string getHeader(const std::string& key, const std::string& defaultValue = "") {
        auto it = m_headers.find(key);
        return it == m_headers.end() ? defaultValue : it->second;
    }

    /**
     * @brief 获取实体内容。
     * @return 内容体字符串（如 Base64 编码的附件或文本正文）。
     */
    const std::string& getContent() const {
        return m_content;
    }

    /**
     * @brief 设置实体内容。
     * @param[in] v 内容体字符串。
     */
    void setContent(const std::string& v) {
        m_content = v;
    }

    /**
     * @brief 将实体格式化为 MIME 字符串。
     * @return 符合 MIME 标准的字符串，包含头部和内容体。
     */
    std::string toString() const;

    /**
     * @brief 输出实体到流。
     * @param[in,out] os 输出流。
     * @param[in] entity EMailEntity 对象。
     * @return 输出流。
     */
    friend std::ostream& operator<<(std::ostream& os, const EMailEntity& entity) {
        os << entity.toString();
        return os;
    }

  private:
    std::map<std::string, std::string> m_headers;  ///< MIME 头字段集合
    std::string                        m_content;  ///< 内容体，如正文或附件内容
};

/**
 * @class EMail
 * @brief 表示一封完整的电子邮件。
 *
 * EMail 用于构造和表示一封电子邮件，包含发件人、收件人、标题、正文及 MIME 实体（正文或附件）。<br/>
 * 支持快速创建邮件对象，并可通过 SMTP 协议发送。
 *
 * @note EMail 是可拷贝的（继承自 Copyable），通过智能指针（Ptr）管理实例。
 * @note 发件人密码用于 SMTP 身份验证，需确保安全存储。
 *
 * @example
 *
 * @code
 * auto email = EMail::Create("sender@example.com", "password", "Test Email", "Hello, World!",
 *                            {"receiver@example.com"});
 * email->addEntity(EMailEntity::CreateAttachment("example.pdf"));
 * // 通过 SMTP 客户端发送邮件（未展示）
 * @endcode
 */
class EMail : public Copyable {
  public:
    /**
     * @typedef std::shared_ptr&lt;EMail&gt;
     * @brief EMail 智能指针类型。
     */
    using Ptr = std::shared_ptr<EMail>;

  public:
    /**
     * @brief 快速创建一封电子邮件对象。
     * @param[in] from_address 发件人邮箱地址。
     * @param[in] from_password 发件人邮箱密码（用于登录）。
     * @param[in] title 邮件标题。
     * @param[in] body 邮件正文。
     * @param[in] to_address 收件人地址列表。
     * @param[in] cc_address 抄送地址列表（可选）。
     * @param[in] bcc_address 密送地址列表（可选）。
     * @return 指向 EMail 的智能指针。
     */
    static EMail::Ptr Create(const std::string&              from_address,
                             const std::string&              from_password,
                             const std::string&              title,
                             const std::string&              body,
                             const std::vector<std::string>& to_address,
                             const std::vector<std::string>& cc_address  = {},
                             const std::vector<std::string>& bcc_address = {});

    /**
     * @brief 获取发件人邮箱地址。
     * @return 发件人邮箱地址。
     */
    const std::string& getFromEMailAddress() const {
        return m_fromEMailAddress;
    }

    /**
     * @brief 设置发件人邮箱地址。
     * @param[in] v 发件人邮箱地址。
     */
    void setFromEMailAddress(const std::string& v) {
        m_fromEMailAddress = v;
    }

    /**
     * @brief 获取发件人邮箱密码。
     * @return 发件人邮箱密码。
     */
    const std::string& getFromEMailPassword() const {
        return m_fromEMailPassword;
    }

    /**
     * @brief 设置发件人邮箱密码。
     * @param[in] v 发件人邮箱密码。
     */
    void setFromEMailPassword(const std::string& v) {
        m_fromEMailPassword = v;
    }

    /**
     * @brief 获取邮件标题。
     * @return 邮件标题。
     */
    const std::string& getTitle() const {
        return m_title;
    }

    /**
     * @brief 设置邮件标题。
     * @param[in] v 邮件标题。
     */
    void setTitle(const std::string& v) {
        m_title = v;
    }

    /**
     * @brief 获取邮件正文。
     * @return 邮件正文。
     */
    const std::string& getBody() const {
        return m_body;
    }

    /**
     * @brief 设置邮件正文。
     * @param[in] v 邮件正文。
     */
    void setBody(const std::string& v) {
        m_body = v;
    }

    /**
     * @brief 获取收件人邮箱地址列表。
     * @return 收件人邮箱地址列表。
     */
    const std::vector<std::string>& getToEMailAddress() const {
        return m_toEMailAddress;
    }

    /**
     * @brief 设置收件人邮箱地址列表。
     * @param[in] v 收件人邮箱地址列表。
     */
    void setToEMailAddress(const std::vector<std::string>& v) {
        m_toEMailAddress = v;
    }

    /**
     * @brief 获取抄送人邮箱地址列表。
     * @return 抄送人邮箱地址列表。
     */
    const std::vector<std::string>& getCcEMailAddress() const {
        return m_ccEMailAddress;
    }

    /**
     * @brief 设置抄送人邮箱地址列表。
     * @param[in] v 抄送人邮箱地址列表。
     */
    void setCcEMailAddress(const std::vector<std::string>& v) {
        m_ccEMailAddress = v;
    }

    /**
     * @brief 获取密送人邮箱地址列表。
     * @return 密送人邮箱地址列表。
     */
    const std::vector<std::string>& getBccEMailAddress() const {
        return m_bccEMailAddress;
    }

    /**
     * @brief 设置密送人邮箱地址列表。
     * @param[in] v 密送人邮箱地址列表。
     */
    void setBccEMailAddress(const std::vector<std::string>& v) {
        m_bccEMailAddress = v;
    }

    /**
     * @brief 添加一个邮件实体（如正文或附件）。
     * @param[in] entity 邮件实体智能指针。
     */
    void addEntity(const EMailEntity::Ptr& entity) {
        m_entities.emplace_back(entity);
    }

    /**
     * @brief 获取所有邮件实体（正文和附件）。
     * @return 邮件实体列表。
     */
    const std::vector<EMailEntity::Ptr>& getEntities() const {
        return m_entities;
    }

  private:
    std::string                   m_fromEMailAddress;   ///< 发件人邮箱
    std::string                   m_fromEMailPassword;  ///< 发件人密码（登录验证用）
    std::string                   m_title;              ///< 邮件标题
    std::string                   m_body;               ///< 邮件正文
    std::vector<std::string>      m_toEMailAddress;     ///< 收件人邮箱列表
    std::vector<std::string>      m_ccEMailAddress;     ///< 抄送人邮箱列表
    std::vector<std::string>      m_bccEMailAddress;    ///< 密送人邮箱列表
    std::vector<EMailEntity::Ptr> m_entities;           ///< 邮件实体（正文、附件等）
};
}  // namespace zmuduo::net::email

#endif