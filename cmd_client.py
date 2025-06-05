import socket             
import time
import sys

args    = sys.argv[1:]
command = " ".join(args) 
command  = " " if len(command) == 0 else command
print(f"Message: '{command}' ")

s    = socket.socket()         
port = 12345

s.connect(('127.0.0.1', port)) 

s.send(command.encode("utf-8"))
responce = s.recv(1024)
print(f"Responce: {responce.decode('utf-8')}")

print("\n")

s.send("disconnect".encode("utf-8"))
responce = s.recv(1024)
print(f"Responce: {responce.decode('utf-8')}")

print("\n")

s.close()     