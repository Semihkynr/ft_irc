/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilknurhancer <ilknurhancer@student.42.f    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:38:15 by skaynar           #+#    #+#             */
/*   Updated: 2025/12/29 12:59:38 by ilknurhance      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

Server::Server(int port, std::string password) : _port(port), _password(password) {}

Server::~Server()
{
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        delete it->second;

    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        delete it->second;

    if (_serverFd > 0)
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








//PROCESS COMMAND
void Server::processCommand(int fd, std::string message)
{
    if (message.empty())
        return;

    if (!message.empty() && message[message.size() - 1] == '\r')
        message.erase(message.size() - 1);

    message = trimSpaces(message);
    if (message.empty())
        return;

    size_t spacePos = message.find(' ');
    std::string command = (spacePos == std::string::npos) ? message : message.substr(0, spacePos);
    std::string params  = (spacePos == std::string::npos) ? ""      : message.substr(spacePos + 1);

    command = toUpper(command);
    params  = trimSpaces(params);

    if (command == "PASS")
    {
        handlePass(fd, params);
        return;
    }
    
    //PASS olmadan diğerleri yok
    if (!_clients[fd]->isAuthenticated())
    {
        std::cout << "FD " << fd << " - BLOCKED: Not authenticated (tried: " << message << ")" << std::endl;
        std::string error = "ERROR :You must authenticate with PASS first\r\n";
        send(fd, error.c_str(), error.length(), 0);
        return;
    }
    
    //Nick ve Username ayarlanmadan diğer komutların çalışması da engelleniyor.
    if (command != "NICK" && command != "USER" && command != "QUIT" && command != "PING")
    {
        if (!_clients[fd]->hasNickname() || !_clients[fd]->hasUsername())
        {
            std::cout << "FD " << fd << " - BLOCKED: Not registered (tried: " << message << ")" << std::endl;
            std::string err = ":server 451 * :You have not registered\r\n";
            send(fd, err.c_str(), err.length(), 0);
            return;
        }
    }
    
    // Komutu ilgili handler'a gönder
    std::cout << "FD " << fd << " [AUTHENTICATED] sent: " << message << std::endl;

    if      (command == "NICK")    handleNick(fd, params);
    else if (command == "USER")    handleUser(fd, params);
    else if (command == "JOIN")    handleJoin(fd, params);
    else if (command == "PRIVMSG") handlePrivmsg(fd, params);
    else if (command == "PING")    handlePing(fd, params);
    else if (command == "QUIT")    handleQuit(fd, params);
    else
    {
        std::string error = "ERROR :Unknown command\r\n";
        send(fd, error.c_str(), error.length(), 0);
    }
}
