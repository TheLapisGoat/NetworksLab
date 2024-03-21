all: initmsocket user1 user2

initmsocket: initmsocket.c libmsocket.a
	gcc -o initmsocket initmsocket.c -L. -lmsocket
	
user1: user1.c libmsocket.a
	gcc -o user1 user1.c -L. -lmsocket

user2: user2.c libmsocket.a
	gcc -o user2 user2.c -L. -lmsocket

libmsocket.a: msocket.o msocket.h
	ar -rcs libmsocket.a msocket.o

msocket.o: msocket.c msocket.h
	gcc -c msocket.c

clean: 
	rm -f initmsocket user1 user2 libmsocket.a msocket.o ./ReceiverFiles/*