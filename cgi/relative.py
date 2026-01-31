#!/usr/bin/python3
import os
print("Content-Type: text/html\r\n\r\n")
try:
    # This file doesn't need to exist, we just want to see WHERE python looks for it
    with open("does_not_exist.txt", "r") as f:
        pass
except FileNotFoundError:
    # Print the Current Working Directory
    print(f"CWD: {os.getcwd()}")
