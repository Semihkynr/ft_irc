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

void Server::handleNames(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c)
        return;

    std::string params = trimSpaces(rawParams);
    std::vector<std::string> targets;

    if (params.empty())
    {
        if (_channels.empty())
        {
            sendNumeric(fd, ":server 366 " + c->getNickname() + " * :End of /NAMES list.\r\n");
            return;
        }
        for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
            targets.push_back(it->first);
    }
    else
    {
        size_t start = 0;
        while (start < params.size())
        {
            size_t comma = params.find(',', start);
            std::string one = (comma == std::string::npos) ? params.substr(start) : params.substr(start, comma - start);
            one = trimSpaces(one);
            if (!one.empty())
                targets.push_back(one);
            if (comma == std::string::npos)
                break;
            start = comma + 1;
        }
    }

    for (size_t i = 0; i < targets.size(); ++i)
    {
        std::string chanName = targets[i];
        std::map<std::string, Channel*>::iterator it = _channels.find(chanName);
        if (it == _channels.end())
        {
            sendNumeric(fd, ":server 366 " + c->getNickname() + " " + chanName + " :End of /NAMES list.\r\n");
            continue;
        }

        Channel* ch = it->second;
        std::string names;
        const std::map<int, Client*>& users = ch->getUsers();
        for (std::map<int, Client*>::const_iterator uit = users.begin(); uit != users.end(); ++uit)
        {
            Client* u = uit->second;
            if (u && u->hasNickname())
            {
                if (!names.empty())
                    names += " ";
                if (ch->isOperator(uit->first))
                    names += "@";
                names += u->getNickname();
            }
        }

        sendNumeric(fd, ":server 353 " + c->getNickname() + " = " + chanName + " :" + names + "\r\n");
        sendNumeric(fd, ":server 366 " + c->getNickname() + " " + chanName + " :End of /NAMES list.\r\n");
    }
}

void Server::handlePart(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c)
        return;

    std::string params = trimSpaces(rawParams);
    if (params.empty())
    {
        sendNumeric(fd, ":server 461 " + c->getNickname() + " PART :Not enough parameters\r\n");
        return;
    }

    // PART <channel>{,<channel>} [:<reason>]
    std::string chanList;
    std::string reason;

    size_t sp = params.find(' ');
    if (sp == std::string::npos)
        chanList = params;
    else
    {
        chanList = trimSpaces(params.substr(0, sp));
        reason = trimSpaces(params.substr(sp + 1));
        if (!reason.empty() && reason[0] == ':')
            reason = reason.substr(1);
    }

    if (reason.empty())
        reason = "Leaving";

    // Parse channel list (comma-separated)
    std::vector<std::string> chans;
    size_t start = 0;
    while (start < chanList.size())
    {
        size_t comma = chanList.find(',', start);
        std::string one = (comma == std::string::npos) ? chanList.substr(start) : chanList.substr(start, comma - start);
        one = trimSpaces(one);
        if (!one.empty())
            chans.push_back(one);
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }

    for (size_t i = 0; i < chans.size(); ++i)
    {
        std::string chan = chans[i];

        // Check if channel exists
        std::map<std::string, Channel*>::iterator it = _channels.find(chan);
        if (it == _channels.end())
        {
            sendNumeric(fd, ":server 403 " + c->getNickname() + " " + chan + " :No such channel\r\n");
            continue;
        }

        Channel* ch = it->second;

        // Check if user is in channel
        if (!ch->hasUser(fd))
        {
            sendNumeric(fd, ":server 442 " + c->getNickname() + " " + chan + " :You're not on that channel\r\n");
            continue;
        }

        // Send PART message to all users in channel (including sender)
        std::string partMsg = makePrefix(c) + " PART " + chan + " :" + reason + "\r\n";
        sendNumeric(fd, partMsg);
        ch->broadcast(partMsg, fd);

        // Remove user from channel
        ch->removeUser(fd);

        // Delete channel if empty
        if (ch->isEmpty())
        {
            delete ch;
            _channels.erase(it);
        }
    }
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


//CHANNEL COMMAND

