import errno, socket, struct, sys

HOST = "127.0.0.1"
PORT = 9000
MAX_MESSAGE = 1024 * 1024

def recv_exact(sock, size):
    data = bytearray()
    while len(data) < size:
        try:
            chunk = sock.recv(size - len(data))
        except InterruptedError:
            continue
        except OSError as exc:
            if exc.errno == errno.EINTR:
                continue
            raise
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
        except OSError as exc:
            if exc.errno == errno.EINTR:
                continue
            raise
        if n == 0:
            raise ConnectionError("send() РІРµСЂРЅСѓР» 0")
        sent += n

def send_frame(sock, text):
    body = text.encode("utf-8")
    if len(body) > MAX_MESSAGE:
        raise ValueError("СЃРѕРѕР±С‰РµРЅРёРµ СЃР»РёС€РєРѕРј Р±РѕР»СЊС€РѕРµ")
    send_all(sock, struct.pack("!I", len(body)))
    send_all(sock, body)

def recv_frame(sock):
    header = recv_exact(sock, 4)
    if header is None:
        return None
    length = struct.unpack("!I", header)[0]
    if length > MAX_MESSAGE:
        raise ValueError("РѕС‚РІРµС‚ СЃР»РёС€РєРѕРј Р±РѕР»СЊС€РѕР№")
    body = recv_exact(sock, length)
    return None if body is None else body.decode("utf-8")

def main():
    expressions = sys.argv[1:] or ["2 + 3 * 4", "(10 - 2) / 4"]
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            try:
                sock.connect((HOST, PORT))
            except OSError as exc:
                if exc.errno == errno.ECONNREFUSED:
                    print("ERR: СЃРµСЂРІРµСЂ РЅРµРґРѕСЃС‚СѓРїРµРЅ (ECONNREFUSED)")
                    return 2
                raise
            for expression in expressions:
                send_frame(sock, expression)
                answer = recv_frame(sock)
                if answer is None:
                    print("ERR: СЃРµСЂРІРµСЂ Р·Р°РєСЂС‹Р» СЃРѕРµРґРёРЅРµРЅРёРµ Р±РµР· РѕС‚РІРµС‚Р°")
                    return 3
                print(f"{expression} = {answer}")
        return 0
    except (ConnectionError, OSError, ValueError, UnicodeDecodeError) as exc:
        print(f"ERR: {exc}")
        return 1

if __name__ == "__main__":
    raise SystemExit(main())