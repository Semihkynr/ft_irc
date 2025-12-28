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

//channel kuralları

bool Channel::isFull() const {
    return users.size() >= static_cast<size_t>(maxUsers);
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

//operator işlemleri KICK-INVİTE-TOPİC-MODE
bool Channel::canKick(int fd) const {
    return isOperator(fd);
}

bool Channel::canInvite(int fd) const {
    return isOperator(fd);
}

bool Channel::canSetTopic(int fd) const {
    return isOperator(fd);
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

//change or view topic diyo subject de sonra burayı kontrol et
bool Channel::changeTopic(int operatorFd, const std::string& newTopic) {
    if (!canSetTopic(operatorFd))
        return false;
    setTopic(newTopic);
    return true;
}

//burda sadece +i ve +k değişiyo sonra kontrol et
//sonrasında mode ları ayırmak lazım
bool Channel::changeMode(int operatorFd, bool makePrivate, const std::string& newPassword) {
    if (!canChangeMode(operatorFd))
        return false;

    isPrivate = makePrivate;
    password = newPassword;
    return true;
}

