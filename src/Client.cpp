/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ihancer <ihancer@student.42istanbul.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:42:43 by skaynar           #+#    #+#             */
/*   Updated: 2026/01/31 13:28:09 by ihancer          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Client.hpp"

Client::Client(int fd)
    : _fd(fd),
      _nickname(""),
      _username(""),
      _buffer(""),
      _authenticated(false),
      _registered(false)
{}

Client::~Client() {}

int Client::getFd() const {
    return _fd;
}

void Client::addBuffer(const std::string& str) {
    _buffer += str;
}

std::string Client::getBuffer() const {
    return _buffer;
}

void Client::clearBuffer() {
    _buffer.clear();
}

bool Client::isAuthenticated() const {
    return _authenticated;
}

void Client::setAuthenticated(bool auth) {
    _authenticated = auth;
}

bool Client::isRegistered() const {
    return _registered;
}

void Client::setRegistered(bool reg) {
    _registered = reg;
}

void Client::setNickname(const std::string& nick) {
    _nickname = nick;
}

const std::string& Client::getNickname() const {
    return _nickname;
}

bool Client::hasNickname() const {
    return !_nickname.empty();
}

void Client::setUsername(const std::string& user) {
    _username = user;
}

const std::string& Client::getUsername() const {
    return _username;
}

bool Client::hasUsername() const {
    return !_username.empty();
}
