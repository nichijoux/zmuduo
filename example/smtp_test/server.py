import socketserver
import threading
import time
import base64
import os
from email.parser import Parser
from io import StringIO
from datetime import datetime

# 简单的用户数据库
USER_DATABASE = {
    "user1": "password1",
    "user2": "password2"
}

# 邮件存储目录
MAIL_STORAGE_DIR = "received_emails"

class SMTPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True

class SMTPHandler(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.sendall(b'220 Welcome to Python SMTP Server\r\n')
        self.mail_from = None
        self.rcpt_to = []
        self.data_buffer = bytearray()  # 改为字节缓冲区
        self.in_data_mode = False
        self.authenticated = False
        self.auth_username = None
        self.auth_state = None

        while True:
            try:
                data = self.request.recv(4096)  # 增大缓冲区
                if not data:
                    break

                if self.in_data_mode:
                    # 添加到缓冲区
                    self.data_buffer.extend(data)

                    # 检查是否收到结束标记\r\n.\r\n
                    if b'\r\n.\r\n' in self.data_buffer:
                        # 提取完整消息（不包括结束标记）
                        end_pos = self.data_buffer.index(b'\r\n.\r\n')
                        message_data = self.data_buffer[:end_pos]

                        # 处理消息
                        self.in_data_mode = False
                        self.data = message_data.decode('utf-8', errors='replace')
                        self.process_message()
                        self.request.sendall(b'250 Message accepted for delivery\r\n')

                        # 清空缓冲区
                        self.data_buffer.clear()
                else:
                    # 将命令转换为字符串处理
                    command_str = data.decode('utf-8').strip()

                    # 处理AUTH流程中的用户名和密码输入
                    if self.auth_state == 'waiting_for_username':
                        try:
                            self.auth_username = base64.b64decode(command_str).decode('utf-8')
                            self.auth_state = 'waiting_for_password'
                            self.request.sendall(b'334 UGFzc3dvcmQ6\r\n')  # "Password:" in Base64
                            continue
                        except:
                            self.request.sendall(b'501 Syntax error in parameters or arguments\r\n')
                            self.auth_state = None
                            continue

                    if self.auth_state == 'waiting_for_password':
                        try:
                            password = base64.b64decode(command_str).decode('utf-8')
                            if self.auth_username in USER_DATABASE and USER_DATABASE[self.auth_username] == password:
                                self.authenticated = True
                                self.request.sendall(b'235 Authentication successful\r\n')
                            else:
                                self.request.sendall(b'535 Authentication credentials invalid\r\n')
                            self.auth_state = None
                            continue
                        except:
                            self.request.sendall(b'501 Syntax error in parameters or arguments\r\n')
                            self.auth_state = None
                            continue

                    # 常规命令处理
                    command = command_str.upper().split()[0] if command_str else ''
                    args = command_str[len(command):].strip() if command_str else ''

                    if command == 'HELO':
                        self.request.sendall(b'250 Hello\r\n')
                    elif command == 'EHLO':
                        response = [
                            '250-Hello',
                            '250-AUTH LOGIN',
                            '250 HELP'
                        ]
                        self.request.sendall('\r\n'.join(response).encode('utf-8') + b'\r\n')
                    elif command == 'AUTH':
                        if args.upper().startswith('LOGIN'):
                            self.auth_state = 'waiting_for_username'
                            self.request.sendall(b'334 VXNlcm5hbWU6\r\n')  # "Username:" in Base64
                        else:
                            self.request.sendall(b'504 Unrecognized authentication type\r\n')
                    elif command == 'MAIL':
                        if not self.authenticated:
                            self.request.sendall(b'530 Authentication required\r\n')
                        elif args.upper().startswith('FROM:'):
                            self.mail_from = args[5:].strip()
                            self.request.sendall(b'250 OK\r\n')
                        else:
                            self.request.sendall(b'501 Syntax error in parameters or arguments\r\n')
                    elif command == 'RCPT':
                        if not self.authenticated:
                            self.request.sendall(b'530 Authentication required\r\n')
                        elif args.upper().startswith('TO:'):
                            self.rcpt_to.append(args[3:].strip())
                            self.request.sendall(b'250 OK\r\n')
                        else:
                            self.request.sendall(b'501 Syntax error in parameters or arguments\r\n')
                    elif command == 'DATA':
                        if not self.authenticated:
                            self.request.sendall(b'530 Authentication required\r\n')
                        else:
                            self.request.sendall(b'354 Start mail input; end with <CRLF>.<CRLF>\r\n')
                            self.in_data_mode = True
                            self.data_buffer.clear()  # 准备接收数据
                    elif command == 'QUIT':
                        self.request.sendall(b'221 Bye\r\n')
                        break
                    elif command == 'NOOP':
                        self.request.sendall(b'250 OK\r\n')
                    elif command == 'RSET':
                        self.mail_from = None
                        self.rcpt_to = []
                        self.data = ''
                        self.authenticated = False
                        self.auth_username = None
                        self.auth_state = None
                        self.data_buffer.clear()
                        self.request.sendall(b'250 OK\r\n')
                    elif command == 'VRFY':
                        self.request.sendall(b'252 Cannot VRFY user\r\n')
                    else:
                        self.request.sendall(b'500 Command not recognized\r\n')
            except Exception as e:
                print(f"Error handling client: {e}")
                break

    def process_message(self):
        print("\nReceived new email:")
        print(f"From: {self.mail_from}")
        print(f"To: {', '.join(self.rcpt_to)}")
        print(f"Authenticated as: {self.auth_username}")
        print("Message:")
        print(self.data)

        # Parse the email
        email_parser = Parser()
        msg = email_parser.parse(StringIO(self.data))

        print("\nParsed email headers:")
        for header, value in msg.items():
            print(f"{header}: {value}")

        # 保存邮件到文件
        self.save_email_to_file(msg)

        # Reset for next message
        self.mail_from = None
        self.rcpt_to = []
        self.data = ''

    def save_email_to_file(self, msg):
        """保存邮件内容到文件"""
        try:
            # 确保邮件存储目录存在
            os.makedirs(MAIL_STORAGE_DIR, exist_ok=True)

            # 生成文件名：时间戳+发件人
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            from_email = self.mail_from.replace('@', '_at_').replace('<', '').replace('>', '')
            filename = f"{timestamp}_{from_email}.eml"
            filepath = os.path.join(MAIL_STORAGE_DIR, filename)

            # 保存原始邮件内容
            with open(filepath, 'w', encoding='utf-8') as f:
                print(repr(self.data))
                f.write(self.data)

            print(f"\nEmail saved to: {filepath}")

            # 如果有附件，单独保存
            if msg.is_multipart():
                for part in msg.walk():
                    if part.get_content_disposition() == 'attachment':
                        attachment_filename = part.get_filename()
                        if attachment_filename:
                            attachment_path = os.path.join(
                                MAIL_STORAGE_DIR,
                                f"{timestamp}_{attachment_filename}"
                            )
                            with open(attachment_path, 'wb') as f:
                                f.write(part.get_payload(decode=True))
                            print(f"Attachment saved: {attachment_path}")

        except Exception as e:
            print(f"Error saving email: {e}")

def run_smtp_server(host='localhost', port=1025):
    # 确保邮件存储目录存在
    os.makedirs(MAIL_STORAGE_DIR, exist_ok=True)

    server = SMTPServer((host, port), SMTPHandler)
    print(f"SMTP server running on {host}:{port}")
    print(f"Available users: {', '.join(USER_DATABASE.keys())}")
    print(f"Emails will be saved to: {os.path.abspath(MAIL_STORAGE_DIR)}")

    try:
        server_thread = threading.Thread(target=server.serve_forever)
        server_thread.daemon = True
        server_thread.start()

        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nShutting down server...")
        server.shutdown()
        server.server_close()

if __name__ == '__main__':
    run_smtp_server()