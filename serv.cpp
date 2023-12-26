#include <iostream>
#include <vector>
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
#define errquit(m) {perror(m); exit(-1);}
#define MAXLINE 200
#define fd first
#define userIdx second
using namespace std;
vector<User> users;
pair<int, int> client[FD_SETSIZE]; // {sockfd, idx}
char sendBuffer[MAXLINE], rcvBuffer[MAXLINE];
void processInput(int sockfd, int idx);
void welcome(int sockfd);
int main(int argc, char* argv[]){
    if(argc != 2){
        cout << "Usage: ./hw2_chat_server [port number]\n";
        exit(-1);
    }
    int port = atoi(argv[1]);

    int i, maxi, maxfd, listenfd, connfd, sockfd, nready;
    ssize_t n;
    fd_set rset, allset;
    
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    bzero(&cliaddr, sizeof(cliaddr));
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(port);
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) errquit("socket");
    int v = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
    if(bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) errquit("bind");
    if(listen(listenfd, SOMAXCONN) < 0) errquit("listen");
    
    // initialize
    maxfd = listenfd;
    maxi = -1;
    for (int i = 0; i < FD_SETSIZE; i++){ 
        client[i].fd = -1;
        client[i].userIdx = -1;
    }
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    cout << "listenfd: " << listenfd << "\n";
    while(1){
        rset = allset;
        if((nready = select(maxfd+1, &rset, NULL, NULL, NULL)) < 0) errquit("select");
        
        // new client connection
        if(FD_ISSET(listenfd, &rset)){ 
            clilen = sizeof(cliaddr);
            if((connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen)) < 0) errquit("accept");
            cout << "client connected at descriptor " << connfd << "\n";
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
                cout << "select sockfd: " << sockfd << "\n";
                bzero(&rcvBuffer, sizeof(rcvBuffer));
                if(n = read(sockfd, rcvBuffer, MAXLINE) == 0){
                    // connection closed by client
                    cout << "connection close by client at descriptor " << sockfd << "\n";
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    client[i].fd = -1;
                    client[i].userIdx = -1;
                } else {
                    // process request here
                    if(n < 0) errquit("read");
                    processInput(sockfd, i);
                    // cout << rcvBuffer << endl;
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
    sprintf(sendBuffer, "*********************************\n** Welcome to the Chat server. **\n*********************************\n");
    if(write(sockfd, sendBuffer, strlen(sendBuffer)) < 0) errquit("write");
}
void processInput(int sockfd, int idx){
    // request already placed in rcvBuffer
    char buf[MAXLINE]; memcpy(buf, rcvBuffer, sizeof(rcvBuffer));
    stringstream ss; ss << buf;
    string command; ss >> command;
    bzero(&sendBuffer, sizeof(sendBuffer));
    if(command == "register"){
        string username, password;
        ss >> username >> password;
        if(username[0] == '\0' || password[0] == '\0'){
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
    } 
    if(write(sockfd, sendBuffer, strlen(sendBuffer)) < 0) errquit("write");
}