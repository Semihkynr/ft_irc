#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include "Channel.hpp"

int main()
{
    int sock1[2];
    int sock2[2];
    int sock3[2];

    socketpair(AF_UNIX, SOCK_STREAM, 0, sock1);
    socketpair(AF_UNIX, SOCK_STREAM, 0, sock2);
    socketpair(AF_UNIX, SOCK_STREAM, 0, sock3);

    Channel ch("#test", "", false, 10);

    ch.addUser(sock1[0], NULL);
    ch.addUser(sock2[0], NULL);
    ch.addUser(sock3[0], NULL);

    ch.broadcast("HELLO\n", sock2[0]); //sender fd 5

    char buf[100] = {0};

    read(sock1[1], buf, sizeof(buf));
    std::cout << "sock1 got: " << buf;

    read(sock3[1], buf, sizeof(buf));
    std::cout << "sock3 got: " << buf;

    // sock2 okumamalı (sender)
}