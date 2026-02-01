#include "Server.hpp"

void Server::handleJoin(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c)
        return;

    if (!c->isAuthenticated())
    {
        sendNumeric(fd, "ERROR :You must authenticate with PASS first\r\n");
        return;
    }
    if (!c->isRegistered() || !c->hasNickname() || !c->hasUsername())
    {
        sendNumeric(fd, ":server 451 * :You have not registered\r\n");
        return;
    }
    
    std::string params = trimSpaces(rawParams);
    if (params.empty())
    {
        sendNumeric(fd, ":server 461 * JOIN :Not enough parameters\r\n");
        return;
    }

    if (params == "0")
    {
        std::vector<std::string> userChans;
        for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        {
            if (it->second && it->second->hasUser(fd))
                userChans.push_back(it->first);
        }

        for (size_t i = 0; i < userChans.size(); ++i)
        {
            std::string chan = userChans[i];
            std::map<std::string, Channel*>::iterator it = _channels.find(chan);
            if (it != _channels.end())
            {
                Channel* ch = it->second;
                std::string partMsg = makePrefix(c) + " PART " + chan + " :leaving\r\n";
                sendNumeric(fd, partMsg);
                ch->broadcast(partMsg, fd);
                ch->removeUser(fd);

                if (ch->isEmpty())
                {
                    delete ch;
                    _channels.erase(it);
                }
            }
        }
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

        if (chan.length() > 50)
        {
            sendNumeric(fd, ":server 403 " + c->getNickname() + " " + chan + " :Channel name too long\r\n");
            continue;
        }

        bool invalid = false;
        for (size_t j = 1; j < chan.length(); ++j)
        {
            unsigned char ch = static_cast<unsigned char>(chan[j]);
            if (chan[j] == ' ' || chan[j] == ',' || chan[j] == 7 || ch < 32)
            {
                invalid = true;
                break;
            }
        }
    
        if (invalid)
        {
            sendNumeric(fd, ":server 403 " + c->getNickname() + " " + chan + " :Invalid channel name\r\n");
            continue;
        }
    
        if (_channels.find(chan) == _channels.end())
        {
            _channels[chan] = new Channel(chan, key, 100);
        }

        Channel* ch = _channels[chan];

        if (ch->hasUser(fd))
            continue;

        if (!ch->canJoin(fd, key))
        {
            if (ch->isFull())
                sendNumeric(fd, ":server 471 " + c->getNickname() + " " + chan + " :Cannot join channel (+l)\r\n");
            else if (ch->getIsPrivate())
                sendNumeric(fd, ":server 473 " + c->getNickname() + " " + chan + " :Cannot join channel (+i)\r\n");
            else
                sendNumeric(fd, ":server 475 " + c->getNickname() + " " + chan + " :Cannot join channel (+k)\r\n");
            continue;
        }
        ch->addUser(fd, c);

        std::string joinMsg = makePrefix(c) + " JOIN :" + chan + "\r\n";
        sendNumeric(fd, joinMsg);
        ch->broadcast(joinMsg, fd);

        if (ch->getTopicSet())
            sendNumeric(fd, ":server 332 " + c->getNickname() + " " + chan + " :" + ch->getTopic() + "\r\n");
        else
            sendNumeric(fd, ":server 331 " + c->getNickname() + " " + chan + " :No topic is set\r\n");

        std::string names;
        const std::map<int, Client*>& users = ch->getUsers();
        for (std::map<int, Client*>::const_iterator it = users.begin(); it != users.end(); ++it) {
            if (it->second && it->second->hasNickname()) {
                if (!names.empty())
                    names += " ";
                if (ch->isOperator(it->first))
                    names += "@";
                names += it->second->getNickname();
            }
        }

        sendNumeric(fd, ":server 353 " + c->getNickname() + " = " + chan + " :" + names + "\r\n");
        sendNumeric(fd, ":server 366 " + c->getNickname() + " " + chan + " :End of /NAMES list.\r\n");
    }
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

