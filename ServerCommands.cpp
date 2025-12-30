#include "Server.hpp"

void Server::handlePass(int fd, const std::string& rawParams)
{
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

void Server::handleJoin(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c)
        return;

    std::string params = trimSpaces(rawParams);
    if (params.empty())
    {
        sendNumeric(fd, ":server 461 * JOIN :Not enough parameters\r\n");
        return;
    }

    std::string chanList;
    std::string keyList;

    size_t sp = params.find(' ');
    if (sp == std::string::npos)
        chanList = params;
    else
    {
        chanList = trimSpaces(params.substr(0, sp));
        keyList  = trimSpaces(params.substr(sp + 1));
    }
    
    // önce channels
    std::vector<std::string> chans;
    size_t start = 0;
    while (start < chanList.size())
    {
        size_t comma = chanList.find(',', start);
        std::string one = (comma == std::string::npos) ? chanList.substr(start) : chanList.substr(start, comma - start);
        one = trimSpaces(one);
        if (!one.empty())
            chans.push_back(one);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }

    // sonra keys
    std::vector<std::string> keys;
    if (!keyList.empty())
    {
        start = 0;
        while (start < keyList.size())
        {
            size_t comma = keyList.find(',', start);
            std::string one = (comma == std::string::npos) ? keyList.substr(start) : keyList.substr(start, comma - start);
            one = trimSpaces(one);
            keys.push_back(one);
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
    }

    for (size_t i = 0; i < chans.size(); ++i)
    {
        std::string chan = chans[i];
        std::string key  = (i < keys.size()) ? keys[i] : "";

        if (chan.empty() || chan[0] != '#')
        {
            sendNumeric(fd, ":server 403 " + c->getNickname() + " " + chan + " :No such channel\r\n");
            continue;
        }

        // Channel var mı? yoksa oluştur
        if (_channels.find(chan) == _channels.end())
        {
            _channels[chan] = new Channel(chan, key, false, 100);
        }

        Channel* ch = _channels[chan];

        // Zaten içeride mi?
        if (ch->hasUser(fd))
            continue;

        // Join kuralları
        if (!ch->canJoin(fd, key))
        {
            // En basit hata ayrımı:
            // doluysa 471, invite-only ise 473, key hatası 475
            if (ch->isFull())
                sendNumeric(fd, ":server 471 " + c->getNickname() + " " + chan + " :Cannot join channel (+l)\r\n");
            else if (ch->getIsPrivate())
                sendNumeric(fd, ":server 473 " + c->getNickname() + " " + chan + " :Cannot join channel (+i)\r\n");
            else
                sendNumeric(fd, ":server 475 " + c->getNickname() + " " + chan + " :Cannot join channel (+k)\r\n");
            continue;
        }

        // Ekle
        ch->addUser(fd, c);

        // JOIN mesajı: önce join eden kişiye de gitsin
        std::string joinMsg = makePrefix(c) + " JOIN :" + chan + "\r\n";
        sendNumeric(fd, joinMsg);
        ch->broadcast(joinMsg, fd); // diğerlerine (sender hariç)

        // TOPIC (331/332)
        if (ch->getTopicSet())
            sendNumeric(fd, ":server 332 " + c->getNickname() + " " + chan + " :" + ch->getTopic() + "\r\n");
        else
            sendNumeric(fd, ":server 331 " + c->getNickname() + " " + chan + " :No topic is set\r\n");

        std::string names;
        const std::map<int, Client*>& users = ch->getUsers();
        for (std::map<int, Client*>::const_iterator it = users.begin(); it != users.end(); ++it)
        {
            if (it->second && it->second->hasNickname())
            {
                if (!names.empty())
                    names += " ";
                names += it->second->getNickname();
            }
        }

        sendNumeric(fd, ":server 353 " + c->getNickname() + " = " + chan + " :" + names + "\r\n");
        sendNumeric(fd, ":server 366 " + c->getNickname() + " " + chan + " :End of /NAMES list.\r\n");
    }
}

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

    // PRIVMSG <target> :<text>
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
        // “ :” yoksa da fallback
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

        ch->broadcast(msg, fd); // sender hariç herkese
        return;
    }

    // User’a mesaj: nick’ten fd bul
    int toFd = -1;
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (it->second && it->second->hasNickname() && it->second->getNickname() == target)
        {
            toFd = it->first;
            break;
        }
    }

    if (toFd == -1)
    {
        sendNumeric(fd, ":server 401 " + c->getNickname() + " " + target + " :No such nick\r\n");
        return;
    }

    sendNumeric(toFd, msg);
}

