all: file_client file_server

file_client: file_client.c
	gcc -o file_client file_client.c

file_server: file_server.c
	gcc -o file_server file_server.c

clean:
	rm -f file_client file_server *.enc *.dec