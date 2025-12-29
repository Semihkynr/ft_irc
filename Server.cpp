/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilknurhancer <ilknurhancer@student.42.f    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:38:15 by skaynar           #+#    #+#             */
/*   Updated: 2025/12/29 11:46:55 by ilknurhance      ###   ########.fr       */
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


//HELPERS

bool Server::isNickInUse(const std::string& nick, int requesterFd) const
{
    std::map<int, Client*>::const_iterator it = _clients.begin();
    while (it != _clients.end())
    {
        int fd = it->first;
        Client* c = it->second;

        if (fd != requesterFd && c && c->hasNickname() && c->getNickname() == nick)
            return true;

        ++it;
    }
    return false;
}

std::string Server::trimSpaces(const std::string& s)
{
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t'))
        ++start;

    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t'))
        --end;

    return s.substr(start, end - start);
}

std::string Server::toUpper(const std::string& s)
{
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i)
    {
        if (out[i] >= 'a' && out[i] <= 'z')
            out[i] = out[i] - 'a' + 'A';
    }
    return out;
}

void Server::sendWelcome(int fd)
{
    Client* c = _clients[fd];
    if (!c)
        return;

    const std::string& nick = c->getNickname();
    const std::string& user = c->getUsername();

    // Minimal host bilgisi (ileride gerçek host/addr eklenebilir)
    std::string host = "localhost";

    std::string m1 = ":server 001 " + nick + " :Welcome to the IRC Network " +
                     nick + "!" + user + "@" + host + "\r\n";

    std::string m2 = ":server 002 " + nick + " :Your host is server, running version 0.1\r\n";
    std::string m3 = ":server 003 " + nick + " :This server was created today\r\n";
    std::string m4 = ":server 004 " + nick + " server 0.1 o o\r\n";

    send(fd, m1.c_str(), m1.length(), 0);
    send(fd, m2.c_str(), m2.length(), 0);
    send(fd, m3.c_str(), m3.length(), 0);
    send(fd, m4.c_str(), m4.length(), 0);
}

void Server::tryRegister(int fd)
{
    Client* c = _clients[fd];
    if (!c)
        return;

    if (!c->isAuthenticated())
        return;

    if (!c->hasNickname() || !c->hasUsername())
        return;

    if (c->isRegistered())
        return;

    c->setRegistered(true);
    sendWelcome(fd);
}


//COMMANDS
void Server::handlePass(int fd, const std::string& rawParams)
{
    std::string params = trimSpaces(rawParams);

    if (params.empty())
    {
        std::cout << "FD " << fd << " - REJECTED: No password provided" << std::endl;
        std::string error = "ERROR :No password provided\r\n";
        send(fd, error.c_str(), error.length(), 0);
        return;
    }
    if (params == _password)
    {
        _clients[fd]->setAuthenticated(true);
        std::cout << "FD " << fd << " - AUTHENTICATED successfully" << std::endl;
        std::string success = ":server 001 * :Password accepted\r\n";
        send(fd, success.c_str(), success.length(), 0);
    }
    else
    {
        std::cout << "FD " << fd << " - REJECTED: Wrong password" << std::endl;
        std::string error = "ERROR :Invalid password\r\n";
        send(fd, error.c_str(), error.length(), 0);
    }
}

void Server::handleNick(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c)
        return;

    std::string params = trimSpaces(rawParams);
    std::string nick;
    size_t sp = params.find(' ');
    if (sp == std::string::npos)
        nick = params;
    else
        nick = params.substr(0, sp);
    nick = trimSpaces(nick);

    if (nick.empty())
    {
        std::string err = ":server 431 * :No nickname given\r\n";
        send(fd, err.c_str(), err.length(), 0);
        return;
    }

    if (isNickInUse(nick, fd))
    {
        std::string err = ":server 433 * " + nick + " :Nickname is already in use\r\n";
        send(fd, err.c_str(), err.length(), 0);
        return;
    }

    std::string oldPrefix = c->hasNickname() ? c->getNickname() : "*";
    c->setNickname(nick);

    std::string reply = ":" + oldPrefix + " NICK :" + nick + "\r\n";
    send(fd, reply.c_str(), reply.length(), 0);

    tryRegister(fd);
}

void Server::handleUser(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c)
        return;

    // IRC: USER <username> <mode> <unused> :<realname>
    std::string params = trimSpaces(rawParams);
    if (params.empty())
    {
        std::string err = ":server 461 * USER :Not enough parameters\r\n";
        send(fd, err.c_str(), err.length(), 0);
        return;
    }
    
    size_t sp = params.find(' ');
    std::string username = (sp == std::string::npos) ? params : params.substr(0, sp);
    username = trimSpaces(username);

    if (username.empty())
    {
        std::string err = ":server 461 * USER :Not enough parameters\r\n";
        send(fd, err.c_str(), err.length(), 0);
        return;
    }
    
    c->setUsername(username);
    tryRegister(fd);
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
