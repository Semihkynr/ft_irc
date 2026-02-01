/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerHelpers.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: teraslan <teraslan@student.42istanbul.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/31 13:44:50 by ihancer           #+#    #+#             */
/*   Updated: 2026/02/01 14:41:40 by teraslan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

bool Server::isNickInUse(const std::string& nick, int requesterFd) const
{
    std::map<int, Client*>::const_iterator it = _clients.begin();
    while (it != _clients.end())
    {
        int fd = it->first;
        Client* c = it->second;

        if (fd != requesterFd && c && c->hasNickname() && toUpper(c->getNickname()) == toUpper(nick))
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

    std::string host = "localhost";

    std::string m1 = ":server 001 " + nick + " :Welcome to the IRC Network " +
                     nick + "!" + user + "@" + host + "\r\n";

    std::string m2 = ":server 002 " + nick + " :Your host is server, running version 0.1\r\n";
    std::string m3 = ":server 003 " + nick + " :This server was created today\r\n";
    std::string m4 = ":server 004 " + nick + " server 0.1 oiws obtkmlvsn\r\n";

    std::string m5 = ":server 005 " + nick +
        " CHANMODES=b,k,l,imnst PREFIX=(ov)@+ NETWORK=LocalNet :are supported\r\n";

    send(fd, m1.c_str(), m1.length(), 0);
    send(fd, m2.c_str(), m2.length(), 0);
    send(fd, m3.c_str(), m3.length(), 0);
    send(fd, m4.c_str(), m4.length(), 0);
    send(fd, m5.c_str(), m5.length(), 0);
}


void Server::tryRegister(int fd)
{
    Client* c = _clients[fd];
    if (!c)
    {
        c->setRegistered(false);
        return;
    }

    if (!c->isAuthenticated())
    {
        c->setRegistered(false);
        return;
    }
    if (!c->hasNickname() || !c->hasUsername())
    {
        c->setRegistered(false);
        return;
    }
    
    if (!c->isRegistered())
	{
		c->setRegistered(true);
		sendWelcome(fd);
	}
}

std::string Server::makePrefix(Client* c) const
{
    std::string nick = (c && c->hasNickname()) ? c->getNickname() : "*";
    std::string user = (c && c->hasUsername()) ? c->getUsername() : "unknown";
    std::string host = "localhost";
    return ":" + nick + "!" + user + "@" + host;
}

void Server::sendNumeric(int fd, const std::string& msg)
{
    ssize_t result = send(fd, msg.c_str(), msg.length(), 0);
    if (result < 0) {
        std::cerr << "Send failed to fd " << fd << std::endl;
    }
}

int Server::findFdByNick(const std::string& nick) const
{
    std::string lowerNick = toUpper(nick);
    for (std::map<int, Client*>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second && it->second->hasNickname() && 
            toUpper(it->second->getNickname()) == lowerNick)
            return it->first;
    }
    return -1;
}

std::string intToString(int v)
{
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

void Server::removeClientFromAllChannels(int fd, const std::string& quitMsg)
{
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); )
    {
        Channel* ch = it->second;

        if (ch && ch->hasUser(fd))
        {
            if (!quitMsg.empty())
                ch->broadcast(quitMsg, fd);

            bool wasOperator = ch->isOperator(fd);

            ch->removeUser(fd);

            if (wasOperator && !ch->isEmpty())
            {
                ch->promoteNewOperator();
            }

            if (ch->isEmpty())
            {
                delete ch;
                std::map<std::string, Channel*>::iterator eraseIt = it++;
                _channels.erase(eraseIt);
                continue;
            }
        }
        ++it;
    }
}
