CC=g++

.PHONY :all
all: parser server http
server: server.cc
	$(CC) -o $@ $^ -ljsoncpp   -std=c++11
http: http_server.cc
	$(CC) -o $@ $^ -ljsoncpp -lpthread -lboost_system -lboost_filesystem -std=c++11
parser: parser.cpp
	$(CC) -o $@ $^  -lboost_system -lboost_filesystem -std=c++11
.PHONY :clean
clean:
	rm -f parser server http


