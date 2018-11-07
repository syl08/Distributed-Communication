# Distributed-Communication

server
compile: gcc -std=c99 -o server server.c -lpthread
run(default port 12345): ./server
run(configure port): ./server portnumber

client
compile: gcc -o client client.c
run(default portnumber is 12345): ./client localhost portnumber
