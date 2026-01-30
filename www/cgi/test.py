#!/usr/bin/python3
import os
import sys
import time

print("Content-Type: text/html")
print("")

method = os.environ.get("REQUEST_METHOD", "?")
query = os.environ.get("QUERY_STRING", "")
length = int(os.environ.get("CONTENT_LENGTH", "0") or 0)
body = sys.stdin.read(length) if length > 0 else ""

now = time.strftime("%Y-%m-%d %H:%M:%S")

# Simple query parsing without extra deps
params = {}
if query:
    for part in query.split("&"):
        if "=" in part:
            k, v = part.split("=", 1)
        else:
            k, v = part, ""
        params[k] = v

name = params.get("name", "webserv")
count = int(params.get("count", "6") or 6)
if count < 1:
    count = 1
if count > 20:
    count = 20

bar = "?" * count

html = f"""
<!doctype html>
<html lang=\"es\">
<head>
  <meta charset=\"utf-8\">
  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">
  <title>CGI Lab</title>
  <style>
    body{{margin:0;font-family:Arial,sans-serif;background:#0f172a;color:#e2e8f0;}}
    .wrap{{max-width:900px;margin:40px auto;padding:24px;}}
    .card{{background:#111827;border:1px solid #1f2937;border-radius:16px;padding:20px;box-shadow:0 20px 50px rgba(0,0,0,.35);}}
    h1{{margin:0 0 8px;font-size:28px;}}
    .pill{{display:inline-block;background:#1f2937;border:1px solid #334155;color:#93c5fd;border-radius:999px;padding:6px 12px;font-size:12px;}}
    .grid{{display:grid;grid-template-columns:1fr 1fr;gap:16px;}}
    pre{{white-space:pre-wrap;background:#0b1220;border:1px solid #1e293b;border-radius:12px;padding:12px;}}
    .bar{{font-family:monospace;font-size:18px;}}
  </style>
</head>
<body>
  <div class=\"wrap\">
    <div class=\"card\">
      <h1>CGI Lab</h1>
      <div class=\"pill\">{now}</div>

      <div style=\"margin-top:16px\">
        <h3>Din?mico</h3>
        <p>Hola <strong>{name}</strong>. Ajusta la barra con <code>?count=1..20</code>.</p>
        <div class=\"bar\">{bar}</div>
        <p>Ejemplo: <code>/cgi/test.py?name=neo&count=12</code></p>
      </div>

      <div class=\"grid\" style=\"margin-top:16px;\">
        <div>
          <h3>Request</h3>
          <p><strong>Method:</strong> {method}</p>
          <p><strong>Query:</strong> {query or '(none)'}</p>
        </div>
        <div>
          <h3>Body</h3>
          <pre>{body or '(empty)'}</pre>
        </div>
      </div>

      <h3>Entorno</h3>
      <pre>{os.environ.get('SCRIPT_FILENAME','')}\n{os.environ.get('SCRIPT_NAME','')}\n{os.environ.get('PATH_INFO','')}</pre>
    </div>
  </div>
</body>
</html>
"""

print(html)
