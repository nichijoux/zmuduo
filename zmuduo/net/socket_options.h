// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_SOCKET_OPTIONS_H
#define ZMUDUO_NET_SOCKET_OPTIONS_H

#include "zmuduo/base/types.h"
#include <netinet/in.h>

namespace zmuduo::net::sockets {
#if VALGRIND || defined(NO_ACCEPT4)
void setNonBlockAndCloseOnExec(int socketFD);
#endif

/**
 * @brief 创建一个非阻塞模式的套接字
 *
 * @param[in] family 地址族
 * @return socket的fd
 */
int createNonblockingOrDie(sa_family_t family);

int getSocketError(int sockfd);

/**
 * @brief 将sockaddr_in类型的指针转换为sockaddr类型的指针
 *
 * @param[in] addr 指向sockaddr_in结构体的指针，该结构体通常用于表示IPv4地址和端口信息
 * @return const sockaddr* 返回转换后的sockaddr类型的指针，便于与通用的网络套接字函数兼容
 */
const sockaddr* sockaddr_cast(const sockaddr_in* addr);

/**
 * @brief 将sockaddr_in6类型的指针转换为sockaddr类型的指针
 *
 * @param[in] addr 指向sockaddr_in6结构体的指针，该结构体通常用于表示IPv6地址和端口信息
 * @return const sockaddr* 返回转换后的sockaddr类型的指针，便于与通用的网络套接字函数兼容
 */
const sockaddr* sockaddr_cast(const sockaddr_in6* addr);

/**
 * @brief 将sockaddr类型的指针转换为sockaddr_in类型的指针
 *
 * @param[in] addr 指向sockaddr结构体的指针，该结构体是通用的网络地址结构体
 * @return const sockaddr_in* 返回转换后的sockaddr_in类型的指针，便于访问IPv4相关的成员变量
 */
const sockaddr_in* sockaddr_in_cast(const sockaddr* addr);

/**
 * @brief 将sockaddr类型的指针转换为sockaddr_in6类型的指针
 *
 * @param[in] addr 指向sockaddr结构体的指针，该结构体是通用的网络地址结构体
 * @return const sockaddr_in6* 返回转换后的sockaddr_in6类型的指针，便于访问IPv6相关的成员变量
 */
const sockaddr_in6* sockaddr_in6_cast(const sockaddr* addr);

/**
 * @brief 获取本地地址
 *
 * @param[in] sockFD 套接字描述符
 * @return struct sockaddr 本地地址
 */
sockaddr_in getLocalAddress(int sockFD);

/**
 * @brief 获取远程连接的地址
 * @param[in] sockFD 套接字描述符
 * @return struct sockaddr 本地地址
 */
sockaddr_in getPeerAddress(int sockFD);

/**
 * @brief 判断是否是自连接
 * @param[in] sockFD 套接字描述符
 * @retval true 是自连接
 * @retval false 不是自连接
 */
bool isSelfConnect(int sockFD);
}  // namespace zmuduo::net::sockets

#endif