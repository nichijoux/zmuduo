import socket
import threading
import os

MAILBOX_DIR = "./mailbox"  # 每封邮件一个文件，文件名为 1.txt, 2.txt 等
USERNAME = "user"
PASSWORD = "pass"


class Pop3Session:
    def __init__(self, conn, addr):
        self.conn = conn
        self.addr = addr
        self.state = "AUTH"
        self.username = ""
        self.messages = self.load_messages()
        self.deleted = set()

    @staticmethod
    def load_messages():
        messages = []
        for fname in sorted(os.listdir(MAILBOX_DIR)):
            if fname.endswith(".txt"):
                with open(os.path.join(MAILBOX_DIR, fname), "rb") as f:
                    content = f.read()
                    messages.append(content)
        return messages

    def send_line(self, line):
        self.conn.sendall((line + "\r\n").encode())

    def handle(self):
        self.send_line("+OK POP3 server ready")
        buffer = ""  # 用于接收残留的命令片段

        while True:
            try:
                data = self.conn.recv(1024)
                if not data:
                    break

                buffer += data.decode(errors='ignore')  # 累加数据
                while '\r\n' in buffer:
                    line, buffer = buffer.split('\r\n', 1)  # 分离一行命令
                    line = line.strip()
                    if not line:
                        continue

                    print(f"[{self.addr}] >> {line}")
                    parts = line.split()
                    cmd = parts[0].upper()
                    args = parts[1:]

                    if self.state == "AUTH":
                        self.handle_auth(cmd, args)
                    elif self.state == "TRANSACTION":
                        self.handle_transaction(cmd, args)

            except Exception as e:
                print(f"Error: {e}")
                break

        self.conn.close()
        self.cleanup_deleted()

    def handle_auth(self, cmd, args):
        if cmd == "USER":
            if args and args[0] == USERNAME:
                self.send_line("+OK User accepted")
                self.username = args[0]
            else:
                self.send_line("-ERR Invalid user")
        elif cmd == "PASS":
            if args and self.username == USERNAME and args[0] == PASSWORD:
                self.send_line("+OK Authenticated")
                self.state = "TRANSACTION"
            else:
                self.send_line("-ERR Invalid password")
        elif cmd == "QUIT":
            self.send_line("+OK Goodbye")
            self.conn.close()
        else:
            self.send_line("-ERR Command not allowed")

    def handle_transaction(self, cmd, args):
        if cmd == "STAT":
            count = len(self.messages) - len(self.deleted)
            size = sum(len(m) for i, m in enumerate(self.messages) if i not in self.deleted)
            self.send_line(f"+OK {count} {size}")
        elif cmd == "LIST":
            if not args:
                self.send_line(f"+OK {len(self.messages)} messages")
                for i, m in enumerate(self.messages):
                    if i not in self.deleted:
                        self.send_line(f"{i + 1} {len(m)}")
                self.send_line(".")
            else:
                index = int(args[0]) - 1
                if 0 <= index < len(self.messages) and index not in self.deleted:
                    self.send_line(f"+OK {index + 1} {len(self.messages[index])}")
                else:
                    self.send_line("-ERR No such message")
        elif cmd == "RETR":
            index = int(args[0]) - 1
            if 0 <= index < len(self.messages) and index not in self.deleted:
                self.send_line(f"+OK {len(self.messages[index])} octets")
                for line in self.messages[index].splitlines():
                    self.send_line(line.decode())
                self.send_line(".")
            else:
                self.send_line("-ERR No such message")
        elif cmd == "DELE":
            index = int(args[0]) - 1
            if 0 <= index < len(self.messages):
                self.deleted.add(index)
                self.send_line(f"+OK Message {index + 1} marked for deletion")
            else:
                self.send_line("-ERR No such message")
        elif cmd == "NOOP":
            self.send_line("+OK")
        elif cmd == "RSET":
            self.deleted.clear()
            self.send_line("+OK Deletion marks cleared")
        elif cmd == "UIDL":
            if not args:
                self.send_line("+OK unique-id listing follows")
                for i, m in enumerate(self.messages):
                    if i not in self.deleted:
                        uid = f"UID{i + 1:04d}"
                        self.send_line(f"{i + 1} {uid}")
                self.send_line(".")
            else:
                index = int(args[0]) - 1
                if 0 <= index < len(self.messages) and index not in self.deleted:
                    self.send_line(f"+OK {index + 1} UID{index + 1:04d}")
                else:
                    self.send_line("-ERR No such message")
        elif cmd == "TOP":
            index = int(args[0]) - 1
            n = int(args[1])
            if 0 <= index < len(self.messages) and index not in self.deleted:
                lines = self.messages[index].splitlines()
                header = []
                body = []
                sep_found = False
                for line in lines:
                    if not sep_found and line == b'':
                        sep_found = True
                    if not sep_found:
                        header.append(line)
                    else:
                        body.append(line)
                self.send_line("+OK Top of message follows")
                for line in header:
                    self.send_line(line.decode())
                for line in body[:n]:
                    self.send_line(line.decode())
                self.send_line(".")
            else:
                self.send_line("-ERR No such message")
        elif cmd == "QUIT":
            self.send_line("+OK Goodbye")
            self.conn.close()
        else:
            self.send_line("-ERR Command not supported")

    def cleanup_deleted(self):
        for i in sorted(self.deleted, reverse=True):
            path = os.path.join(MAILBOX_DIR, f"{i + 1}.txt")
            if os.path.exists(path):
                os.remove(path)


def start_server(host="127.0.0.1", port=110):
    if not os.path.exists(MAILBOX_DIR):
        os.makedirs(MAILBOX_DIR)

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((host, port))
    server.listen()
    print(f"POP3 server started on {host}:{port}...")

    while True:
        conn, addr = server.accept()
        print(f"Connection from {addr}")
        session = Pop3Session(conn, addr)
        threading.Thread(target=session.handle, daemon=True).start()


if __name__ == "__main__":
    start_server()
