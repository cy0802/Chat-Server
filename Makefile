all: hw2_chat_server.cpp User.h
	g++ -o hw2_chat_server hw2_chat_server.cpp User.h Chatroom.h