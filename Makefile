CPPFLAGS = -g -std=c++1y -MD -MP

web-server: httpTransaction.o web-server.o
	g++ $(CPPFLAGS) -o $@ $^
	
web-client: httpTransaction.o web-client.o
	g++ $(CPPFLAGS) -o $@ $^

