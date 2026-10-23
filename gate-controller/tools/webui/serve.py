#!/usr/bin/env python3
"""Local twin of the device web UI for development without hardware.

Serves components/webui/assets/index.html and implements the same JSON API
(/api/status, /api/cmd, /api/sim, /api/log) backed by a tiny Python model of
the gate FSM + plant, so the dashboard can be developed in a browser while
firmware catches up. The real device serves the identical page from flash.

    python3 tools/webui/serve.py 8080
"""
import json, re, sys, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HTML = Path(__file__).resolve().parents[2] / "components/webui/assets/index.html"

class Sim:
    def __init__(self):
        self.L = threading.Lock()
        self.pos, self.target, self.travel = 0, 0, 4000
        self.state, self.mdrv, self.dir = "IDLE_CLOSED", 0, 0
        self.i_rms, self.inj = 0.0, False
        self.batt_v, self.batt_pct, self.mains = 12.6, 95, True
        self.lim = [True, False]          # closed engaged at pos 0
        self.lim_fault, self.plant = False, True
        self.log = []
        self.ev("boot", "simulator ready")
    def ev(self, k, m):
        self.log.append({"t": round(time.time() % 100000, 1), "k": k, "m": m})
        del self.log[:-50]
    def cmd(self, c):
        with self.L:
            if c == "open":  self.target, self.state = self.travel, "MOVING_OPEN"; self.ev("gate", "cmd open")
            if c == "close": self.target, self.state = 0, "MOVING_CLOSE"; self.ev("gate", "cmd close")
            if c == "stop":  self.target, self.state, self.dir = self.pos, "IDLE_MID", 0; self.ev("gate", "cmd stop")
    def tick(self):
        while True:
            time.sleep(0.05)
            with self.L:
                moving = self.state.startswith("MOVING")
                if moving and self.plant:
                    d = 3 if self.state == "MOVING_OPEN" else -3
                    self.pos = max(0, min(self.travel, self.pos + d))
                    self.i_rms = 1.4 if not self.inj else self.i_rms
                    self.lim = [self.pos <= 0, self.pos >= self.travel]
                    if self.pos == self.target or (self.target == self.travel and self.lim[1]) or \
                       (self.target == 0 and self.lim[0]):
                        self.state = "IDLE_OPEN" if self.pos >= self.travel else \
                                     ("IDLE_CLOSED" if self.pos <= 0 else "IDLE_MID")
                        self.mdrv, self.dir = 0, 0
                        self.ev("gate", f"arrived pos={self.pos}")
                        self.inj = False
                elif moving:
                    self.mdrv, self.dir = 3, (1 if self.state == "MOVING_OPEN" else 2)
                else:
                    self.mdrv, self.dir = 0, 0
                    if not self.inj: self.i_rms = 0.0
    def sim(self, b):
        with self.L:
            if "btn" in b: self.cmd(b["btn"])
            if "limit" in b: self.lim[0 if b["limit"] == "closed" else 1] = bool(b.get("engaged"))
            if "i" in b:
                self.inj = b["i"] >= 0
                if self.inj: self.i_rms = float(b["i"])
            if "mains" in b: self.mains = bool(b["mains"]); self.ev("power", f"mains {'on' if self.mains else 'LOST'}")
            if "plant" in b: self.plant = bool(b["plant"])
    def status(self):
        with self.L:
            return dict(pos=self.pos, target=self.target, travel=self.travel,
                state=self.state, mdrv=self.mdrv, dir=self.dir,
                i_rms=self.i_rms, inj=int(self.inj),
                batt_v=self.batt_v, batt_pct=self.batt_pct, mains=self.mains,
                lim_closed=self.lim[0], lim_open=self.lim[1], lim_fault=self.lim_fault,
                relays=self.mdrv & 3, relays_ok=True, enc_status=0, log_dropped=0)

SIM = Sim()

class H(BaseHTTPRequestHandler):
    def _send(self, code, ctype, body):
        self.send_response(code); self.send_header("Content-Type", ctype)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers(); self.wfile.write(body)
    def do_GET(self):
        if self.path in ("/", "/index.html"):
            self._send(200, "text/html", HTML.read_bytes())
        elif self.path == "/api/status":
            self._send(200, "application/json", json.dumps(SIM.status()).encode())
        elif self.path == "/api/log":
            self._send(200, "application/json", json.dumps(SIM.log).encode())
        else: self._send(404, "text/plain", b"not found")
    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        try: b = json.loads(self.rfile.read(n) or b"{}")
        except Exception: b = {}
        if self.path == "/api/cmd": SIM.cmd(b.get("cmd", ""))
        elif self.path == "/api/sim": SIM.sim(b)
        self._send(200, "application/json", b'{"ok":true}')
    def log_message(self, *a): pass

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    threading.Thread(target=SIM.tick, daemon=True).start()
    print(f"dev dashboard: http://localhost:{port}/  (device twin, pure-python plant)")
    ThreadingHTTPServer(("0.0.0.0", port), H).serve_forever()
