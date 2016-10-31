CPPFLAGS = -g -std=c++1y -MD -MP

all: web-server web-client

web-server: httpTransaction.o web-server.o
	g++ $(CPPFLAGS) -o $@ $^
	
web-client: httpTransaction.o web-client.o
	g++ $(CPPFLAGS) -o $@ $^

