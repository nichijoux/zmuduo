from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import time


class MyRequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        # 根据URL路径判断返回类型
        if self.path == "/normal":
            self.handle_normal_response()
        elif self.path == "/chunked":
            self.handle_chunked_response()
        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"Not Found")

    def handle_normal_response(self):
        # 正常返回数据
        self.send_response(200)
        self.send_header("Connection", "keep-alive")
        self.send_header("Content-type", "application/json")
        self.end_headers()
        response_data = []
        for i in range(10):
            response_data.append({"message": "This is a normal response"})
        self.wfile.write(json.dumps(response_data).encode())

    def handle_chunked_response(self):
        # 分块返回JSON数据
        self.send_response(200)
        self.send_header("Connection", "keep-alive")
        self.send_header("Transfer-Encoding", "chunked")
        self.send_header("Content-type", "application/json")
        self.end_headers()

        # 生成100个JSON数据
        data = [{"id": i, "name": f"Item {i}"} for i in range(1, 10)]

        # 分块发送数据
        for item in data:
            chunk = json.dumps(item) + "\n"  # 每个块是一个 JSON 对象，加上换行符
            chunk_bytes = chunk.encode("utf-8")
            chunk_size = len(chunk_bytes)

            # 发送块大小（以十六进制表示）和数据
            self.wfile.write(f"{chunk_size:x}\r\n".encode("utf-8"))  # 发送块大小
            self.wfile.write(chunk_bytes)  # 发送块数据
            self.wfile.write("\r\n".encode("utf-8"))  # 块数据结束标志
        time.sleep(1)
        # 发送最后一个块（大小为 0，表示结束）
        self.wfile.write("0\r\n\r\n".encode("utf-8"))


def run(server_class=HTTPServer, handler_class=MyRequestHandler, port=8000):
    server_address = ("", port)
    httpd = server_class(server_address, handler_class)
    print(f"Starting httpd server on port {port}...")
    httpd.serve_forever()


if __name__ == "__main__":
    run()
