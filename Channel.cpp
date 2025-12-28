#include "Channel.hpp"

Channel::Channel(const std::string& name, const std::string& password, bool isPrivate, int maxUsers)
    : name(name), password(password), isPrivate(isPrivate), maxUsers(maxUsers), topicSet(false) {
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
    users[fd] = client;
}

void Channel::removeUser(int fd) {
    users.erase(fd);
    operators.erase(fd);
    invitedUsers.erase(fd);
}

void Channel::addOperator(int fd) {
    operators.insert(fd);
}

void Channel::removeOperator(int fd) {
    operators.erase(fd);
}

void Channel::inviteUser(int fd) {
    invitedUsers.insert(fd);
}

void Channel::setTopic(const std::string& newTopic) {
    topic = newTopic;
    topicSet = true;
}

//broadcast eklenmesi lazım