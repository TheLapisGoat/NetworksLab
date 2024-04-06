all: client server

client: simDNSClient.c
	gcc -o simDNSClient simDNSClient.c

server: simDNSServer.c
	gcc -o simDNSServer simDNSServer.c

clean:
	rm -f simDNSClient simDNSServer