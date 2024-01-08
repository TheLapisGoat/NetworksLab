all: wordclient wordserver

wordclient: wordclient.c
	gcc -o wordclient wordclient.c

wordserver: wordserver.c
	gcc -o wordserver wordserver.c

clean:
	rm -f wordclient wordserver