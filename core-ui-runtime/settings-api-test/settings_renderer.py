import json
import pathlib
import socket
import time

log = open("settings-renderer.log", "w", buffering=1)
server = socket.socket()
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.bind(("127.0.0.1", 37541))
server.listen(5)
server.settimeout(300)
log.write("LISTENING\n")

pause_pairs = []
baseline = None
targets = None
stage = "waiting"
queue = []
restore_queue = []
options_path = pathlib.Path("mdk/run/options.txt")

def send(client, obj):
    raw = json.dumps(obj, separators=(",", ":")) + "\n"
    client.sendall(raw.encode())
    log.write("TX " + raw)

def setting_msg(pair, key, value):
    return {
        "type": "setting_set",
        "key": key,
        "value": str(value).lower() if isinstance(value, bool) else str(value),
        "session": pair[0],
        "generation": pair[1],
    }

def action_msg(pair, action):
    return {
        "type": "action",
        "action": action,
        "session": pair[0],
        "generation": pair[1],
    }

def next_int(value, minimum, maximum):
    if value < maximum:
        return value + 1
    if value > minimum:
        return value - 1
    raise RuntimeError("integer option has no alternate value")

def check_ranges(state):
    for key in ("fov", "gui_scale", "render_distance", "simulation_distance", "framerate_limit"):
        lo = int(state[key + "_min"])
        hi = int(state[key + "_max"])
        val = int(state[key])
        if not lo <= val <= hi:
            raise RuntimeError(f"{key} value {val} outside {lo}..{hi}")
        log.write(f"RANGE_PASS {key} {lo}..{hi} current={val}\n")

def check_subtitle_persistence(expected):
    if not options_path.exists():
        raise RuntimeError("options.txt missing")
    wanted = "showSubtitles:" + str(expected).lower()
    text = options_path.read_text(errors="replace")
    if wanted not in text.splitlines():
        raise RuntimeError(f"options.txt missing {wanted}")
    log.write("PERSISTENCE_SUBTITLES=PASS " + wanted + "\n")

def start_mutations(client, pair, state):
    global baseline, targets, queue, restore_queue, stage
    baseline = {
        "subtitles": bool(state["subtitles"]),
        "auto_jump": bool(state["auto_jump"]),
        "fov": int(state["fov"]),
        "render_distance": int(state["render_distance"]),
        "graphics_mode": str(state["graphics_mode"]),
    }
    graphics = ["fast", "fancy", "fabulous"]
    current_graphics = baseline["graphics_mode"]
    target_graphics = graphics[(graphics.index(current_graphics) + 1) % len(graphics)]
    targets = {
        "subtitles": not baseline["subtitles"],
        "auto_jump": not baseline["auto_jump"],
        "fov": next_int(baseline["fov"], int(state["fov_min"]), int(state["fov_max"])),
        "render_distance": next_int(
            baseline["render_distance"],
            int(state["render_distance_min"]),
            int(state["render_distance_max"]),
        ),
        "graphics_mode": target_graphics,
    }
    queue = list(targets.items())
    restore_queue = list(baseline.items())
    stage = "mutating"
    key, value = queue.pop(0)
    log.write("BEGIN_MUTATIONS baseline=" + json.dumps(baseline, sort_keys=True) + "\n")
    send(client, setting_msg(pair, key, value))

