#include "Channel.hpp"

Channel::Channel(const std::string& name, const std::string& password, bool isPrivate, int maxUsers)
    : name(name), password(password), isPrivate(isPrivate), maxUsers(maxUsers), topicSet(false) {
}

Channel::~Channel() {
}