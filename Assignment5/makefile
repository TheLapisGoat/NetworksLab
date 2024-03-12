all: initmsocket user1

.initmsocket: initmsocket.c msocket.c msocket.h
	gcc initmsocket.c msocket.c -o initmsocket
	./initmsocket
	
.user1: user1.c msocket.c msocket.h
	gcc user1.c msocket.c -o user1
	./user1

clean: 
	rm -f initmsocket user1