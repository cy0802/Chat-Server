#include <iostream>
#include <vector>
#include <algorithm>
#include <utility>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include "User.h"
#include "Chatroom.h"
#define errquit(m) {perror(m); exit(-1);}
#define MAXLINE 200
#define fd first
#define userIdx second
using namespace std;
vector<User> users;
vector<Chatroom> chatrooms;
pair<int, int> client[FD_SETSIZE]; // {sockfd, idx}
fd_set rset, allset;
char sendBuffer[MAXLINE], rcvBuffer[MAXLINE];
string _status[3] = {"online", "offline", "busy"};
void processInput(int sockfd, int idx);
void welcome(int sockfd);
void sendAll(string msg, int roomNum);
bool cmp(int a, int b);
void filter(string& str);
int main(int argc, char* argv[]){
    // cout << "FD_SIZE: " << FD_SETSIZE << "\n";
    if(argc != 2){
        // cout << "Usage: ./hw2_chat_server [port number]\n";
        exit(-1);
    }
    int port = atoi(argv[1]);

    int i, maxi, maxfd, listenfd, connfd, sockfd, nready;
    ssize_t n;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    
    chatrooms.resize(101);
    bzero(&cliaddr, sizeof(cliaddr));
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(port);
    
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) errquit("socket");
    int v = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
    if(bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) errquit("bind");
    if(listen(listenfd, 4096) < 0) errquit("listen");
    
    // initialize
    maxfd = listenfd;
    maxi = -1;
    for (int i = 0; i < FD_SETSIZE; i++){ 
        client[i].fd = -1;
        client[i].userIdx = -1;
    }
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // cout << "listenfd: " << listenfd << "\n";
    while(1){
        rset = allset;
        struct timeval timeout = {5, 0};
        if((nready = select(maxfd+1, &rset, NULL, NULL, &timeout)) < 0) errquit("select");
        
        // new client connection
        if(FD_ISSET(listenfd, &rset)){ 
            clilen = sizeof(cliaddr);
            if((connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen)) < 0) errquit("accept");
            // cout << "client connected at descriptor " << connfd << "\n";
            welcome(connfd);
            for (i = 0; i < FD_SETSIZE; i++){
                if (client[i].fd < 0) {
                    client[i].fd = connfd; // save descriptor
                    break;
                }
            }
            if (i == FD_SETSIZE) errquit("too many client");
            FD_SET(connfd, &allset); 
            if (connfd > maxfd) maxfd = connfd;
            if (i > maxi) maxi = i;
            if (--nready <= 0) continue;
        }
        
        // check client
        for(int i = 0; i <= maxi; i++){
            if((sockfd = client[i].fd) < 0) continue;
            if (FD_ISSET(sockfd, &rset)) {
                // cout << "select sockfd: " << sockfd << "\n";
                bzero(&rcvBuffer, sizeof(rcvBuffer));
                if(n = read(sockfd, rcvBuffer, MAXLINE) == 0){
                    // connection closed by client
                    // cout << "connection close by client at descriptor " << sockfd << "\n";
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    if(client[i].userIdx != -1) users[client[i].userIdx].logout();
                    client[i].fd = -1;
                    client[i].userIdx = -1;
                } else {
                    // process request here
                    if(n < 0) errquit("read");
                    processInput(sockfd, i);
                    // // cout << rcvBuffer << endl;
                    // if(send(sockfd, rcvBuffer, strlen(rcvBuffer), 0) < 0) errquit("write"); 
                }
                if(--nready <= 0) break;
            }
        }
        // end of main loop
    }
    return 0;
}

