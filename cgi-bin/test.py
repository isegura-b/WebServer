#!/usr/bin/python3
import os
import sys

print("Content-Type: text/html\r\n\r\n", end="")
print("<h1 style='color:green;'>CGI works!</h1>")

keys = sorted(os.environ.keys())
for key in keys:
    print(f"<tr><td><b>{key}</b></td><td>{os.environ[key]}</td></tr>")
print("</table>")

print("<h2>POST Body (STDIN)</h2>")
if os.environ.get('REQUEST_METHOD') == 'POST':
    try:
        content_length = os.environ.get('CONTENT_LENGTH')
        if content_length:
            length = int(content_length)
            body = sys.stdin.read(length) 
            print(f"<pre style='background:#eee; padding:10px;'>{body}</pre>")
        else:
            print("<i>No Content-Length received.</i>")
    except Exception as e:
        print(f"<i>Error reading stdin: {e}</i>")
else:
    print("<i>This was a GET request (no body).</i>")

print("</body></html>")
