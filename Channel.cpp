#include "Channel.hpp"
#include <sys/socket.h> // send için

Channel::Channel(const std::string& name, const std::string& password, bool isPrivate, int maxUsers)
    : name(name),
      topic(),
      password(password),
      isPrivate(isPrivate),
      topicSet(false),
      maxUsers(maxUsers)
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
void Channel::broadcast(const std::string& message, int senderFd) {
    for (std::map<int, Client*>::const_iterator it = users.begin(); it != users.end(); ++it) {
        std::cout << "Send to fd: " << it->first << std::endl;
        if (it->first != senderFd) {
            send(it->first, message.c_str(), message.length(), 0);
        }
    }
}