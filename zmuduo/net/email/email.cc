#include "zmuduo/net/email/email.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/base/utils/fs_util.h"
#include "zmuduo/base/utils/hash_util.h"
#include <fstream>
#include <sstream>

namespace zmuduo::net::email {
EMailEntity::Ptr EMailEntity::CreateAttachment(const std::string& path) {
    // 打开文件，以二进制模式读取
    std::ifstream ifs(path, std::ios::binary);
    // 如果文件无法打开，返回 nullptr
    if (!ifs.is_open()) {
        ZMUDUO_LOG_ERROR << "can't open file " << path;
        return nullptr;
    }
    // 定义一个缓冲区，用于存储读取的文件内容
    std::string buffer(1024, '\0');
    // 创建一个新的邮件实体指针
    EMailEntity::Ptr entity(new EMailEntity());
    // 循环读取文件内容，直到文件结束
    while (!ifs.eof()) {
        // 从文件中读取数据到缓冲区
        ifs.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        // 将读取的数据追加到邮件实体的内容中
        entity->m_content.append(buffer.c_str(), ifs.gcount());
    }
    // 将邮件实体的内容进行Base64编码
    entity->m_content = utils::HashUtil::Base64encode(entity->m_content);
    // 添加邮件头，指定内容传输编码为Base64
    entity->addHeader("Content-Transfer-Encoding", "base64");
    entity->addHeader("Content-Disposition", "attachment");
    entity->addHeader("Content-Type",
                      "application/octet-stream;name=" + utils::FSUtil::GetName(path).string());
    return entity;
}

std::string EMailEntity::toString() const {
    std::stringstream ss;
    for (const auto& [k, v] : m_headers) {
        ss << k << ": " << v << "\r\n";
    }
    ss << m_content << "\r\n";
    return ss.str();
}

EMail::Ptr EMail::Create(const std::string&              from_address,
                         const std::string&              from_password,
                         const std::string&              title,
                         const std::string&              body,
                         const std::vector<std::string>& to_address,
                         const std::vector<std::string>& cc_address,
                         const std::vector<std::string>& bcc_address) {
    EMail::Ptr email = std::make_shared<EMail>();
    email->setFromEMailAddress(from_address);
    email->setFromEMailPassword(from_password);
    email->setTitle(title);
    email->setBody(body);
    email->setToEMailAddress(to_address);
    email->setCcEMailAddress(cc_address);
    email->setBccEMailAddress(bcc_address);
    return email;
}
}  // namespace zmuduo::net::email