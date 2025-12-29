#include "Server.hpp"

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

std::string Server::makePrefix(Client* c) const
{
    std::string nick = (c && c->hasNickname()) ? c->getNickname() : "*";
    std::string user = (c && c->hasUsername()) ? c->getUsername() : "unknown";
    std::string host = "localhost";
    return ":" + nick + "!" + user + "@" + host;
}

void Server::sendNumeric(int fd, const std::string& msg)
{
    send(fd, msg.c_str(), msg.length(), 0);
}