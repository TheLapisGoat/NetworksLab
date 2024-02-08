all: smtpmail client popserver

smtpmail: smtpmail.c
	gcc -Wall -o smtpmail smtpmail.c
client: mailclient.c
	gcc -Wall -o client mailclient.c
popserver: popserver.c
	gcc -Wall -o popserver popserver.c

clean:
	rm -f smtpmail client popserver