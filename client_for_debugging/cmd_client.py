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
responce = s.recv(100_000)
print(f"Responce: {responce.decode('utf-8')}")

file = open("client_for_debugging\\response.txt", 'w', encoding="utf-8")
file.write(responce.decode("utf-8"))
file.close()

print("=========================================")

s.send("{\"command_id\": 1, \"extra\": {}}".encode("utf-8"))
responce = s.recv(100_000)
print(f"Responce: {responce.decode('utf-8')}")

print("\n")

s.close()     