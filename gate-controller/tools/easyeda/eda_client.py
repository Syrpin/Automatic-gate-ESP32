#!/usr/bin/env python3
"""EasyEDA/LCSC cookie-based API client.

Reads cookies from environment variable EASYEDA_COOKIES (raw "k=v; k2=v2"
header string) or from a Netscape-format cookies file (--cookies-file).

Endpoints used:
  * LCSC search index (public, no auth): https://datasheet.lcsc.com/api/search?...
  * EasyEDA user info   (needs easyeda_session): https://easyeda.com/api/user/info
  * EasyEDA project ops (needs easyeda_session + csrf): https://easyeda.com/api/...

Usage:
  eda_client.py whoami
  eda_client.py part ZMCT103C
  eda_client.py export <project_uuid> -o out.easm
"""
import argparse, json, os, sys, urllib.parse, urllib.request

UA = ("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/126.0 Safari/537.36")

def load_cookies(args):
    if args.cookies_file and os.path.exists(args.cookies_file):
        # raw header-style file
        with open(args.cookies_file) as f:
            return f.read().strip()
    return os.environ.get("EASYEDA_COOKIES", "")

def get(url, cookies="", referer="https://easyeda.com/"):
    req = urllib.request.Request(url, headers={
        "User-Agent": UA,
        "Accept": "application/json, text/plain, */*",
        "Referer": referer,
    })
    if cookies:
        req.add_header("Cookie", cookies)
    with urllib.request.urlopen(req, timeout=20) as r:
        return r.status, r.read().decode("utf-8", "replace")

def cmd_whoami(cookies):
    st, body = get("https://easyeda.com/api/user/info", cookies)
    print(f"HTTP {st}")
    try:
        j = json.loads(body)
        d = j.get("result", j)
        print(json.dumps(d, indent=2, ensure_ascii=False)[:2000])
    except Exception:
        print(body[:500])

def cmd_part(q):
    url = ("https://search.lcsc.com/api/search?keyword="
           + urllib.parse.quote(q) + "&limit=10&offset=0&type=1")
    st, body = get(url)
    j = json.loads(body)
    prods = (((j.get("result") or {}).get("data")) or [])
    if not prods:
        print("no results"); return
    for p in prods[:10]:
        print(f"{p.get('product_code','')} | {p.get('description','')[:90]}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cookies-file")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("whoami")
    pp = sub.add_parser("part"); pp.add_argument("query")
    args = ap.parse_args()
    cookies = load_cookies(args)
    if args.cmd == "whoami":
        if not cookies:
            sys.exit("EASYEDA_COOKIES not set — paste the Cookie header string.")
        cmd_whoami(cookies)
    elif args.cmd == "part":
        cmd_part(args.query)

if __name__ == "__main__":
    main()