void welcome(int sockfd){
    bzero(&sendBuffer, sizeof(sendBuffer));
    sprintf(sendBuffer, "*********************************\n** Welcome to the Chat server. **\n*********************************\n%% ");
    if(write(sockfd, sendBuffer, strlen(sendBuffer)) < 0) errquit("write");
}
void processInput(int sockfd, int idx){
    // request already placed in rcvBuffer
    char buf[MAXLINE]; memcpy(buf, rcvBuffer, sizeof(rcvBuffer));
    stringstream ss; ss << buf;
    string command; ss >> command;
    bzero(&sendBuffer, sizeof(sendBuffer));
    bool chatroomMode = client[idx].userIdx == -1 ? false : (users[client[idx].userIdx].curChatroom != -1);
    if(command == "register" && !chatroomMode){
        string username, password, tmp;
        ss >> username >> password >> tmp;
        if(username[0] == '\0' || password[0] == '\0' || tmp[0] != '\0'){
            sprintf(sendBuffer, "Usage: register <username> <password>\n");
        } else {
            // check if the username already used
            bool exist = false;
            for(int i = 0; i < users.size(); i++){
                if(users[i].username == username) {
                    exist = true;
                    break;
                }
            }
            if(exist) {
                sprintf(sendBuffer, "Username is already used.\n");
            } else {
                users.push_back(User(username, password));
                // client[idx].userIdx = users.size()-1;
                sprintf(sendBuffer, "Register successfully.\n");
            }
        }
    } else if(command == "login" && !chatroomMode){
        string username, password, tmp;
        ss >> username >> password >> tmp;
        if(username[0] == '\0' || password[0] == '\0' || tmp[0] != '\0'){
            sprintf(sendBuffer, "Usage: login <username> <password>\n");
        } else if(client[idx].userIdx != -1) {
            sprintf(sendBuffer, "Please logout first.\n");
        } else {
            // find the user
            int user = -1;
            bool pwdCorrect = false;
            for(int i = 0; i < users.size(); i++){
                if(users[i].username == username){
                    user = i;
                    if(users[i].password == password) pwdCorrect = true;
                    break;
                }
            }
            if(user == -1 || pwdCorrect == false || users[user].loggedin == true){
                sprintf(sendBuffer, "Login failed.\n");
            } else {
                client[idx].userIdx = user;
                users[user].sockfd = sockfd;
                users[user].login();
                sprintf(sendBuffer, "Welcome, %s.\n", users[user].username.c_str());
            }
        }
    } else if (command == "logout" && !chatroomMode) {
        string tmp; ss >> tmp;
        if (tmp[0] != '\0') {
            sprintf(sendBuffer, "Usage: logout\n");
        } else if (client[idx].userIdx == -1) {
            sprintf(sendBuffer, "Please login first.\n");
        } else {
            sprintf(sendBuffer, "Bye, %s.\n", users[client[idx].userIdx].username.c_str());
            users[client[idx].userIdx].logout();
            users[client[idx].userIdx].sockfd = -1;
            client[idx].userIdx = -1;
        }
    } else if (command == "exit" && !chatroomMode){
        string tmp; ss >> tmp;
        if(tmp[0] != '\0'){
            sprintf(sendBuffer, "Usage: exit\n");
        } else {
            if(client[idx].userIdx != -1){
                sprintf(sendBuffer, "Bye, %s.\n", users[client[idx].userIdx].username.c_str());
                if(write(sockfd, sendBuffer, strlen(sendBuffer)) < 0) errquit("write");
                users[client[idx].userIdx].logout();
                users[client[idx].userIdx].sockfd = -1;
                client[idx].userIdx = -1;
            }
            FD_CLR(sockfd, &allset);
            close(sockfd);
            client[idx].fd = -1;
            return;
        }
    } else if (command == "whoami" && !chatroomMode) {
        string tmp; ss >> tmp;
        if(tmp[0] != '\0'){
            sprintf(sendBuffer, "Usage: whoami\n");
        } else if (client[idx].userIdx == -1) {
            sprintf(sendBuffer, "Please login first.\n");
        } else {
            sprintf(sendBuffer, "%s\n", users[client[idx].userIdx].username.c_str());
        }
    } else if (command == "set-status" && !chatroomMode) {
        string status, tmp; ss >> status >> tmp;
        if(status[0] == '\0' || tmp[0] != '\0'){
            sprintf(sendBuffer, "Usage: set-status <status>\n");
        } else if (client[idx].userIdx == -1) {
            sprintf(sendBuffer, "Please login first.\n");
        } else if (status != "busy" && status != "online" && status != "offline"){
            sprintf(sendBuffer, "set-status failed\n");
        } else {
            if(status == "busy") users[client[idx].userIdx].status = BUSY;
            if(status == "online") users[client[idx].userIdx].status = ONLINE;
            if(status == "offline") users[client[idx].userIdx].status = OFFLINE;
            sprintf(sendBuffer, "%s %s\n", users[client[idx].userIdx].username.c_str(), status.c_str());
        }
    } else if (command == "list-user" && !chatroomMode) {
        string tmp; ss >> tmp;
        if(tmp[0] != '\0'){
            sprintf(sendBuffer, "Usage: list-user\n");
        } else if (client[idx].userIdx == -1) {
            sprintf(sendBuffer, "Please login first.\n");
        } else {
            vector<int> order;
            for(int i = 0; i < users.size(); i++) order.push_back(i);
            sort(order.begin(), order.end(), cmp);
            for(int i = 0; i < users.size(); i++){
                sprintf(sendBuffer, "%s%s %s\n", sendBuffer, 
                    users[order[i]].username.c_str(), _status[users[order[i]].status].c_str());
            }
        }
    } else if (command == "enter-chat-room" && !chatroomMode) {
        string num, tmp; ss >> num >> tmp;
        int roomNum = (num[0] != '\0' ? atoi(num.c_str()) : -1);
        if(num[0] == '\0' || tmp[0] != '\0'){
            sprintf(sendBuffer, "Usage: enter-chat-room <number>\n");
        } else if (roomNum < 1 || roomNum > 100) {
            sprintf(sendBuffer, "Number %d is not valid.\n", roomNum);
        } else if (client[idx].userIdx == -1) {
            sprintf(sendBuffer, "Please login first.\n");
        } else {
            if (!chatrooms[roomNum].active){
                chatrooms[roomNum].active = true;
                chatrooms[roomNum].owner = users[client[idx].userIdx].username;
                chatrooms[roomNum].id = roomNum;
            }
            sprintf(sendBuffer, "Welcome to the public chat room.\nRoom number: %d\nOwner: %s\n", 
                roomNum, chatrooms[roomNum].owner.c_str());
            // show history
            for(int i = 0; i < chatrooms[roomNum].history.size(); i++){
                sprintf(sendBuffer, "%s%s", sendBuffer, chatrooms[roomNum].history[i].c_str());
            }
            if(chatrooms[roomNum].pinnedMsg[0] != '\0'){
                sprintf(sendBuffer, "%s%s", sendBuffer, chatrooms[roomNum].pinnedMsg.c_str());
            }
            // send msg to all client
            string msg = users[client[idx].userIdx].username + " had enter the chat room.\n";
            sendAll(msg, roomNum);
            // enter the room
            chatrooms[roomNum].users.push_back(client[idx].userIdx);
            users[client[idx].userIdx].curChatroom = roomNum;
            chatroomMode = true;
        }
    } else if (command == "list-chat-room" && !chatroomMode) {
        string tmp; ss >> tmp;
        if(tmp[0] != '\0') {
            sprintf(sendBuffer, "Usage: list-chat-room\n");
        } else if (client[idx].userIdx == -1) {
            sprintf(sendBuffer, "Please login first.\n");
        } else {
            for(int i = 1; i < chatrooms.size(); i++){
                if(chatrooms[i].active){
                    sprintf(sendBuffer, "%s%s %d\n", sendBuffer, chatrooms[i].owner.c_str(), i);
                }
            }
        }
    } else if (command == "close-chat-room" && !chatroomMode) {
        string num, tmp; ss >> num >> tmp;
        int roomNum = (num[0] != '\0' ? atoi(num.c_str()) : -1);
        if(num[0] == '\0' || tmp[0] != '\0'){
            sprintf(sendBuffer, "Usage: close-chat-room <number>\n");
        } else if (client[idx].userIdx == -1) {
            sprintf(sendBuffer, "Please login first.\n");
        } else if (roomNum < 1 || roomNum > 100 || !chatrooms[roomNum].active){
            sprintf(sendBuffer, "Chat room %d does not exist.\n", roomNum);
        } else if (chatrooms[roomNum].owner != users[client[idx].userIdx].username) {
            sprintf(sendBuffer, "Only the owner can close this chat room.\n");
        } else {
            // close the chat room
            string msg = "Chat room " + to_string(roomNum) + " was closed.\n% ";
            sprintf(sendBuffer, "Chat room %d was closed.\n", roomNum);
            sendAll(msg, roomNum);
            // kick out all users
            for(int i = 0; i < chatrooms[roomNum].users.size(); i++){
                users[chatrooms[roomNum].users[i]].curChatroom = -1;
            }
            chatrooms[roomNum].reset();
        }
    } else if (command == "/pin" && chatroomMode) {
        // if users[client[idx].userIdx].curChatroom == -1?
        int roomNum = users[client[idx].userIdx].curChatroom;
        string msg = ss.str();
        msg.erase(0, 5);
        string sendMsg = "Pin -> [" + users[client[idx].userIdx].username + "]: " + msg;
        filter(sendMsg);
        chatrooms[roomNum].pinnedMsg.clear();
        chatrooms[roomNum].pinnedMsg = sendMsg;
        sendAll(sendMsg, roomNum);
    } else if (command == "/delete-pin" && chatroomMode) {
        int roomNum = users[client[idx].userIdx].curChatroom;
        if(chatrooms[roomNum].pinnedMsg[0] == '\0'){
            sprintf(sendBuffer, "No pin message in chat room %d\n", roomNum);
        } else {
            chatrooms[roomNum].pinnedMsg.clear();
        }
    } else if (command == "/exit-chat-room" && chatroomMode) { 
        int roomNum = users[client[idx].userIdx].curChatroom;
        users[client[idx].userIdx].curChatroom = -1;
        chatrooms[roomNum].users.erase(
            find(chatrooms[roomNum].users.begin(), chatrooms[roomNum].users.end(), client[idx].userIdx)
        );
        // cout << "current users in room " << roomNum << ": ";
        for(auto userid: chatrooms[roomNum].users){
            // cout << users[userid].username << " ";
        }
        // cout << "\n";
        string msg = users[client[idx].userIdx].username + " had left the chat room.\n";
        sendAll(msg, roomNum);
        chatroomMode = false;
    } else if (command == "/list-user" && chatroomMode) {
        int roomNum = users[client[idx].userIdx].curChatroom;
        sort(chatrooms[roomNum].users.begin(), chatrooms[roomNum].users.end(), cmp);
        for(int i = 0; i < chatrooms[roomNum].users.size(); i++){
            int _idx = chatrooms[roomNum].users[i];
            sprintf(sendBuffer, "%s%s %s\n", sendBuffer, 
                users[_idx].username.c_str(), _status[users[_idx].status].c_str());
        }
    } else if (client[idx].userIdx != -1 && users[client[idx].userIdx].curChatroom != -1 && command[0] != '/') {
        int roomNum = users[client[idx].userIdx].curChatroom;
        string msg = ss.str(); filter(msg);
        string record = "[" + users[client[idx].userIdx].username + "]: " + msg;
        if(chatrooms[roomNum].history.size() == 10) 
            chatrooms[roomNum].history.erase(chatrooms[roomNum].history.begin()); // pop_front
        chatrooms[roomNum].history.push_back(record);
        sendAll(record, roomNum);
    } else {
        sprintf(sendBuffer, "Error: Unknown command\n");
    }
    // for(int i = 0; i < users.size(); i++) users[i].print();
    if(client[idx].userIdx == -1 || !chatroomMode) sprintf(sendBuffer, "%s%% ", sendBuffer);
    if(sendBuffer[0] != '\0') if(write(sockfd, sendBuffer, strlen(sendBuffer)) < 0) errquit("write");
}
bool cmp(int a, int b){
    return users[a].username < users[b].username;
}
void sendAll(string msg, int chatroomId){
    char buf[MAXLINE]; bzero(&buf, sizeof(buf));
    sprintf(buf, "%s", msg.c_str());
    for(int i = 0; i < chatrooms[chatroomId].users.size(); i++){
        int userid = chatrooms[chatroomId].users[i];
        if(write(users[userid].sockfd, buf, strlen(buf)) < 0) errquit("write");
    }
}
void filter(string& str){
    string keywords[5] = {"==", "Superpie", "hello", "Starburst Stream", "Domain Expansion"};
    for(auto keyword: keywords){
        int kIdx = 0;
        bool finding = false;
        for(int i = 0; i < str.length(); i++){
            if(!finding && (str[i] == keyword[kIdx] ||
                    ('a' <= str[i] && str[i] <= 'z' && str[i] == keyword[kIdx] - 'A' + 'a') ||
                    ('A' <= str[i] && str[i] <= 'Z' && str[i] == keyword[kIdx] - 'a' + 'A'))){
                finding = true;
                kIdx++;
            } else if(finding && kIdx == keyword.length()){
                // found, replace
                kIdx = 0;
                finding = false;
                str.replace(str.begin() + i - keyword.length(), str.begin() + i, keyword.length(), '*');
            } else if(finding && str[i] == keyword[kIdx]){
                kIdx++;
            } else if(finding && str[i] != keyword[kIdx]){
                if('a' <= str[i] && str[i] <= 'z' && str[i] == keyword[kIdx] - 'A' + 'a'){
                    kIdx++;
                } else if('A' <= str[i] && str[i] <= 'Z' && str[i] == keyword[kIdx] - 'a' + 'A'){
                    kIdx++;
                } else {
                    finding = false;
                    kIdx = 0;
                }
            }
        }
    }
}