void Server::handleTopic(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c) return;

    std::string params = trimSpaces(rawParams);
    if (params.empty())
    {
        sendNumeric(fd, ":server 461 " + c->getNickname() + " TOPIC :Not enough parameters\r\n");
        return;
    }

    // TOPIC #chan [:new topic]
    size_t sp = params.find(' ');
    std::string chan = (sp == std::string::npos) ? params : trimSpaces(params.substr(0, sp));
    std::string rest = (sp == std::string::npos) ? ""     : trimSpaces(params.substr(sp + 1));

    std::map<std::string, Channel*>::iterator it = _channels.find(chan);
    if (it == _channels.end())
    {
        sendNumeric(fd, ":server 403 " + c->getNickname() + " " + chan + " :No such channel\r\n");
        return;
    }

    Channel* ch = it->second;
    if (!ch->hasUser(fd))
    {
        sendNumeric(fd, ":server 442 " + c->getNickname() + " " + chan + " :You're not on that channel\r\n");
        return;
    }

    // Sadece görüntüleme
    if (rest.empty())
    {
        if (ch->getTopicSet())
            sendNumeric(fd, ":server 332 " + c->getNickname() + " " + chan + " :" + ch->getTopic() + "\r\n");
        else
            sendNumeric(fd, ":server 331 " + c->getNickname() + " " + chan + " :No topic is set\r\n");
        return;
    }

    // Set etme (rest genelde ":topic")
    if (!rest.empty() && rest[0] == ':') rest = rest.substr(1);

    if (!ch->changeTopic(fd, rest))
    {
        sendNumeric(fd, ":server 482 " + c->getNickname() + " " + chan + " :You're not channel operator\r\n");
        return;
    }

    std::string msg = makePrefix(c) + " TOPIC " + chan + " :" + rest + "\r\n";
    sendNumeric(fd, msg);
    ch->broadcast(msg, fd);
}


void Server::handleInvite(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c) return;

    std::string params = trimSpaces(rawParams);
    size_t sp = params.find(' ');
    if (sp == std::string::npos)
    {
        sendNumeric(fd, ":server 461 " + c->getNickname() + " INVITE :Not enough parameters\r\n");
        return;
    }

    std::string nick = trimSpaces(params.substr(0, sp));
    std::string chan = trimSpaces(params.substr(sp + 1));

    int targetFd = findFdByNick(nick);
    if (targetFd == -1)
    {
        sendNumeric(fd, ":server 401 " + c->getNickname() + " " + nick + " :No such nick\r\n");
        return;
    }

    std::map<std::string, Channel*>::iterator it = _channels.find(chan);
    if (it == _channels.end())
    {
        sendNumeric(fd, ":server 403 " + c->getNickname() + " " + chan + " :No such channel\r\n");
        return;
    }

    Channel* ch = it->second;
    if (!ch->hasUser(fd))
    {
        sendNumeric(fd, ":server 442 " + c->getNickname() + " " + chan + " :You're not on that channel\r\n");
        return;
    }

    if (!ch->invite(fd, targetFd))
    {
        sendNumeric(fd, ":server 482 " + c->getNickname() + " " + chan + " :You're not channel operator\r\n");
        return;
    }

    std::string msgToTarget = makePrefix(c) + " INVITE " + nick + " :" + chan + "\r\n";
    sendNumeric(targetFd, msgToTarget);

    sendNumeric(fd, ":server 341 " + c->getNickname() + " " + nick + " " + chan + "\r\n");
}


void Server::handleKick(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c) return;

    std::string params = trimSpaces(rawParams);
    // KICK #chan nick [:reason]
    size_t sp1 = params.find(' ');
    if (sp1 == std::string::npos)
    {
        sendNumeric(fd, ":server 461 " + c->getNickname() + " KICK :Not enough parameters\r\n");
        return;
    }

    std::string chan = trimSpaces(params.substr(0, sp1));
    std::string rest = trimSpaces(params.substr(sp1 + 1));

    size_t sp2 = rest.find(' ');
    std::string nick = (sp2 == std::string::npos) ? rest : trimSpaces(rest.substr(0, sp2));
    std::string reason = (sp2 == std::string::npos) ? "" : trimSpaces(rest.substr(sp2 + 1));
    if (!reason.empty() && reason[0] == ':') reason = reason.substr(1);
    if (reason.empty()) reason = "Kicked";

    int targetFd = findFdByNick(nick);
    if (targetFd == -1)
    {
        sendNumeric(fd, ":server 401 " + c->getNickname() + " " + nick + " :No such nick\r\n");
        return;
    }

    std::map<std::string, Channel*>::iterator it = _channels.find(chan);
    if (it == _channels.end())
    {
        sendNumeric(fd, ":server 403 " + c->getNickname() + " " + chan + " :No such channel\r\n");
        return;
    }

    Channel* ch = it->second;
    if (!ch->hasUser(fd))
    {
        sendNumeric(fd, ":server 442 " + c->getNickname() + " " + chan + " :You're not on that channel\r\n");
        return;
    }

    if (!ch->kickUser(fd, targetFd))
    {
        sendNumeric(fd, ":server 482 " + c->getNickname() + " " + chan + " :You're not channel operator\r\n");
        return;
    }

    std::string msg = makePrefix(c) + " KICK " + chan + " " + nick + " :" + reason + "\r\n";
    sendNumeric(fd, msg);
    ch->broadcast(msg, fd);
    sendNumeric(targetFd, msg);

    // kanal boşsa sil (policy)
    if (ch->isEmpty())
    {
        delete ch;
        _channels.erase(it);
    }
}

