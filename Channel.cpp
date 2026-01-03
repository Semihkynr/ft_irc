/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: teraslan <teraslan@student.42istanbul.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/03 14:12:19 by teraslan          #+#    #+#             */
/*   Updated: 2026/01/03 16:59:33 by teraslan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"
#include <sys/socket.h> // send için

Channel::Channel(const std::string& name, const std::string& password, bool isPrivate, int maxUsers)
    : name(name),
      topic(),
      password(password),
      isPrivate(isPrivate),
      topicSet(false),
      maxUsers(maxUsers),
      inviteOnlyMode(isPrivate),
      topicOperatorOnlyMode(false),
      keyMode(!password.empty()),
      limitMode(maxUsers > 0)
{
}

Channel::~Channel() {
}

bool Channel::hasUser(int fd) const {
    return users.find(fd) != users.end();
}

bool Channel::isOperator(int fd) const {
    return operators.find(fd) != operators.end();
}

bool Channel::isInvited(int fd) const {
    return invitedUsers.find(fd) != invitedUsers.end();
}

void Channel::addUser(int fd, Client* client) {
    if(users.empty()) {
        operators.insert(fd); // İlk kullanıcı operatör olur
    }
    users[fd] = client;
}

void Channel::removeUser(int fd) {
    users.erase(fd);
    operators.erase(fd);
    invitedUsers.erase(fd);
}

//+o mode için
void Channel::addOperator(int fd) {
    operators.insert(fd);
}
//-o mode için
void Channel::removeOperator(int fd) {
    operators.erase(fd);
}

void Channel::inviteUser(int fd) {
    invitedUsers.insert(fd);
}

void Channel::removeInvite(int fd) {
    invitedUsers.erase(fd);
}

void Channel::setTopic(const std::string& newTopic) {
    topic = newTopic;
    topicSet = true; //JOIN  de mesaj basılırken lazım
}

//change or view topic diyo subject de sonra burayı kontrol et
bool Channel::changeTopic(int operatorFd, const std::string& newTopic) {
    if (!canSetTopic(operatorFd))
        return false;
    setTopic(newTopic);
    return true;
}

//broadcast eklenmesi lazım
//kanaldaki herkese mesaj
void Channel::broadcast(const std::string& message, int senderFd) {
    for (std::map<int, Client*>::const_iterator it = users.begin(); it != users.end(); ++it) {
        std::cout << "Send to fd: " << it->first << std::endl;
        if (it->first != senderFd) {
            send(it->first, message.c_str(), message.length(), 0);
        }
    }
}

//channel kuralları

//join kontrolü
bool Channel::isFull() const {
    if (maxUsers <= 0)
        return false; // limit yok
    return users.size() >= static_cast<size_t>(maxUsers);
}

bool Channel::isEmpty() const {
    return users.empty();
}

bool Channel::canJoin(int fd, const std::string& pass) const {
    if (isFull())
        return false;

    if (!password.empty() && pass != password)
        return false;

    if (isPrivate && invitedUsers.find(fd) == invitedUsers.end())
        return false;

    return true;
}

//getterlar
std::string Channel::getName() const {
    return name;
}

std::string Channel::getTopic() const {
    return topic;
}

bool Channel::getIsPrivate() const {
    return isPrivate;
}

bool Channel::getTopicSet() const {
    return topicSet;
}

int Channel::getMaxUsers() const {
    return maxUsers;
}

size_t Channel::getUserCount() const {
    return users.size();
}

const std::map<int, Client*>& Channel::getUsers() const {
    return users;
}

std::string Channel::getModeString() const {
    std::string modes = "+";
    if (inviteOnlyMode)
        modes += "i";
    if (topicOperatorOnlyMode)
        modes += "t";
    if (keyMode)
        modes += "k";
    if (limitMode)
        modes += "l";
    if (modes == "+")
        return "";
    return modes;
}


//operator işlemleri KICK-INVİTE-TOPİC-MODE
bool Channel::canKick(int fd) const {
    return isOperator(fd);
}

bool Channel::canInvite(int fd) const {
    return isOperator(fd);
}

bool Channel::canSetTopic(int fd) const {
    if (!topicOperatorOnlyMode)
        return true;          // +t yoksa herkes
    return isOperator(fd);    // +t varsa sadece OP
}

bool Channel::canChangeMode(int fd) const {
    return isOperator(fd);
}

bool Channel::kickUser(int operatorFd, int targetFd) {
    if (!canKick(operatorFd) || !hasUser(targetFd))
        return false;

    removeUser(targetFd);
    return true;
}

bool Channel::invite(int operatorFd, int targetFd) {
    if (!canInvite(operatorFd))
        return false;
    if (hasUser(targetFd))
        return false;
    if (isInvited(targetFd))
        return false;
    inviteUser(targetFd);
    return true;
}

//MODE

// MODE #channel +i,+t,+k,+l <password> <limit>

bool Channel::applyModeString(int operatorFd, const std::string& modes,
                     const std::vector<std::string>& params) {
    if (!canChangeMode(operatorFd))
        return false;

    bool enable = true;
    size_t paramIndex = 0;

    for (size_t i = 0; i < modes.length(); ++i) {
        char mode = modes[i];
        if (mode == '+') {
            enable = true;
        } else if (mode == '-') {
            enable = false;
        } else {
            std::string param;
            if ((mode == 'k' || mode == 'l') && paramIndex < params.size()) {
                param = params[paramIndex++];
            }
            if (!setMode(operatorFd, mode, enable, param)) {
                return false; // Failed to set mode
            }
        }
    }
    return true;
}

bool Channel::setMode(int operatorFd, char mode, bool enable, const std::string& param) {
    if (!canChangeMode(operatorFd))
        return false;
    switch (mode) {
        case 'i':
            setInviteOnlyMode(enable);
            break;
        case 't':
            setTopicOperatorOnlyMode(enable);
            break;
        case 'k':
            if (enable) {
                password = param;
            } else {
                password.clear();
            }
            setKeyMode(enable);
            break;
        case 'l':
            if (enable) {
                maxUsers = atoi(param.c_str());
            } else {
                maxUsers = 0; // 0 means no limit
            }
            setLimitMode(enable);
            break;
        default:
            return false; // Unknown mode
    }
    return true;
}

//channel modes

void Channel::setInviteOnlyMode(bool mode) {
    inviteOnlyMode = mode;
}

void Channel::setTopicOperatorOnlyMode(bool mode) {
    topicOperatorOnlyMode = mode;
}

void Channel::setKeyMode(bool mode) {
    keyMode = mode;
}

void Channel::setLimitMode(bool mode) {
    limitMode = mode;
}

bool Channel::getInviteOnlyMode() const {
    return inviteOnlyMode;
}

bool Channel::getTopicOperatorOnlyMode() const {
    return topicOperatorOnlyMode;
}

bool Channel::getKeyMode() const {
    return keyMode;
}

bool Channel::getLimitMode() const {
    return limitMode;
}