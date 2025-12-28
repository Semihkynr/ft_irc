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

        bool hasUser(int fd) const;
        bool isOperator(int fd) const;
        bool isInvited(int fd) const;

        void addUser(int fd,Client* client);
        void removeUser(int fd);

        void addOperator(int fd);
        void removeOperator(int fd);

        void inviteUser(int fd);

        void setTopic(const std::string& newTopic);

        //broadcast eklenmesi lazım
        void broadcast(const std::string& message, int senderFd);
};




#endif