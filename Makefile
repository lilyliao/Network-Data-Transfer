all: clientmake sendermake
clientrmake : client.cpp packet.h
	g++ -o client client.cpp -I.
servermake : server.cpp packet.h
	g++ -o server server.cpp -I.
clean: clientclean serverclean receivedclean
clientclean :
	rm client
serverclean :
	rm server
receivedclean :
	rm *.out
