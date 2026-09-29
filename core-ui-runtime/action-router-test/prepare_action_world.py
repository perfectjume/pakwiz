import os, signal, subprocess, sys, time

process = subprocess.Popen(
    ["./gradlew", "--no-daemon", "runServer", "--console=plain"],
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True,
    bufsize=1,
    start_new_session=True,
)
ready = False
with open("../action-server-console.log", "w", buffering=1) as log:
    deadline = time.time() + 240
    while time.time() < deadline:
        line = process.stdout.readline()
        if line:
            sys.stdout.write(line)
            log.write(line)
            if "Done (" in line and "For help" in line:
                ready = True
                break
        elif process.poll() is not None:
            break
try:
    os.killpg(process.pid, signal.SIGTERM)
except ProcessLookupError:
    pass
try:
    process.wait(timeout=20)
except subprocess.TimeoutExpired:
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait(timeout=10)
if not ready:
    raise SystemExit("SERVER_WORLD_GENERATION_DID_NOT_REACH_READY")