void Server::handleList(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c)
        return;

    std::string params = trimSpaces(rawParams);

    sendNumeric(fd, ":server 321 " + c->getNickname() + " Channel :Users Name\r\n");

    std::vector<std::string> targets;
    if (params.empty())
    {
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
            continue;

        Channel* ch = it->second;
        std::string topic = ch->getTopic();
        sendNumeric(fd, ":server 322 " + c->getNickname() + " " + chanName + " " + intToString(ch->getUserCount()) + " :" + topic + "\r\n");
    }
    sendNumeric(fd, ":server 323 " + c->getNickname() + " :End of /LIST\r\n");
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

        std::map<std::string, Channel*>::iterator it = _channels.find(chan);
        if (it == _channels.end())
        {
            sendNumeric(fd, ":server 403 " + c->getNickname() + " " + chan + " :No such channel\r\n");
            continue;
        }

        Channel* ch = it->second;

        if (!ch->hasUser(fd))
        {
            sendNumeric(fd, ":server 442 " + c->getNickname() + " " + chan + " :You're not on that channel\r\n");
            continue;
        }

        std::string partMsg = makePrefix(c) + " PART " + chan + " :" + reason + "\r\n";
        sendNumeric(fd, partMsg);
        ch->broadcast(partMsg, fd);

        bool wasOperator = ch->isOperator(fd);

        ch->removeUser(fd);

        if (wasOperator && !ch->isEmpty())
        {
            ch->promoteNewOperator();
        }

        if (ch->isEmpty())
        {
            delete ch;
            _channels.erase(it);
        }
    }
}

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

    if (rest.empty())
    {
        if (ch->getTopicSet())
            sendNumeric(fd, ":server 332 " + c->getNickname() + " " + chan + " :" + ch->getTopic() + "\r\n");
        else
            sendNumeric(fd, ":server 331 " + c->getNickname() + " " + chan + " :No topic is set\r\n");
        return;
    }

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

    if (!ch->hasUser(targetFd))
    {
        sendNumeric(fd, ":server 441 " + c->getNickname() + " " + nick + " " + chan + " :They aren't on that channel\r\n");
        return;
    }

    if (!ch->canKick(fd))
    {
        sendNumeric(fd, ":server 482 " + c->getNickname() + " " + chan + " :You're not channel operator\r\n");
        return;
    }

    ch->kickUser(fd, targetFd);

    std::string msg = makePrefix(c) + " KICK " + chan + " " + nick + " :" + reason + "\r\n";

    sendNumeric(fd, msg);
    sendNumeric(targetFd, msg);
    ch->broadcast(msg, fd);

    if (ch->isEmpty()) {
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

    if (rest.empty())
    {
        std::string modes = ch->getModeString();
        sendNumeric(fd, ":server 324 " + c->getNickname() + " " + chan + " " + modes + "\r\n");
        return;
    }

    size_t sp2 = rest.find(' ');
    std::string modeStr = (sp2 == std::string::npos) ? rest : trimSpaces(rest.substr(0, sp2));
    std::string paramStr = (sp2 == std::string::npos) ? "" : trimSpaces(rest.substr(sp2 + 1));

    std::vector<std::string> rawModeParams;
    while (!paramStr.empty())
    {
        size_t psp = paramStr.find(' ');
        if (psp == std::string::npos) { rawModeParams.push_back(paramStr); break; }
        rawModeParams.push_back(paramStr.substr(0, psp));
        paramStr = trimSpaces(paramStr.substr(psp + 1));
    }

    std::vector<std::string> paramsForChannel;
    size_t rawIdx = 0;

    char sign = '+';

    for (size_t i = 0; i < modeStr.size(); ++i)
    {
        char m = modeStr[i];
        if (m == '+' || m == '-') { sign = m; continue; }

        bool needsParam = false;
        if (m == 'o') needsParam = true;
        else if (m == 'k') needsParam = (sign == '+');
        else if (m == 'l') needsParam = (sign == '+');

        if (needsParam)
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
                p = intToString(targetFd);
            }
            else if (m == 'l')
            {
                bool isNumeric = !p.empty();
                for (size_t j = 0; j < p.length(); ++j)
                    if (p[j] < '0' || p[j] > '9') { isNumeric = false; break; }

                if (!isNumeric || std::atoi(p.c_str()) <= 0)
                {
                    sendNumeric(fd, ":server 461 " + c->getNickname() + " MODE :Invalid limit parameter\r\n");
                    return;
                }
            }
            else if (m == 'k')
            {
                if (p.empty())
                {
                    sendNumeric(fd, ":server 461 " + c->getNickname() + " MODE :Empty key provided\r\n");
                    return;
                }
            }

            paramsForChannel.push_back(p);
        }
    }


    if (!ch->applyModeString(fd, modeStr, paramsForChannel))
    {
        sendNumeric(fd, ":server 482 " + c->getNickname() + " " + chan + " :You're not channel operator\r\n");
        return;
    }

    std::string msg = makePrefix(c) + " MODE " + chan + " " + modeStr;
    for (size_t i = 0; i < rawModeParams.size(); ++i)
        msg += " " + rawModeParams[i];
    msg += "\r\n";

    sendNumeric(fd, msg);
    ch->broadcast(msg, fd);
}