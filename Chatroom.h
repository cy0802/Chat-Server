#pragma once
#include <string>
#include <iostream>
#include <vector>
#include "User.h"

class Chatroom{
public:
    int id;
    bool active;
    std::string owner;
    std::vector<int> users;
    std::string pinnedMsg;
    std::vector<std::string> history; // only show top 10 record
    Chatroom(){
        active = false;
        users.clear();
    }
    void reset(){
        active = false;
        owner = "";
        users.clear();
        pinnedMsg = "";
        history.clear();
    }
};