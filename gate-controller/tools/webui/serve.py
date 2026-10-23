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
        self.ac_sel, self.ac_meas, self.zc = 0, 50, False  # AUTO, line freq
        self.region = 1
        # CT front-end variant: ZMCT103C is PRIMARY (decision 2026);
        # "hall" selectable as legacy bring-up fallback. K_eff mV/A per doc.
        self.ct_sensor = "ZMCT103C"; self.ct_k = 22
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
            # device-identical console grammar: "ct zmct [K]" / "ct hall [K]"
            parts = c.split()
            if len(parts) >= 2 and parts[0] == "ct" and parts[1] in ("zmct", "hall"):
                self.ct_sensor = "ZMCT103C" if parts[1] == "zmct" else "HALL"
                self.ct_k = int(parts[2]) if len(parts) > 2 and parts[2].isdigit() \
                            else (22 if parts[1] == "zmct" else 66)
                cfg_save({"ct_sensor": parts[1], "ct_k_mv_per_a": self.ct_k})
                self.ev("config", f"ct := {self.ct_sensor} K={self.ct_k} mV/A (persisted)")
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
            for key in ("region", "ac"):      # {"ac":0|50|60}, device-identical
                if key in b:
                    r = b[key]
                    sel = 0 if r in ("auto", 0, "0") else int(r)
                    assert sel in (0, 50, 60), "bad region value"
                    self.ac_sel = sel
                    self.region = {0: 0, 50: 1, 60: 2}[sel]
                    self.ac_meas = 60 if sel == 60 else 50   # simulated line
                    cfg_save({"mains_freq": sel, "region_code": self.region})
                    self.ev("config", f"region := {sel or 'auto'} (line {self.ac_meas} Hz, persisted)")
    def status(self):
        with self.L:
            return dict(pos=self.pos, target=self.target, travel=self.travel,
                state=self.state, mdrv=self.mdrv, dir=self.dir,
                i_rms=self.i_rms, inj=int(self.inj),
                batt_v=self.batt_v, batt_pct=self.batt_pct, mains=self.mains,
                lim_closed=self.lim[0], lim_open=self.lim[1], lim_fault=self.lim_fault,
                relays=self.mdrv & 3, relays_ok=True, enc_status=0, log_dropped=0,
                ac_sel=self.ac_sel, ac_meas=self.ac_meas,
                ac_half_us=(1000000 // self.ac_meas) if self.ac_meas else 0,
                zc_sim=self.zc, region=self.region, i_fullscale_a=30.0,
                ct_sensor=self.ct_sensor, ct_k_mv_per_a=self.ct_k)

CFG_PATH = Path(__file__).resolve().parent / "config.json"
def cfg_save(d):
    try:
        cur = json.loads(CFG_PATH.read_text()) if CFG_PATH.exists() else {}
        cur.update(d); CFG_PATH.write_text(json.dumps(cur))
    except Exception: pass

SIM = Sim()
try:
    _c = json.loads(CFG_PATH.read_text()) if CFG_PATH.exists() else {}
    SIM.ac_sel = int(_c.get("mains_freq", 0)); SIM.region = int(_c.get("region_code", 1))
    SIM.ac_meas = 60 if SIM.ac_sel == 60 else 50
    _cts = _c.get("ct_sensor")
    if _cts in ("zmct", "hall"):
        SIM.ct_sensor = "ZMCT103C" if _cts == "zmct" else "HALL"
        SIM.ct_k = int(_c.get("ct_k_mv_per_a", 22 if _cts == "zmct" else 66))
except Exception: pass

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
