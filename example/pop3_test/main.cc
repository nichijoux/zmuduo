#include "zmuduo/base/utils/hash_util.h"
#include <fstream>
#include <regex>
#include <zmuduo/base/logger.h>
#include <zmuduo/base/utils/string_util.h>
#include <zmuduo/net/email/email.h>
#include <zmuduo/net/email/pop3_client.h>

using namespace zmuduo;
using namespace zmuduo::utils;
using namespace zmuduo::net;
using namespace zmuduo::net::email;

int main() {
    EventLoop  loop;
    auto       address = IPv4Address::Create("127.0.0.1", 110);
    Pop3Client client(&loop, address, "user", "pass");
    client.setAuthenticateCallback([&]() {
        client.stat([](const Pop3StatResponse::Ptr& response) {
            if (response->m_success) {
                ZMUDUO_LOG_INFO << response->m_num;
                ZMUDUO_LOG_INFO << response->m_size;
            } else {
                ZMUDUO_LOG_INFO << response;
            }
        });
        client.list([](const Pop3ListResponse::Ptr& response) {
            if (response->m_success) {
                for (const auto& e : response->m_entries) {
                    ZMUDUO_LOG_INFO << e.m_num << "," << e.m_size;
                }
            }
        });
        client.retr(5, [](const Pop3RetrResponse::Ptr& response) {
            if (response->m_success) {
//                auto email = parsePOP3ResponseToEmail(response->m_message);
//                if (email) {
//                    ZMUDUO_LOG_IMPORTANT << "解析成功";
//
//                    ZMUDUO_LOG_IMPORTANT << "Subject: " << email->getTitle();
//                    ZMUDUO_LOG_IMPORTANT << "Body: " << email->getBody();
//                    for (const auto& entity : email->getEntities()) {
//                        ZMUDUO_LOG_IMPORTANT << "Attachment: " << entity->getContent();
//                    }
//                }
            }
        });
    });
    client.connect();
    loop.loop();
    return 0;
}