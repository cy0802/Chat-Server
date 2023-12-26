#include <string>
#include <iostream>
#include <vector>
#include "User.h"

class Chatroom{
    int id;
    std::string owner;
    std::vector<User*> users;
    std::string pinnedMsg;
    std::string history[10]; // only show top 10 record
    
    
};