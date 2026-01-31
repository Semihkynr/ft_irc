#include "Server.hpp"

void Server::handlePrivmsg(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c)
        return;

    std::string params = trimSpaces(rawParams);
    if (params.empty())
    {
        sendNumeric(fd, ":server 461 * PRIVMSG :Not enough parameters\r\n");
        return;
    }

    size_t sp = params.find(' ');
    if (sp == std::string::npos)
    {
        sendNumeric(fd, ":server 411 * :No recipient given (PRIVMSG)\r\n");
        return;
    }

    std::string target = trimSpaces(params.substr(0, sp));
    std::string rest   = trimSpaces(params.substr(sp + 1));

    if (target.empty())
    {
        sendNumeric(fd, ":server 411 * :No recipient given (PRIVMSG)\r\n");
        return;
    }

    std::string text;
    if (!rest.empty() && rest[0] == ':')
        text = rest.substr(1);
    else
    {
        size_t colonPos = rest.find(" :");
        if (colonPos != std::string::npos)
            text = rest.substr(colonPos + 2);
        else
            text = rest;
    }

    text = trimSpaces(text);
    if (text.empty())
    {
        sendNumeric(fd, ":server 412 * :No text to send\r\n");
        return;
    }

    std::string msg = makePrefix(c) + " PRIVMSG " + target + " :" + text + "\r\n";

    if (!target.empty() && target[0] == '#')
    {
        std::map<std::string, Channel*>::iterator it = _channels.find(target);
        if (it == _channels.end())
        {
            sendNumeric(fd, ":server 403 " + c->getNickname() + " " + target + " :No such channel\r\n");
            return;
        }

        Channel* ch = it->second;

        if (!ch->hasUser(fd))
        {
            sendNumeric(fd, ":server 404 " + c->getNickname() + " " + target + " :Cannot send to channel\r\n");
            return;
        }

        ch->broadcast(msg, fd);
        return;
    }

    int toFd = findFdByNick(target);

    if (toFd == -1)
    {
        sendNumeric(fd, ":server 401 " + c->getNickname() + " " + target + " :No such nick\r\n");
        return;
    }

    sendNumeric(toFd, msg);
}

void Server::handleNotice(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c)
        return;

    std::string params = trimSpaces(rawParams);
    if (params.empty())
    {
        return;
    }

    size_t sp = params.find(' ');
    if (sp == std::string::npos)
    {
        return;
    }

    std::string target = trimSpaces(params.substr(0, sp));
    std::string rest   = trimSpaces(params.substr(sp + 1));

    if (target.empty())
    {
        return;
    }

    std::string text;
    if (!rest.empty() && rest[0] == ':')
        text = rest.substr(1);
    else
    {
        size_t colonPos = rest.find(" :");
        if (colonPos != std::string::npos)
            text = rest.substr(colonPos + 2);
        else
            text = rest;
    }

    text = trimSpaces(text);
    if (text.empty())
    {
        return;
    }

    std::string msg = makePrefix(c) + " NOTICE " + target + " :" + text + "\r\n";

    if (!target.empty() && target[0] == '#')
    {
        std::map<std::string, Channel*>::iterator it = _channels.find(target);
        if (it == _channels.end())
        {
            return;
        }

        Channel* ch = it->second;

        if (!ch->hasUser(fd))
        {
            return;
        }

        ch->broadcast(msg, fd);
        return;
    }

    int toFd = findFdByNick(target);

    if (toFd == -1)
    {
        return;
    }

    sendNumeric(toFd, msg);
}

void Server::handleQuit(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];

    std::string reason = trimSpaces(rawParams);
    if (!reason.empty() && reason[0] == ':')
        reason = reason.substr(1);
    if (reason.empty())
        reason = "Client Quit";

    std::string quitMsg;
    if (c)
        quitMsg = makePrefix(c) + " QUIT :" + reason + "\r\n";

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
}

