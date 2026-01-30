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

    // Check nickname format (RFC 2812)
    // Nick must be 1-9 chars, first char must be letter
    if (nick.length() < 1 || nick.length() > 9)
    {
        std::string err = ":server 432 * " + nick + " :Erroneous nickname\r\n";
        send(fd, err.c_str(), err.length(), 0);
        return;
    }
    
    // First character must be a letter (RFC 2812)
    if (!((nick[0] >= 'a' && nick[0] <= 'z') || (nick[0] >= 'A' && nick[0] <= 'Z')))
    {
        std::string err = ":server 432 * " + nick + " :Erroneous nickname\r\n";
        send(fd, err.c_str(), err.length(), 0);
        return;
    }
    
    // Check for invalid characters (can be letter, digit, or special: - [ ] \ ` ^ { | })
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
    std::string oldPrefix = makePrefix(c);  // Full prefix with user@host
    c->setNickname(nick);

    std::string reply = oldPrefix + " NICK :" + nick + "\r\n";
    sendNumeric(fd, reply);

    // Other clients in same channel should also get this
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

    // JOIN 0: tüm kanallardan ayrıl (RFC 2812)
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

        // Channel name length check (max 50)
        if (chan.length() > 50)
        {
            sendNumeric(fd, ":server 403 " + c->getNickname() + " " + chan + " :Channel name too long\r\n");
            continue;
        }

        // Check for spaces, commas, ctrl characters
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
    
        // Channel var mı? yoksa oluştur
        if (_channels.find(chan) == _channels.end())
        {
            _channels[chan] = new Channel(chan, key, 100);
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
        for (std::map<int, Client*>::const_iterator it = users.begin(); it != users.end(); ++it) {
            if (it->second && it->second->hasNickname()) {
                if (!names.empty())
                    names += " ";
                if (ch->isOperator(it->first))  // ✅ Operator check ekle
                    names += "@";
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

    int toFd = findFdByNick(target);

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

void Server::handleList(int fd, const std::string& rawParams)
{
    Client* c = _clients[fd];
    if (!c)
        return;

    std::string params = trimSpaces(rawParams);

    // RPL_LISTSTART
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

    // RPL_LISTEND
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

        // Check if the leaving user is an operator
        bool wasOperator = ch->isOperator(fd);

        // Remove user from channel
        ch->removeUser(fd);

        // If an operator left and there are still users in the channel, promote a new operator
        if (wasOperator && !ch->isEmpty())
        {
            ch->promoteNewOperator();
        }

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

    // Check if target user is in the channel
    if (!ch->hasUser(targetFd))
    {
        sendNumeric(fd, ":server 441 " + c->getNickname() + " " + nick + " " + chan + " :They aren't on that channel\r\n");
        return;
    }

    // Check if kicker is an operator
    if (!ch->canKick(fd))
    {
        sendNumeric(fd, ":server 482 " + c->getNickname() + " " + chan + " :You're not channel operator\r\n");
        return;
    }

    ch->kickUser(fd, targetFd);

    std::string msg = makePrefix(c) + " KICK " + chan + " " + nick + " :" + reason + "\r\n";

    // Mesajı gönder
    sendNumeric(fd, msg);
    sendNumeric(targetFd, msg);
    ch->broadcast(msg, fd);  // ← -1 = herkese (sender dahil değil zaten broadcast'ta)

    // Kanal boşsa sil
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

    std::vector<std::string> paramsForChannel;
    size_t rawIdx = 0;

    char sign = '+';

    for (size_t i = 0; i < modeStr.size(); ++i)
    {
        char m = modeStr[i];
        if (m == '+' || m == '-') { sign = m; continue; }

        bool needsParam = false;
        if (m == 'o') needsParam = true;
        else if (m == 'k') needsParam = (sign == '+'); // +k ister, -k istemez
        else if (m == 'l') needsParam = (sign == '+'); // +l ister, -l istemez

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

    // MODE değişikliğini kanala duyur
    std::string msg = makePrefix(c) + " MODE " + chan + " " + modeStr;
    for (size_t i = 0; i < rawModeParams.size(); ++i)
        msg += " " + rawModeParams[i];
    msg += "\r\n";

    sendNumeric(fd, msg);
    ch->broadcast(msg, fd);
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

    // WHO can be for a channel or a nickname
    std::string target = params;
    size_t sp = target.find(' ');
    if (sp != std::string::npos)
        target = target.substr(0, sp);
    target = trimSpaces(target);

    // Check if target is a channel
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

            std::string flags = "H"; // H = here, G = gone (away)
            if (ch->isOperator(uit->first))
                flags += "@";

            // RPL_WHOREPLY (352): <channel> <user> <host> <server> <nick> <flags> :<hopcount> <realname>
            std::string reply = ":server 352 " + c->getNickname() + " " + target + " " +
                              u->getUsername() + " localhost server " + u->getNickname() + " " +
                              flags + " :0 " + u->getUsername() + "\r\n";
            sendNumeric(fd, reply);
        }

        // RPL_ENDOFWHO (315)
        sendNumeric(fd, ":server 315 " + c->getNickname() + " " + target + " :End of WHO list\r\n");
    }
    else
    {
        // WHO for a specific nickname
        int targetFd = findFdByNick(target);
        if (targetFd == -1)
        {
            sendNumeric(fd, ":server 315 " + c->getNickname() + " " + target + " :End of WHO list\r\n");
            return;
        }

        Client* targetClient = _clients[targetFd];
        if (targetClient && targetClient->hasNickname())
        {
            std::string flags = "H"; // Here

            // Find what channels the user is in
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

    // Parse nickname (could be comma-separated but we'll handle single for now)
    std::string targetNick = params;
    size_t sp = targetNick.find(' ');
    if (sp != std::string::npos)
        targetNick = targetNick.substr(0, sp);
    targetNick = trimSpaces(targetNick);

    int targetFd = findFdByNick(targetNick);
    if (targetFd == -1)
    {
        // ERR_NOSUCHNICK (401)
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

    // RPL_WHOISUSER (311): <nick> <user> <host> * :<real name>
    std::string whoisUser = ":server 311 " + c->getNickname() + " " + targetClient->getNickname() + " " +
                           targetClient->getUsername() + " localhost * :" + targetClient->getUsername() + "\r\n";
    sendNumeric(fd, whoisUser);

    // RPL_WHOISSERVER (312): <nick> <server> :<server info>
    std::string whoisServer = ":server 312 " + c->getNickname() + " " + targetClient->getNickname() + 
                             " server :IRC Server\r\n";
    sendNumeric(fd, whoisServer);

    // RPL_WHOISCHANNELS (319): <nick> :{[@|+]<channel> }
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

    // RPL_ENDOFWHOIS (318)
    sendNumeric(fd, ":server 318 " + c->getNickname() + " " + targetClient->getNickname() + " :End of WHOIS list\r\n");
}

void Server::handlePing(int fd, const std::string& rawParams) {
    Client* c = _clients[fd];
    if (!c) return;
    
    std::string params = trimSpaces(rawParams);
    std::string msg = ":server PONG :" + (params.empty() ? "server" : params) + "\r\n";
    sendNumeric(fd, msg);
}