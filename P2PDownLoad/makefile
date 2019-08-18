all:server client
server:Server.cpp
	g++ Server.cpp -o server -lboost_thread -lboost_system -lboost_filesystem -lpthread -std=c++11 -g
client:Client.cpp
	g++ Client.cpp -o client -lboost_thread -lboost_system -lboost_filesystem -lpthread -std=c++11 -g
.PHONY:clean
clean:
	rm -f server 
	rm -f client

