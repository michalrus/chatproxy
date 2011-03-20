C=g++ -g -Wall -O2 -c -o

all:
	${C} object/Chat.o src/Chat.cc
	${C} object/ChatOnet.o src/ChatOnet.cc
	${C} object/ChatWP.o src/ChatWP.cc
	${C} object/Config.o src/Config.cc
	${C} object/Debug.o src/Debug.cc
	${C} object/Http.o src/Http.cc
	${C} object/main.o src/main.cc
	${C} object/Session.o src/Session.cc
	${C} object/Tcp.o src/Tcp.cc

	g++ -Wall -O2 -o chatproxy3/chatproxy3 \
		object/Chat.o \
		object/ChatOnet.o \
		object/ChatWP.o \
		object/Config.o \
		object/Debug.o \
		object/Http.o \
		object/main.o \
		object/Session.o \
		object/Tcp.o \
		-Wl,-Bstatic \
			-lcurl \
			-lssh2 \
			-lboost_system \
			-lboost_filesystem \
			-lboost_regex \
			-lboost_thread \
			-lstdc++ \
		-Wl,-Bdynamic \
			-lldap \
			-lgssapi_krb5 \
			-lssl \
			-lidn \
			-lpthread \
			-lrt
	strip chatproxy3/chatproxy3
