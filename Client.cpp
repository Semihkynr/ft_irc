/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: skaynar <skaynar@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:42:43 by skaynar           #+#    #+#             */
/*   Updated: 2025/12/24 18:45:27 by skaynar          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

Client::Client(int fd) : _fd(fd), _nickname(""), _username(""), _buffer("") {
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