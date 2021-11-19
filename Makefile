all: simpleserver
simpleserver: simpleserver.c
		gcc -W -Wall -lpthread -o simpleserver simpleserver.c


clean:
		rm simpleserver