def handle_settings_state(client, pair, state):
    global stage, queue, restore_queue
    reason = str(state.get("reason", ""))
    log.write("SETTINGS_STATE reason=" + reason + " " + json.dumps(state, sort_keys=True) + "\n")

    if stage == "waiting" and len(pause_pairs) == 1 and reason == "request":
        check_ranges(state)
        start_mutations(client, pair, state)
        return

    if stage == "mutating" and reason.startswith("set-"):
        key = reason[4:]
        expected = targets.get(key)
        if expected is None:
            raise RuntimeError("unexpected mutation response " + key)
        actual = state[key]
        if isinstance(expected, bool):
            actual = bool(actual)
        elif isinstance(expected, int):
            actual = int(actual)
        else:
            actual = str(actual)
        if actual != expected:
            raise RuntimeError(f"mutation mismatch {key}: {actual!r} != {expected!r}")
        log.write(f"MUTATION_PASS {key}={actual}\n")
        if key == "subtitles":
            check_subtitle_persistence(expected)
        if queue:
            next_key, next_value = queue.pop(0)
            send(client, setting_msg(pair, next_key, next_value))
        else:
            stage = "restoring"
            time.sleep(0.35)
            next_key, next_value = restore_queue.pop(0)
            send(client, setting_msg(pair, next_key, next_value))
        return

    if stage == "restoring" and reason.startswith("set-"):
        key = reason[4:]
        expected = baseline.get(key)
        if expected is None:
            raise RuntimeError("unexpected restore response " + key)
        actual = state[key]
        if isinstance(expected, bool):
            actual = bool(actual)
        elif isinstance(expected, int):
            actual = int(actual)
        else:
            actual = str(actual)
        if actual != expected:
            raise RuntimeError(f"restore mismatch {key}: {actual!r} != {expected!r}")
        log.write(f"RESTORE_PASS {key}={actual}\n")
        if restore_queue:
            next_key, next_value = restore_queue.pop(0)
            send(client, setting_msg(pair, next_key, next_value))
        else:
            stage = "wait_second"
            log.write("ALL_RESTORED=PASS\n")
            time.sleep(0.35)
            send(client, action_msg(pair, "resume"))
        return

    if stage == "stale_check" and len(pause_pairs) == 2 and reason == "request":
        for key, expected in baseline.items():
            actual = state[key]
            if isinstance(expected, bool):
                actual = bool(actual)
            elif isinstance(expected, int):
                actual = int(actual)
            else:
                actual = str(actual)
            if actual != expected:
                raise RuntimeError(f"stale session changed {key}: {actual!r} != {expected!r}")
        log.write("STALE_SETTING_PROTOCOL_REJECTION=PASS\n")
        stage = "done"
        send(client, action_msg(pair, "resume"))

try:
    while True:
        try:
            client, addr = server.accept()
        except socket.timeout:
            break
        log.write(f"CONNECTION {addr!r}\n")
        client.settimeout(300)
        send(client, {"type": "ready", "protocol": 11, "renderer": "settings-runtime-test"})
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

                    if obj.get("type") == "state" and obj.get("screen") == "pause":
                        pair = (int(obj["session"]), int(obj["generation"]))
                        if pair not in pause_pairs:
                            pause_pairs.append(pair)
                            log.write(f"PAUSE_PAIR {len(pause_pairs)} {pair[0]}/{pair[1]}\n")
                            if len(pause_pairs) == 1:
                                send(client, {
                                    "type": "settings_request",
                                    "session": pair[0],
                                    "generation": pair[1],
                                })
                            elif len(pause_pairs) == 2:
                                stale_pair = pause_pairs[0]
                                stale_value = not baseline["subtitles"]
                                send(client, setting_msg(stale_pair, "subtitles", stale_value))
                                log.write(
                                    f"STALE_SETTING_SENT subtitles={stale_value} "
                                    f"{stale_pair[0]}/{stale_pair[1]} while_active={pair[0]}/{pair[1]}\n"
                                )
                                time.sleep(0.5)
                                stage = "stale_check"
                                send(client, {
                                    "type": "settings_request",
                                    "session": pair[0],
                                    "generation": pair[1],
                                })

                    elif obj.get("type") == "settings_state":
                        pair = (int(obj["session"]), int(obj["generation"]))
                        handle_settings_state(client, pair, obj)

        except Exception as exc:
            log.write(f"CONNECTION_ERROR {exc!r}\n")
            raise
        finally:
            try:
                client.close()
            except Exception:
                pass

        if stage == "done":
            break
finally:
    server.close()
    log.close()
