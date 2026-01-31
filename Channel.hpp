/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: teraslan <teraslan@student.42istanbul.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/03 14:12:31 by teraslan          #+#    #+#             */
/*   Updated: 2026/01/31 12:35:25 by teraslan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <map>
#include <set>
#include <iostream>
#include <sys/socket.h>
#include <cstdlib>
#include "Client.hpp"
#include <vector>

class Channel {

    private:
        std::string name;
        std::string topic;
        std::string password;

        bool       topicSet;
        int        maxUsers;
        bool       inviteOnlyMode;
        bool       topicOperatorOnlyMode;
        bool       keyMode;
        bool       limitMode;

        std::map<int, Client*> users;
        std::set<int> operators;
        std::set<int> invitedUsers;

    public:
        Channel(const std::string& name, const std::string& password, int maxUsers);
        ~Channel();

        bool hasUser(int fd) const;
        bool isOperator(int fd) const;
        bool isInvited(int fd) const;

        void addUser(int fd,Client* client);
        void removeUser(int fd);
        void promoteNewOperator();

        void addOperator(int fd);
        void removeOperator(int fd);

        void inviteUser(int fd);
        void removeInvite(int fd);

        void setTopic(const std::string& newTopic);
        bool changeTopic(int operatorFd, const std::string& newTopic);

        void broadcast(const std::string& message, int senderFd);

        bool isFull() const;
        bool isEmpty() const;
        bool canJoin(int fd,const std::string& pass) const;

        std::string getName() const;
        std::string getTopic() const;
        bool        getIsPrivate() const;
        bool        getTopicSet() const;
        int         getMaxUsers() const;
        size_t      getUserCount() const;
        const std::map<int, Client*>& getUsers() const;
        std::string getModeString() const;

        bool canKick(int fd) const;
        bool canInvite(int fd) const;
        bool canSetTopic(int fd) const;
        bool canChangeMode(int fd) const;

        bool kickUser(int operatorFd, int targetFd);
        bool invite(int operatorFd, int targetFd);

        bool setMode(int operatorFd, char mode, bool enable, const std::string& param);
        bool applyModeString(int operatorFd, const std::string& modes,
                     const std::vector<std::string>& params);

        void setInviteOnlyMode(bool mode);
        void setTopicOperatorOnlyMode(bool mode);
        void setKeyMode(bool mode);
        void setLimitMode(bool mode);

        bool getInviteOnlyMode() const;
        bool getTopicOperatorOnlyMode() const;
        bool getKeyMode() const;
        bool getLimitMode() const;


};




#endif