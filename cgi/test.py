#!/usr/bin/python3
import os
import sys

# --- 1. SEND HEADERS ---
# The server expects \r\n\r\n to separate headers from body
print("Content-Type: text/html\r\n\r\n", end="")

# --- 2. HTML OUTPUT ---
print("<html><head><title>CGI Test</title></head><body>")
print("<h1 style='color:green;'>CGI works!</h1>")

# --- 3. DUMP ENVIRONMENT VARIABLES ---
# This verifies PATH_INFO, QUERY_STRING, REMOTE_ADDR, etc.
print("<h2>Environment Variables</h2>")
print("<table border='1' cellpadding='5'>")
keys = sorted(os.environ.keys())
for key in keys:
    print(f"<tr><td><b>{key}</b></td><td>{os.environ[key]}</td></tr>")
print("</table>")

# --- 4. ECHO POST BODY (STDIN) ---
# This verifies that your server correctly piped the request body to the child process
print("<h2>POST Body (STDIN)</h2>")
if os.environ.get('REQUEST_METHOD') == 'POST':
    try:
        content_length = os.environ.get('CONTENT_LENGTH')
        if content_length:
            length = int(content_length)
            # Read exactly 'length' bytes
            body = sys.stdin.read(length) 
            print(f"<pre style='background:#eee; padding:10px;'>{body}</pre>")
        else:
            print("<i>No Content-Length received.</i>")
    except Exception as e:
        print(f"<i>Error reading stdin: {e}</i>")
else:
    print("<i>This was a GET request (no body).</i>")

print("</body></html>")
