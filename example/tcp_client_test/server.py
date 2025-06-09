import socket

HOST = '127.0.0.1'  # 监听本地地址（可改为 '0.0.0.0' 监听所有地址）
PORT = 8500  # 监听端口


def start_server():
    # 创建 TCP 套接字
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        # 设置地址重用
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # 绑定地址和端口
        server_socket.bind((HOST, PORT))
        # 开始监听，最大连接数为 5
        server_socket.listen(5)
        print(f"[*] TCP服务器已启动，监听 {HOST}:{PORT}")

        while True:
            # 接受客户端连接
            client_socket, client_address = server_socket.accept()
            print(f"[+] 客户端已连接：{client_address}")

            with client_socket:
                while True:
                    data = client_socket.recv(1024)  # 接收数据
                    if not data:
                        print("[-] 客户端已断开连接")
                        break
                    print(f"[>] 收到数据：{data.decode('utf-8')}")
                    client_socket.sendall(data)  # 回显数据


if __name__ == "__main__":
    start_server()
