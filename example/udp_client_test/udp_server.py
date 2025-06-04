import socket


def udp_server(host='0.0.0.0', port=8000):
    # 创建 UDP 套接字
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # 绑定地址和端口
    server_socket.bind((host, port))
    print(f"UDP 服务器已启动，监听 {host}:{port}")

    try:
        while True:
            # 接收数据和客户端地址
            data, client_addr = server_socket.recvfrom(1024)
            message = data.decode('utf-8')
            print(f"收到来自 {client_addr} 的消息: {message}")

            # 构造响应内容
            response = f"服务器已收到：{message}"
            server_socket.sendto(response.encode('utf-8'), client_addr)

    except KeyboardInterrupt:
        print("\n服务器已退出。")

    finally:
        server_socket.close()


if __name__ == '__main__':
    udp_server()
