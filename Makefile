all: serv.cpp User.h
	g++ -o hw2_chat_server serv.cpp User.h Chatroom.h