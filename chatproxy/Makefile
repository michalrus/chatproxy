all:
	gcc -Wall -O2 -c -o obj/main.o main.c
	gcc -Wall -O2 -rdynamic -lytils -ldl -o obj/chatproxy \
		obj/main.o

clean:
	rm obj/*.o obj/chatproxy
