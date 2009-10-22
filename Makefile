BUILD=g++ -g -Wall -O2 -c -o

all:
	obj/buildnum
	${BUILD} obj/main.o src/main.cc
	${BUILD} obj/AuthKey.o src/AuthKey.cc
	${BUILD} obj/Config.o src/Config.cc
	${BUILD} obj/Session.o src/Session.cc
	${BUILD} obj/Tcp.o src/Tcp.cc
	${BUILD} obj/Chat.o src/Chat.cc
	${BUILD} obj/ChatOnet.o src/ChatOnet.cc
	${BUILD} obj/ChatWP.o src/ChatWP.cc
	g++ -Wall -O2 -o chatproxy2 \
		obj/main.o \
		obj/AuthKey.o \
		obj/Config.o \
		obj/Session.o \
		obj/Tcp.o \
		obj/Chat.o \
		obj/ChatOnet.o \
		obj/ChatWP.o \
		-Wl,-Bstatic -lcurl -lcryptopp -lGeoIP \
		-lboost_regex-mt -lboost_iostreams-mt -lboost_system-mt -lboost_filesystem-mt -lboost_thread-mt \
		-Wl,-Bdynamic -lssl -lpthread -lrt

clean:
	rm chatproxy2 obj/*.o
