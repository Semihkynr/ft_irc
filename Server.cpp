/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: teraslan <teraslan@student.42istanbul.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:38:15 by skaynar           #+#    #+#             */
/*   Updated: 2026/01/31 12:12:10 by teraslan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <cstring>   // std::memset
#include <stdexcept> // std::runtime_error

Server::Server(int port, std::string password)
    : _port(port), _serverFd(-1), _password(password) {}

Server::~Server()
{
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        delete it->second;
    _channels.clear();

    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        int fd = it->first;
        close(fd);
        delete it->second;
    }
    _clients.clear();

    if (_serverFd >= 0)
        close(_serverFd);

    std::cout << "[SHUTDOWN]: Tüm bağlantılar kesildi ve bellek temizlendi." << std::endl;
}

static void setNonBlockingOrThrow(int fd)
{
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl(O_NONBLOCK) failed");
}

void Server::init()
{
    _serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverFd < 0)
        throw std::runtime_error("socket() failed");

    int opt = 1;
    if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");

    setNonBlockingOrThrow(_serverFd);

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_port);

    if (bind(_serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed");

    if (listen(_serverFd, SOMAXCONN) < 0)
        throw std::runtime_error("listen() failed");

    struct pollfd serverPoll;
    serverPoll.fd = _serverFd;
    serverPoll.events = POLLIN;
    serverPoll.revents = 0;

    _pollFds.push_back(serverPoll);
}

void Server::run()
{
    while (true)
    {
        int ret = poll(&_pollFds[0], _pollFds.size(), -1);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue; // signal ile bölündüyse devam
            break;       // gerçek hata
        }

    for (size_t i = 0; i < _pollFds.size(); ++i)
    {
        if (!(_pollFds[i].revents & POLLIN))
            continue;

        int curFd = _pollFds[i].fd;

        if (curFd == _serverFd)
            acceptNewClient();
        else
            handleClientData(curFd);
        // _pollFds değişmiş olabilir (disconnect/accept). Güvenli olmak için çık.
        break;
    }

    }
}

void Server::acceptNewClient()
{
    while (true)
    {
        int clientFd = accept(_serverFd, NULL, NULL);
        if (clientFd == -1)
        {
            // Non-blocking accept: kuyruk boşsa EAGAIN/EWOULDBLOCK gelir, normal
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            // Diğer hatalar loglanabilir, ama loop'u kırmak yeterli
            break;
        }

        // Yeni client fd non-blocking
        try {
            setNonBlockingOrThrow(clientFd);
        } catch (...) {
            close(clientFd);
            continue;
        }

        struct pollfd pfd;
        pfd.fd = clientFd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        _pollFds.push_back(pfd);
        _clients[clientFd] = new Client(clientFd);
    }
}

void Server::handleClientData(int fd)
{
    char buf[512];

    while (true)
    {
        int bytes = recv(fd, buf, sizeof(buf) - 1, 0);

        if (bytes == 0)
        {
            // Client bağlantıyı kapattı (EOF)
            Client* c = (_clients.find(fd) != _clients.end()) ? _clients[fd] : NULL;
            std::string quitMsg;
            if (c)
                quitMsg = makePrefix(c) + " QUIT :Connection closed\r\n";

            // NEW: önce kanallardan çıkar + kanal boşsa sil
            removeClientFromAllChannels(fd, quitMsg);

            close(fd);
            if (_clients.find(fd) != _clients.end())
            {
                delete _clients[fd];
                _clients.erase(fd);
            }

            for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it)
            {
                if (it->fd == fd) { _pollFds.erase(it); break; }
            }
            return;
        }
        else if (bytes < 0)
        {
            // Non-blocking recv: veri yoksa EAGAIN/EWOULDBLOCK normal
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;

            // Gerçek hata -> disconnect
            Client* c = (_clients.find(fd) != _clients.end()) ? _clients[fd] : NULL;
            std::string quitMsg;
            if (c)
                quitMsg = makePrefix(c) + " QUIT :Read error\r\n";

            // NEW: önce kanallardan çıkar + kanal boşsa sil
            removeClientFromAllChannels(fd, quitMsg);

            close(fd);
            if (_clients.find(fd) != _clients.end())
            {
                delete _clients[fd];
                _clients.erase(fd);
            }

            for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it)
            {
                if (it->fd == fd) { _pollFds.erase(it); break; }
            }
            return;
        }

        // bytes > 0
        buf[bytes] = '\0';

        // fd map'te yoksa güvenli çık (normalde olmamalı ama crash önler)
        if (_clients.find(fd) == _clients.end() || !_clients[fd])
            return;

        _clients[fd]->addBuffer(buf);
        std::string currentBuffer = _clients[fd]->getBuffer();

        size_t pos;
        while ((pos = currentBuffer.find("\n")) != std::string::npos)
        {
            std::string cmd = currentBuffer.substr(0, pos);
            processCommand(fd, cmd);
            if (_clients.find(fd) == _clients.end())
                return;
            currentBuffer.erase(0, pos + 1);
        }
        if (_clients.find(fd) == _clients.end())
            return;
        _clients[fd]->clearBuffer();
        _clients[fd]->addBuffer(currentBuffer);
    }
}


//PROCESS COMMAND
void Server::processCommand(int fd, std::string message)
{
    if (message.empty())
        return;

    // Check map first
    if (_clients.find(fd) == _clients.end() || !_clients[fd])
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

    if (!_clients[fd]->isAuthenticated())
    {
        std::cout << "FD " << fd << " - BLOCKED: Not authenticated (tried: " << message << ")" << std::endl;
        std::string error = "ERROR :You must authenticate with PASS first\r\n";
        send(fd, error.c_str(), error.length(), 0);
        return;
    }

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

    std::cout << "FD " << fd << " [AUTHENTICATED] sent: " << message << std::endl;

    if      (command == "NICK")    handleNick(fd, params);
    else if (command == "USER")    handleUser(fd, params);
    else if (command == "JOIN")    handleJoin(fd, params);
    else if (command == "PRIVMSG") handlePrivmsg(fd, params);
    else if (command == "NOTICE")  handleNotice(fd, params);
    else if (command == "NAMES")   handleNames(fd, params);
    else if (command == "LIST")    handleList(fd, params);
    else if (command == "PART")    handlePart(fd, params);
    else if (command == "MODE")   handleMode(fd, params);
    else if (command == "KICK")   handleKick(fd, params);
    else if (command == "INVITE") handleInvite(fd, params);
    else if (command == "TOPIC")  handleTopic(fd, params);
    else if (command == "WHO")    handleWho(fd, params);
    else if (command == "WHOIS")  handleWhois(fd, params);
    else if (command == "PING")    handlePing(fd, params);
    else if (command == "QUIT")    handleQuit(fd, params);
    else
    {
        std::string error = "ERROR :Unknown command\r\n";
        send(fd, error.c_str(), error.length(), 0);
    }
}

