#include <string>
#include <iostream>
enum Status {
    ONLINE,
    OFFLINE,
    BUSY
};
class User{
public:
    std::string username;
    std::string password;
    bool loggedin;
    // curChatroom = -1: out of chatroom
    int curChatroom;
    Status status;

    User(std::string _username, std::string _password){
        username = _username;
        password = _password;
        curChatroom = -1;
        loggedin = true;
        status = ONLINE;
    }
    void print(){
        std::cout << "Username: " << username << "\npassword: " << password
            << "\ncurrent chatroom: " << curChatroom << "\nlog in status: " << loggedin 
            << "\nstatus: " << (status == ONLINE ? "online" : (status == OFFLINE ? "offline" : "busy"));
    }
};