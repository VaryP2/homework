import ast, operator, socket, struct, sys

HOST = "127.0.0.1"
PORT = 9000
MAX_MESSAGE = 1024 * 1024

OPS = {
    ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul,
    ast.Div: operator.truediv, ast.UAdd: operator.pos, ast.USub: operator.neg,
}

def recv_exact(sock, size):
    data = bytearray()
    while len(data) < size:
        try:
            chunk = sock.recv(size - len(data))
        except InterruptedError:
            continue
        if not chunk:
            if not data:
                return None
            raise ConnectionError("СЃРѕРµРґРёРЅРµРЅРёРµ Р·Р°РєСЂС‹С‚Рѕ РІРЅСѓС‚СЂРё СЃРѕРѕР±С‰РµРЅРёСЏ")
        data.extend(chunk)
    return bytes(data)

def send_all(sock, data):
    sent = 0
    while sent < len(data):
        try:
            n = sock.send(data[sent:])
        except InterruptedError:
            continue
        if n == 0:
            raise ConnectionError("send() РІРµСЂРЅСѓР» 0")
        sent += n

def recv_frame(sock):
    header = recv_exact(sock, 4)
    if header is None:
        return None
    length = struct.unpack("!I", header)[0]
    if length > MAX_MESSAGE:
        raise ValueError("СЃРѕРѕР±С‰РµРЅРёРµ СЃР»РёС€РєРѕРј Р±РѕР»СЊС€РѕРµ")
    body = recv_exact(sock, length)
    if body is None:
        raise ConnectionError("РЅРµС‚ С‚РµР»Р° СЃРѕРѕР±С‰РµРЅРёСЏ")
    return body.decode("utf-8")

def send_frame(sock, text):
    body = text.encode("utf-8")
    if len(body) > MAX_MESSAGE:
        raise ValueError("РѕС‚РІРµС‚ СЃР»РёС€РєРѕРј Р±РѕР»СЊС€РѕР№")
    send_all(sock, struct.pack("!I", len(body)))
    send_all(sock, body)

def evaluate(node):
    if isinstance(node, ast.Expression):
        return evaluate(node.body)
    if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
        return node.value
    if isinstance(node, ast.BinOp) and type(node.op) in OPS:
        return OPS[type(node.op)](evaluate(node.left), evaluate(node.right))
    if isinstance(node, ast.UnaryOp) and type(node.op) in OPS:
        return OPS[type(node.op)](evaluate(node.operand))
    raise ValueError("СЂР°Р·СЂРµС€РµРЅС‹ С‚РѕР»СЊРєРѕ С‡РёСЃР»Р° Рё РѕРїРµСЂР°С†РёРё + - * / СЃРѕ СЃРєРѕР±РєР°РјРё")

def calculate(expression):
    expression = expression.strip()
    if not expression:
        return "ERR: РїСѓСЃС‚РѕР№ Р·Р°РїСЂРѕСЃ"
    try:
        result = evaluate(ast.parse(expression, mode="eval"))
        if isinstance(result, float) and result.is_integer():
            result = int(result)
        return str(result)
    except ZeroDivisionError:
        return "ERR: РґРµР»РµРЅРёРµ РЅР° РЅРѕР»СЊ"
    except (SyntaxError, ValueError, TypeError, OverflowError):
        return "ERR: РЅРµРєРѕСЂСЂРµРєС‚РЅРѕРµ РІС‹СЂР°Р¶РµРЅРёРµ"

def serve_client(conn, addr):
    print(f"accepted {addr[0]}:{addr[1]}", flush=True)
    with conn:
        while True:
            try:
                expression = recv_frame(conn)
                if expression is None:
                    print("client closed", flush=True)
                    return
                print(f"request: {expression!r}", flush=True)
                answer = calculate(expression)
                send_frame(conn, answer)
                print(f"response: {answer!r}", flush=True)
            except InterruptedError:
                continue
            except (ConnectionError, ValueError, UnicodeDecodeError) as exc:
                print(f"connection error: {exc}", flush=True)
                return
            except OSError as exc:
                if getattr(exc, "errno", None) == 4:
                    continue
                print(f"socket error: {exc}", flush=True)
                return

def main():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            server.bind((HOST, PORT))
            server.listen(5)
        except OSError as exc:
            print(f"server error: {exc}", file=sys.stderr)
            return 1
        print(f"listening on {HOST}:{PORT}", flush=True)
        while True:
            try:
                conn, addr = server.accept()
            except InterruptedError:
                continue
            except OSError as exc:
                if getattr(exc, "errno", None) == 4:
                    continue
                print(f"accept error: {exc}", file=sys.stderr)
                return 1
            serve_client(conn, addr)

if __name__ == "__main__":
    raise SystemExit(main())