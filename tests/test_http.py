"""End-to-end loopback API test; launches and terminates only its own child server."""

import json
import socket
import subprocess
import sys
import time
import urllib.request


def run(executable):
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    process = subprocess.Popen(
        [executable, "--port", str(port)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    base = f"http://127.0.0.1:{port}"

    def request(path, payload=None):
        data = None if payload is None else json.dumps(payload).encode()
        req = urllib.request.Request(
            base + path, data=data, headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=2) as response:
            return response.read()

    def state():
        return json.loads(request("/api/state"))

    try:
        for _ in range(150):
            if process.poll() is not None:
                raise RuntimeError(process.stderr.read().decode())
            try:
                state()
                break
            except OSError:
                time.sleep(0.02)
        else:
            raise RuntimeError("Server startup timed out")
        page = request("/").decode()
        assert "SIMULATION ONLY" in page and "Combined drive" in page and 'id="dual"' in page
        request("/api/scenarios/run", {"name": "combined_drive"})
        s = state()
        assert (
            s["dual"]["enabled"]
            and s["dual"]["mg1_command_percent"] > 0
            and s["dual"]["mg2_command_percent"] > 0
        )
        request("/api/fault", {"name": "mg1_timeout", "enabled": True})
        for _ in range(100):
            s = state()
            if s["dual"]["latched"]:
                break
            time.sleep(0.01)
        assert s["dual"]["latched"] and not s["torque_authorized"]
        assert s["dual"]["mg1_command_percent"] == 0 and s["dual"]["mg2_command_percent"] == 0
        request("/api/fault", {"name": "mg1_timeout", "enabled": False})
        assert state()["dual"]["latched"]
        request("/api/scenarios/run", {"name": "combined_regen"})
        assert state()["dual"]["mg1_command_percent"] < 0
        # Header/body deliberately arrive in separate reads.
        body = b'{"action":"reset"}'
        with socket.create_connection(("127.0.0.1", port), timeout=2) as client:
            client.sendall(
                b"POST /api/control HTTP/1.1\r\nHost: localhost\r\ncontent-length: "
                + str(len(body)).encode()
                + b"\r\n\r\n"
            )
            time.sleep(0.01)
            client.sendall(body)
            result = b""
            while True:
                chunk = client.recv(4096)
                if not chunk:
                    break
                result += chunk
        assert b'"ok":true' in result
        assert not state()["dual"]["enabled"]
        print("HTTP UI assets, scenarios, fault latch, regen and fragmented request tests passed.")
    finally:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        process.stderr.close()


if __name__ == "__main__":
    run(sys.argv[1])