void Server::handleWho(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c) return;

    std::string params = trimSpaces(rawParams);
    if (params.empty())
    {
        sendNumeric(fd, ":server 461 " + c->getNickname() + " WHO :Not enough parameters\r\n");
        return;
    }

    std::string target = params;
    size_t sp = target.find(' ');
    if (sp != std::string::npos)
        target = target.substr(0, sp);
    target = trimSpaces(target);

    if (!target.empty() && target[0] == '#')
    {
        std::map<std::string, Channel*>::iterator it = _channels.find(target);
        if (it == _channels.end())
        {
            sendNumeric(fd, ":server 315 " + c->getNickname() + " " + target + " :End of WHO list\r\n");
            return;
        }

        Channel* ch = it->second;
        const std::map<int, Client*>& users = ch->getUsers();

        for (std::map<int, Client*>::const_iterator uit = users.begin(); uit != users.end(); ++uit)
        {
            Client* u = uit->second;
            if (!u || !u->hasNickname()) continue;

            std::string flags = "H";
            if (ch->isOperator(uit->first))
                flags += "@";

            std::string reply = ":server 352 " + c->getNickname() + " " + target + " " +
                              u->getUsername() + " localhost server " + u->getNickname() + " " +
                              flags + " :0 " + u->getUsername() + "\r\n";
            sendNumeric(fd, reply);
        }
        sendNumeric(fd, ":server 315 " + c->getNickname() + " " + target + " :End of WHO list\r\n");
    }
    else
    {
        int targetFd = findFdByNick(target);
        if (targetFd == -1)
        {
            sendNumeric(fd, ":server 315 " + c->getNickname() + " " + target + " :End of WHO list\r\n");
            return;
        }

        Client* targetClient = _clients[targetFd];
        if (targetClient && targetClient->hasNickname())
        {
            std::string flags = "H";
            std::string chanName = "*";
            for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
            {
                if (it->second && it->second->hasUser(targetFd))
                {
                    chanName = it->first;
                    if (it->second->isOperator(targetFd))
                        flags += "@";
                    break;
                }
            }

            std::string reply = ":server 352 " + c->getNickname() + " " + chanName + " " +
                              targetClient->getUsername() + " localhost server " + 
                              targetClient->getNickname() + " " + flags + " :0 " + 
                              targetClient->getUsername() + "\r\n";
            sendNumeric(fd, reply);
        }

        sendNumeric(fd, ":server 315 " + c->getNickname() + " " + target + " :End of WHO list\r\n");
    }
}

void Server::handleWhois(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c) return;

    std::string params = trimSpaces(rawParams);
    if (params.empty())
    {
        sendNumeric(fd, ":server 431 " + c->getNickname() + " :No nickname given\r\n");
        return;
    }

    std::string targetNick = params;
    size_t sp = targetNick.find(' ');
    if (sp != std::string::npos)
        targetNick = targetNick.substr(0, sp);
    targetNick = trimSpaces(targetNick);

    int targetFd = findFdByNick(targetNick);
    if (targetFd == -1)
    {
        sendNumeric(fd, ":server 401 " + c->getNickname() + " " + targetNick + " :No such nick\r\n");
        sendNumeric(fd, ":server 318 " + c->getNickname() + " " + targetNick + " :End of WHOIS list\r\n");
        return;
    }

    Client* targetClient = _clients[targetFd];
    if (!targetClient || !targetClient->hasNickname())
    {
        sendNumeric(fd, ":server 401 " + c->getNickname() + " " + targetNick + " :No such nick\r\n");
        sendNumeric(fd, ":server 318 " + c->getNickname() + " " + targetNick + " :End of WHOIS list\r\n");
        return;
    }

    std::string whoisUser = ":server 311 " + c->getNickname() + " " + targetClient->getNickname() + " " +
                           targetClient->getUsername() + " localhost * :" + targetClient->getUsername() + "\r\n";
    sendNumeric(fd, whoisUser);

    std::string whoisServer = ":server 312 " + c->getNickname() + " " + targetClient->getNickname() + 
                             " server :IRC Server\r\n";
    sendNumeric(fd, whoisServer);

    std::string channels;
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
    {
        if (it->second && it->second->hasUser(targetFd))
        {
            if (!channels.empty())
                channels += " ";
            if (it->second->isOperator(targetFd))
                channels += "@";
            channels += it->first;
        }
    }

    if (!channels.empty())
    {
        std::string whoisChannels = ":server 319 " + c->getNickname() + " " + targetClient->getNickname() + 
                                   " :" + channels + "\r\n";
        sendNumeric(fd, whoisChannels);
    }
    sendNumeric(fd, ":server 318 " + c->getNickname() + " " + targetClient->getNickname() + " :End of WHOIS list\r\n");
}

void Server::handlePing(int fd, const std::string& rawParams) {
    Client* c = _clients[fd];
    if (!c) return;
    
    std::string params = trimSpaces(rawParams);
    std::string msg = ":server PONG :" + (params.empty() ? "server" : params) + "\r\n";
    sendNumeric(fd, msg);
}