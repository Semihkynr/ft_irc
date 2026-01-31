#include "Server.hpp"

void Server::handlePass(int fd, const std::string& rawParams)
{
    if (_clients.find(fd) == _clients.end() || !_clients[fd])
        return;

    std::string params = trimSpaces(rawParams);
    if(_clients[fd]->isAuthenticated())
    {
        std::cout << "FD " << fd << " - REJECTED: Already authenticated" << std::endl;
        std::string error = "ERROR :You are already authenticated\r\n";
        send(fd, error.c_str(), error.length(), 0);
        return;
    }
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
    if (_clients.find(fd) == _clients.end() || !_clients[fd])
        return;

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

    if (nick.length() < 1 || nick.length() > 9)
    {
        std::string err = ":server 432 * " + nick + " :Erroneous nickname\r\n";
        send(fd, err.c_str(), err.length(), 0);
        return;
    }
    
    if (!((nick[0] >= 'a' && nick[0] <= 'z') || (nick[0] >= 'A' && nick[0] <= 'Z')))
    {
        std::string err = ":server 432 * " + nick + " :Erroneous nickname\r\n";
        send(fd, err.c_str(), err.length(), 0);
        return;
    }
    
    for (size_t i = 0; i < nick.length(); ++i)
    {
        char c = nick[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
            (c >= '0' && c <= '9') || c == '-' || c == '[' || 
            c == ']' || c == '{' || c == '}' || c == '\\' || c == '|' || c == '`' || c == '^'))
        {
            std::string err = ":server 432 * " + nick + " :Erroneous nickname\r\n";
            send(fd, err.c_str(), err.length(), 0);
            return;
        }
    }

    if (isNickInUse(nick, fd))
    {
        std::string err = ":server 433 * " + nick + " :Nickname is already in use\r\n";
        send(fd, err.c_str(), err.length(), 0);
        return;
    }

    std::string oldNick = c->hasNickname() ? c->getNickname() : c->getUsername();
    std::string oldPrefix = makePrefix(c);
    c->setNickname(nick);

    std::string reply = oldPrefix + " NICK :" + nick + "\r\n";
    sendNumeric(fd, reply);

    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        if (it->second && it->second->hasUser(fd)) {
            it->second->broadcast(reply, fd);
        }
    }
    tryRegister(fd);
}


void Server::handleUser(int fd, const std::string& rawParams)
{
    if (_clients.find(fd) == _clients.end() || !_clients[fd])
        return;
    
    Client* c = _clients[fd];
    if (!c)
        return;

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
