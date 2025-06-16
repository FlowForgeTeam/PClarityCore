import socket             
import time
import sys

s    = socket.socket()         
port = 12345

s.connect(('127.0.0.1', port)) 

while(True):
    s.send("{\"command_id\": 0, \"extra\": {}}".encode("utf-8"))
    responce = s.recv(500_000)












