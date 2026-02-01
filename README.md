# IRC Server (ft_irc)

This project is a simplified implementation of an IRC (Internet Relay Chat) server written in **C++98**, developed according to the ft_irc subject requirements.

The server supports multiple simultaneous clients using non-blocking sockets and the `poll()` system call. It implements core IRC commands, channel management, user authentication, and basic IRC protocol rules.

---

## Features

### Networking
- TCP server using BSD sockets
- Non-blocking I/O with `poll()`
- Multiple clients handled concurrently
- Graceful client disconnection handling

### Authentication & Registration
- `PASS` command required before any other command
- `NICK` and `USER` commands required to complete registration
- Clients cannot JOIN channels or send messages before registration
- Nicknames are unique and case-insensitive

### User Commands
- `PASS`, `NICK`, `USER`
- `QUIT`, `PING / PONG`
- `PRIVMSG`, `NOTICE`
- `WHO`, `WHOIS`

### Channel Commands
- `JOIN`, `PART`
- `NAMES`, `LIST`
- `TOPIC`
- `INVITE`, `KICK`
- `MODE`

### Channel Modes
- `+i` Invite-only
- `+t` Topic change restricted to operators
- `+k` Channel password
- `+l` User limit
- `+o` Operator privilege

### Channel Management
- Channels are created on first JOIN
- First user becomes channel operator
- Empty channels are automatically deleted
- Proper broadcast of JOIN / PART / QUIT messages

---

## Project Structure

.
├── includes/
│ ├── Server.hpp
│ ├── Client.hpp
│ └── Channel.hpp
│
├── src/
│ ├── main.cpp
│ ├── Server.cpp
│ ├── ServerHelpers.cpp
│ ├── ServerCommands.cpp
│ ├── Client.cpp
│ └── Channel.cpp
│
├── Makefile
└── README.md

## Build & Run

### Compile
make
./ircserv <port> <password>
Example:
./ircserv 6667 secretpass
Connecting with an IRC Client
You can connect using KVirc, Irssi, or netcat.

Example (KVirc)
Server: <server_ip>

Port: <port>

Password: <password>

The client must send:

PASS <password>

NICK <nickname>

USER <username> 0 * :realname

Protocol Notes
Commands are parsed line-by-line using \r\n
Trailing parameters (starting with :) are handled correctly
Nicknames and commands are case-insensitive
Invalid or out-of-order commands return proper numeric errors
Server never crashes on malformed input

Memory & Safety
Dynamic memory (Client*, Channel*) is properly managed
All clients are removed from channels on disconnect
Channels are deleted when empty
No file descriptors or heap memory leaks

Limitations
Single-server implementation (no server-to-server links)
No SSL/TLS
No file transfer (DCC)
Minimal hostname handling

Compliance
Written in C++98
Uses only allowed system calls
No external libraries
Fully compliant with the ft_irc subject

Authors
Developed as part of the 42 Network – ft_irc project.