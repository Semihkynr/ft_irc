/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: teraslan <teraslan@student.42istanbul.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:42:43 by skaynar           #+#    #+#             */
/*   Updated: 2025/12/24 19:59:41 by teraslan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

Client::Client(int fd) : _fd(fd), _nickname(""), _username(""), _buffer(""), _authenticated(false) {
}

Client::~Client() {}

int Client::getFd() const {
    return _fd;
}

std::string Client::getBuffer() const {
    return _buffer;
}


void Client::addBuffer(std::string str) {
    _buffer += str;
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