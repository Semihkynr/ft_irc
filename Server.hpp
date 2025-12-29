/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilknurhancer <ilknurhancer@student.42.f    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:38:07 by skaynar           #+#    #+#             */
/*   Updated: 2025/12/29 13:00:15 by ilknurhance      ###   ########.fr       */
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
#include "Client.hpp"
#include "Channel.hpp"

class Server {
private:
    int                         _port;
    int                         _serverFd;
    std::string                 _password;
    std::vector<struct pollfd>  _pollFds;
    std::map<int, Client*>      _clients; // FD'ye göre Client nesnesine hızlı erişim
    std::map<std::string, Channel*> _channels; //elimizdeki channel listesine erişmek içim

    void    setupServerSocket();
    void    acceptNewClient();
    void    handleClientData(int fd);
    void    processCommand(int fd, std::string message);
    
    void handlePass(int fd, const std::string& params);
    void handleNick(int fd, const std::string& params);
    void handleUser(int fd, const std::string& params);
    void handleJoin(int fd, const std::string& params);
    void handlePrivmsg(int fd, const std::string& params);

    //void handlePing(int fd, const std::string& params);
    void handleQuit(int fd, const std::string& params);

    // Helpers
    bool isNickInUse(const std::string& nick, int requesterFd) const;
    static std::string trimSpaces(const std::string& s);
    static std::string toUpper(const std::string& s);
    void tryRegister(int fd);
    void sendWelcome(int fd);
    std::string makePrefix(Client* c) const;
    void        sendNumeric(int fd, const std::string& msg);




public:
    Server(int port, std::string password);
    ~Server();

    void    init();
    void    run();
};

#endif