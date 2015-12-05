all: receivermake sendermake
receivermake : receiver.cpp packet.h
	g++ -o receiver receiver.cpp -I.
sendermake : sender.cpp packet.h
	g++ -o sender sender.cpp -I.
clean: receiverclean senderclean receivedclean
receiverclean :
	rm receiver
senderclean :
	rm sender
receivedclean :
	rm *.out