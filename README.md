*This project has been created as part of the 42 curriculum by <skaynar>, <teraslan>, <ihancer>.*

# ft_irc — Internet Relay Chat Server

## Description

**ft_irc** is a simplified implementation of an IRC (Internet Relay Chat) server written in **C++98**, developed as part of the 42 Network curriculum.

The goal of this project is to understand low-level network programming, event-driven server design, and the fundamentals of the IRC protocol by implementing a functional IRC server from scratch.

The server supports multiple simultaneous clients using non-blocking sockets and the `poll()` system call, and follows the core rules defined in RFC 1459 / RFC 2812 where required by the subject.

---

## Features

### Networking
- TCP server using BSD sockets
- Non-blocking file descriptors
- I/O multiplexing with `poll()`
- Handles multiple clients concurrently
- Clean handling of client disconnections and server shutdown

### Authentication & Registration
- `PASS` command is mandatory before registration
- `NICK` and `USER` commands are required to complete registration
- Clients **cannot use JOIN, PRIVMSG, or channel commands before registration**
- Nicknames are unique and case-insensitive
- Proper numeric error replies for invalid or out-of-order commands

### User Commands
- `PASS`, `NICK`, `USER`
- `QUIT`
- `PING / PONG`
- `PRIVMSG`
- `NOTICE`
- `WHO`, `WHOIS`

### Channel Commands
- `JOIN`, `PART`
- `NAMES`, `LIST`
- `TOPIC`
- `INVITE`, `KICK`
- `MODE`

### Channel Modes
- `+i` Invite-only channel
- `+t` Topic restricted to operators
- `+k` Channel password (key)
- `+l` User limit
- `+o` Operator privilege

### Channel Management
- Channels are created automatically on first `JOIN`
- The first user joining a channel becomes an operator
- Empty channels are automatically destroyed
- JOIN / PART / QUIT messages are properly broadcasted
- Operator reassignment when needed

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

---

## Instructions

### Compilation
```bash
make
```

### Execution
```bash
./ircserv <port> <password>
```

Example:
```bash
./ircserv 6667 secretpass
```

### Connecting to the Server

You can connect using an IRC client such as KVirc, Irssi, or via `nc` (netcat).

**Example with KVirc:**
- Server: `<server_ip>`
- Port: `<port>`
- Password: `<password>`

**Command Sequence:**

The client must send the following commands in order for registration:
```
PASS <password>
NICK <nickname>
USER <username> 0 * :realname
```

---

## Protocol Notes

- Commands are parsed line-by-line using `\r\n`
- Trailing parameters (starting with `:`) are handled correctly
- IRC commands and nicknames are case-insensitive
- Invalid commands return appropriate numeric error replies
- The server never crashes on malformed input

---

## Memory Management & Safety

- Dynamic memory (`Client*`, `Channel*`) is carefully managed
- All clients are removed from channels on `QUIT` or disconnect
- Channels are deleted when empty
- No file descriptor leaks
- No heap memory leaks

---

## Limitations

- Single-server implementation (no server-to-server links)
- No SSL/TLS support
- No file transfer (DCC)
- Minimal hostname handling

---

## Compliance

- Written in **C++98** (no modern C++ features)
- Uses only allowed system calls
- No external libraries
- Fully compliant with the ft_irc subject requirements

---

## Resources

### IRC Protocol References
- [RFC 1459 - Internet Relay Chat Protocol](https://tools.ietf.org/html/rfc1459) - Original IRC protocol specification
- [RFC 2812 - Internet Relay Chat: Client Protocol](https://tools.ietf.org/html/rfc2812) - Updated IRC protocol specification
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) - Comprehensive guide to socket programming in C

### Technical Documentation
- [Linux man pages: poll(2)](https://man7.org/linux/man-pages/man2/poll.2.html) - I/O multiplexing system call
- [Linux man pages: socket(2)](https://man7.org/linux/man-pages/man2/socket.2.html) - Socket creation and configuration
- [Linux man pages: send(2)](https://man7.org/linux/man-pages/man2/send.2.html) - Sending data on sockets

### C++ References
- [cppreference.com](https://en.cppreference.com/) - C++ Standard Library reference
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) - Best practices for C++

### IRC Client Tools
- [KVirc](http://www.kvirc.net/) - Cross-platform graphical IRC client
- [Irssi](https://irssi.org/) - Terminal-based IRC client
- [netcat (nc)](https://www.man7.org/linux/man-pages/man1/nc.1.html) - Network utility for socket connection

### AI Usage

GitHub Copilot was utilized in the following areas of this project:

- **Documentation and comments**: Generating clarifying comments for complex networking logic and protocol implementation
- **Error handling patterns**: Suggesting proper IRC protocol error responses and numeric reply formatting

The core architecture, IRC protocol implementation decisions, algorithm designs, and critical networking logic were developed manually to ensure deep understanding of IRC protocol mechanics and network programming fundamentals.
