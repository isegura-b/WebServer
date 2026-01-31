#!/usr/bin/python3
import os
print("Content-Type: text/html\r\n\r\n")
try:
    with open("does_not_exist.txt", "r") as f:
        pass
except FileNotFoundError:
    print(f"CWD: {os.getcwd()}")
