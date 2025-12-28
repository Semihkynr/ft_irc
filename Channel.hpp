#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <map>
#include <set>
#include "Client.hpp"

class Channel {

    private:
        std::string name;
        std::string topic;
        std::string password;
        bool       isPrivate;
        bool       topicSet;
        int        maxUsers;

        std::map<int, Client*> users;
        std::set<int> operators;
        std::set<int> invitedUsers;

    public:
        Channel(const std::string& name, const std::string& password, bool isPrivate, int maxUsers);
        ~Channel();
};




#endif