void Server::handleMode(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c) return;

    std::string params = trimSpaces(rawParams);
    if (params.empty())
    {
        sendNumeric(fd, ":server 461 " + c->getNickname() + " MODE :Not enough parameters\r\n");
        return;
    }

    // MODE #chan [modes] [modeparams...]
    size_t sp = params.find(' ');
    std::string chan = (sp == std::string::npos) ? params : trimSpaces(params.substr(0, sp));
    std::string rest = (sp == std::string::npos) ? ""     : trimSpaces(params.substr(sp + 1));

    std::map<std::string, Channel*>::iterator it = _channels.find(chan);
    if (it == _channels.end())
    {
        sendNumeric(fd, ":server 403 " + c->getNickname() + " " + chan + " :No such channel\r\n");
        return;
    }

    Channel* ch = it->second;
    if (!ch->hasUser(fd))
    {
        sendNumeric(fd, ":server 442 " + c->getNickname() + " " + chan + " :You're not on that channel\r\n");
        return;
    }

    // sadece MODE #chan -> mevcut mode string dön
    if (rest.empty())
    {
        std::string modes = ch->getModeString();
        sendNumeric(fd, ":server 324 " + c->getNickname() + " " + chan + " " + modes + "\r\n");
        return;
    }

    // rest: "<modes> <params...>"
    size_t sp2 = rest.find(' ');
    std::string modeStr = (sp2 == std::string::npos) ? rest : trimSpaces(rest.substr(0, sp2));
    std::string paramStr = (sp2 == std::string::npos) ? "" : trimSpaces(rest.substr(sp2 + 1));

    // paramStr’yi tokenlara böl
    std::vector<std::string> rawModeParams;
    while (!paramStr.empty())
    {
        size_t psp = paramStr.find(' ');
        if (psp == std::string::npos) { rawModeParams.push_back(paramStr); break; }
        rawModeParams.push_back(paramStr.substr(0, psp));
        paramStr = trimSpaces(paramStr.substr(psp + 1));
    }

    // Channel.applyModeString paramları:
    // k,l,o için birer parametre gerekir.
    // o parametresi IRC’de nick; burada nick->fd çevireceğiz.
    std::vector<std::string> paramsForChannel;
    bool sign = true;
    size_t rawIdx = 0;

    for (size_t i = 0; i < modeStr.size(); ++i)
    {
        char m = modeStr[i];
        if (m == '+' || m == '-') continue;

        if (m == 'k' || m == 'l' || m == 'o')
        {
            if (rawIdx >= rawModeParams.size())
            {
                sendNumeric(fd, ":server 461 " + c->getNickname() + " MODE :Not enough parameters\r\n");
                return;
            }

            std::string p = rawModeParams[rawIdx++];

            if (m == 'o')
            {
                int targetFd = findFdByNick(p);
                if (targetFd == -1)
                {
                    sendNumeric(fd, ":server 401 " + c->getNickname() + " " + p + " :No such nick\r\n");
                    return;
                }
                p = intToString(targetFd); // Channel'a fd olarak ver
            }

            paramsForChannel.push_back(p);
        }
    }

    if (!ch->applyModeString(fd, modeStr, paramsForChannel))
    {
        sendNumeric(fd, ":server 482 " + c->getNickname() + " " + chan + " :You're not channel operator\r\n");
        return;
    }

    // MODE değişikliğini kanala duyur
    std::string msg = makePrefix(c) + " MODE " + chan + " " + modeStr;
    for (size_t i = 0; i < rawModeParams.size(); ++i)
        msg += " " + rawModeParams[i];
    msg += "\r\n";

    sendNumeric(fd, msg);
    ch->broadcast(msg, fd);
}
