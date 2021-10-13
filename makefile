all: server c/client

	sudo ifconfig en0 192.168.0.1 up
	sudo ifconfig en1 192.168.1.112 up
	echo "2組虛擬IP：192.168.0.[1-2]"
server:server.o
	g++ server.o -o server
server.o:server.cpp
	g++ -c server.cpp
c/client:client.o
	g++ client.o -o c/client
client.o:client.cpp
	g++ -c client.cpp
clean:
	rm -f server c/client *.o
