/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilknurhancer <ilknurhancer@student.42.f    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:38:07 by skaynar           #+#    #+#             */
/*   Updated: 2026/01/31 15:53:12 by ilknurhance      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <sstream>
#include "Client.hpp"
#include "Channel.hpp"

class Server {
private:
    int                         _port;
    int                         _serverFd;
    std::string                 _password;
    std::vector<struct pollfd>  _pollFds;
    std::map<int, Client*>      _clients;
    std::map<std::string, Channel*> _channels;

    void    acceptNewClient();
    void    handleClientData(int fd);
    void    processCommand(int fd, std::string message);

    void handlePass(int fd, const std::string& params);
    void handleNick(int fd, const std::string& params);
    void handleUser(int fd, const std::string& params);
    void handleJoin(int fd, const std::string& params);
    void handlePrivmsg(int fd, const std::string& params);
    void handleNotice(int fd, const std::string& params);
    void handleNames(int fd, const std::string& params);
    void handlePart(int fd, const std::string& params);
    void handleQuit(int fd, const std::string& params);

    void handleMode(int fd, const std::string& params);
    void handleList(int fd, const std::string& params);
    void handleKick(int fd, const std::string& params);
    void handleInvite(int fd, const std::string& params);
    void handleTopic(int fd, const std::string& params);
    void handleWho(int fd, const std::string& params);
    void handleWhois(int fd, const std::string& params);
    void handlePing(int fd, const std::string& params);

    bool isNickInUse(const std::string& nick, int requesterFd) const;
    static std::string trimSpaces(const std::string& s);
    static std::string toUpper(const std::string& s);
    void tryRegister(int fd);
    void sendWelcome(int fd);
    std::string makePrefix(Client* c) const;
    void        sendNumeric(int fd, const std::string& msg);
    int findFdByNick(const std::string& nick) const;
    void removeClientFromAllChannels(int fd, const std::string& quitMsg);

public:
    Server(int port, std::string password);
    ~Server();

    void    init();
    void    run();
};

std::string intToString(int v);

#endif