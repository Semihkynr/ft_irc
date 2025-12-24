/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: teraslan <teraslan@student.42istanbul.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:38:15 by skaynar           #+#    #+#             */
/*   Updated: 2025/12/24 20:05:14 by teraslan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

Server::Server(int port, std::string password) : _port(port), _password(password) {}

Server::~Server() {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        delete it->second;
        close(it->first);
    }
    close(_serverFd);
}

void Server::init() {
    _serverFd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    fcntl(_serverFd, F_SETFL, O_NONBLOCK);

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_port);

    if (bind(_serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) exit(1);
    listen(_serverFd, SOMAXCONN);

    struct pollfd serverPoll = {_serverFd, POLLIN, 0};
    _pollFds.push_back(serverPoll);
}

void Server::run() {
    while (true) {
        if (poll(&_pollFds[0], _pollFds.size(), -1) < 0) break;

        for (size_t i = 0; i < _pollFds.size(); i++) {
            if (_pollFds[i].revents & POLLIN) {
                if (_pollFds[i].fd == _serverFd)
                    acceptNewClient();
                else
                    handleClientData(_pollFds[i].fd);
            }
        }
    }
}

void Server::acceptNewClient() {
    int clientFd = accept(_serverFd, NULL, NULL);
    if (clientFd != -1) {
        fcntl(clientFd, F_SETFL, O_NONBLOCK);
        struct pollfd pfd = {clientFd, POLLIN, 0};
        _pollFds.push_back(pfd);
        _clients[clientFd] = new Client(clientFd);
    }
}

void Server::handleClientData(int fd) {
    char buf[512];
    int bytes = recv(fd, buf, sizeof(buf) - 1, 0);

    if (bytes <= 0) {
        close(fd);
        delete _clients[fd];
        _clients.erase(fd);
        for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it) {
            if (it->fd == fd) { _pollFds.erase(it); break; }
        }
    } else {
        buf[bytes] = '\0';
        _clients[fd]->addBuffer(buf);
        std::string currentBuffer = _clients[fd]->getBuffer();

        size_t pos;
        while ((pos = currentBuffer.find("\n")) != std::string::npos) {
            std::string cmd = currentBuffer.substr(0, pos);
            processCommand(fd, cmd); // burada komutu işleyeceğiz
            currentBuffer.erase(0, pos + 1);
        }
        _clients[fd]->clearBuffer();
        _clients[fd]->addBuffer(currentBuffer); // Kalan parça varsa geri koy
    }
}

void Server::processCommand(int fd, std::string message) {
    if (message.empty())
        return;
    
    // Komutu ve parametreleri ayır
    size_t spacePos = message.find(' ');
    std::string command = message.substr(0, spacePos);
    std::string params = "";
    if (spacePos != std::string::npos)
        params = message.substr(spacePos + 1);
    
    // PASS komutu - kimlik doğrulama
    if (command == "PASS") {
        if (params.empty()) {
			//std::cout lar kontrol için eklendi
            std::cout << "FD " << fd << " - REJECTED: No password provided" << std::endl;
            std::string error = "ERROR :No password provided\r\n";
            send(fd, error.c_str(), error.length(), 0);
            return;
        }
        if (params == _password) {
            _clients[fd]->setAuthenticated(true);
            std::cout << "FD " << fd << " - AUTHENTICATED successfully" << std::endl;
            std::string success = ":server 001 * :Password accepted\r\n";
            send(fd, success.c_str(), success.length(), 0);
        } else {
            std::cout << "FD " << fd << " - REJECTED: Wrong password" << std::endl;
            std::string error = "ERROR :Invalid password\r\n";
            send(fd, error.c_str(), error.length(), 0);
        }
        return;
    }
    
    // Diğer tüm komutlar için kimlik doğrulama kontrolü
    if (!_clients[fd]->isAuthenticated()) {
        std::cout << "FD " << fd << " - BLOCKED: Not authenticated (tried: " << message << ")" << std::endl;
        std::string error = "ERROR :You must authenticate with PASS first\r\n";
        send(fd, error.c_str(), error.length(), 0);
        return;
    }
    
    // Buraya geldi ise authenticated demektir
    std::cout << "FD " << fd << " [AUTHENTICATED] sent: " << message << std::endl;
    
    // Burası diğer komutların işleneceği yer (NICK, USER, JOIN, PRIVMSG, vb.)
    // Şimdilik sadece echo yapıyoruz
    std::string response = "Command received: " + message + "\r\n";
    send(fd, response.c_str(), response.length(), 0);
}