# EasyEDA integration

## Status of the cookie/API attempt (2026-10-23)
The provided `easyeda_session` cookie is **valid** (the site returns the logged-in
user JSON for it), but EasyEDA's public REST endpoints for schematic documents are
not reachable from this environment:
* `easyeda.com/api/*` -> 404 HTML (endpoints moved/deprecated; Pro uses private XHRs behind CSRF)
* `pro.easyeda.com/api/*` -> 404 (needs request signing done by their SPA only)
* `wmsc.lcsc.com` / `api.lcsc.com` search -> blocked by Akamai bot protection from datacenter IPs

=> Direct "upload a schematic into your EasyEDA account" is not possible from here.
   It IS possible from your own browser in 2 clicks — see Option B below.

## What we produced instead
`gate_v1_netlist.json` — machine-readable netlist + BOM with LCSC part codes,
covering the full v1 topology agreed in SCHEMATIC.md / CT_WIRING.md:
power tree (HLK-PM10 → buck 3V3 → UPS/LiFePO4 → supervisor), triacs+relays via
ULN2803A, ZMCT103C burden front-end on ADC1_GPIO2, H11AA1 zero-cross on GPIO1,
encoder/limit/button inputs with TVS cells, OV5640 DVP pin map, IR option.

## Option A — KiCad bridge (recommended, scriptable)
1. On your PC: `pip install kicad-cli` ships with KiCad 8.
2. I can generate a `.kicad_sch` + `.net` from this JSON on request (KiCad sexpr
   format is open and fully documented).
3. EasyEDA Pro: File → Import → KiCad (.kicad_pcb/.kicad_sch supported since 6.5).
   Components auto-match by LCSC code if you keep the "lcsc" field in footprints.

## Option B — paste into EasyEDA Std editor (no import needed)
1. Open https://easyeda.com/editor (you are logged in with this cookie).
2. New Project → Schematic.
3. Place parts by searching the LCSC codes from `parts[].lcsc` (e.g. C2829257 =
   ZMCT103C). Nets list gives exact pin-to-pin wiring per page.
4. For PCB: keep clearance/slot notes from `safety_notes[]`.

## Option C — WebSocket bridge to your local editor (full automation later)
EasyEDA Std desktop/extension exposes a localhost WS API (`easyeda.*` JS calls).
If you install the "EasyEDA Editor" chrome extension or desktop app and run a
small tunnel (cloudflared/ngrok), I can drive placement programmatically:
placeNetPort/placeComponent/routeWire calls generated from this same JSON.
Say the word and I'll write the ~60-line bridge server.

## Security note
Rotate/revoke the session after we finish (log out of all sessions at
https://easyeda.com/account/security) — the cookie grants full account access.
