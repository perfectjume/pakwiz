import json, socket, time

log = open("action-renderer.log", "w", buffering=1)
server = socket.socket()
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.bind(("127.0.0.1", 37541))
server.listen(5)
server.settimeout(300)
log.write("LISTENING\n")

seen = []
prior = None
actions = {1: "options", 2: "advancements", 3: "statistics"}

try:
    while True:
        try:
            client, addr = server.accept()
        except socket.timeout:
            break
        log.write(f"CONNECTION {addr!r}\n")
        client.settimeout(300)
        client.sendall(b'{"type":"ready","protocol":11,"renderer":"action-runtime-test"}\n')
        buf = b""
        try:
            while True:
                data = client.recv(65536)
                if not data:
                    break
                buf += data
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    if not raw:
                        continue
                    line = raw.decode("utf-8", "replace")
                    log.write("RX " + line + "\n")
                    try:
                        obj = json.loads(line)
                    except Exception:
                        continue
                    if obj.get("type") != "state" or obj.get("screen") != "pause":
                        continue
                    pair = (int(obj.get("session", 0)), int(obj.get("generation", 0)))
                    if pair in seen:
                        continue
                    seen.append(pair)
                    count = len(seen)
                    log.write(f"PAUSE_PAIR {count} {pair[0]}/{pair[1]}\n")

                    time.sleep(0.45)
                    if count == 2 and prior is not None:
                        stale = {
                            "type": "action", "action": "options",
                            "session": prior[0], "generation": prior[1],
                        }
                        client.sendall((json.dumps(stale, separators=(",", ":")) + "\n").encode())
                        log.write(f"STALE_ACTION_SENT options {prior[0]}/{prior[1]}\n")
                        time.sleep(0.30)

                    action = actions.get(count)
                    if action:
                        msg = {
                            "type": "action", "action": action,
                            "session": pair[0], "generation": pair[1],
                        }
                        client.sendall((json.dumps(msg, separators=(",", ":")) + "\n").encode())
                        log.write(f"ACTION_SENT {action} {pair[0]}/{pair[1]}\n")
                    prior = pair
        except Exception as exc:
            log.write(f"CONNECTION_END {exc!r}\n")
        finally:
            try:
                client.close()
            except Exception:
                pass
finally:
    server.close()
    log.close()
