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

        //channel kuralları
        bool isFull() const;
        bool canJoin(int fd,const std::string& pass) const;

        //getterlar
        std::string getName() const;
        std::string getTopic() const;
        bool        getIsPrivate() const;
        bool        getTopicSet() const;
        int         getMaxUsers() const;
        size_t      getUserCount() const;
        const std::map<int, Client*>& getUsers() const;

       //operator işlemleri KICK-INVİTE-TOPİC-MODE
        bool canKick(int fd) const;
        bool canInvite(int fd) const;
        bool canSetTopic(int fd) const;
        bool canChangeMode(int fd) const;

        bool kickUser(int operatorFd, int targetFd);
        bool invite(int operatorFd, int targetFd);
        bool changeTopic(int operatorFd, const std::string& newTopic);
        bool changeMode(int operatorFd, bool makePrivate, const std::string& newPassword);

        //channel modes
        /*
            +i : Set/remove Invite-only channel
            +t : Set/remove the restrictions of the TOPIC command to channel operators
            +k : Set/remove the channel key (password)
            +o : Give/take channel operator privilege
            +l : Set/remove the user limit to channel
        */

};




#endif