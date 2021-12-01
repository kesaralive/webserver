all: main
main: main.c
		gcc -W -Wall -lpthread -o main main.c


clean:
		rm main