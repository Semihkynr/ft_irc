/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: skaynar <skaynar@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:38:07 by skaynar           #+#    #+#             */
/*   Updated: 2025/12/24 18:38:08 by skaynar          ###   ########.fr       */
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

class Server {
private:
    int                         _port;
    int                         _serverFd;
    std::string                 _password;
    std::vector<struct pollfd>  _pollFds;
    std::map<int, Client*>      _clients; // FD'ye göre Client nesnesine hızlı erişim

    void    setupServerSocket();
    void    acceptNewClient();
    void    handleClientData(int fd);
    void    processCommand(int fd, std::string message);

public:
    Server(int port, std::string password);
    ~Server();

    void    init();
    void    run();
};

#endif