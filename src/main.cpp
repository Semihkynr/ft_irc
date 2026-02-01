/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ihancer <ihancer@student.42istanbul.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:41:43 by skaynar           #+#    #+#             */
/*   Updated: 2026/01/31 13:28:51 by ihancer          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Server.hpp"


static bool isDigits(const char* s) {
    if (!s || !*s) return false;
    for (size_t i = 0; s[i]; ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    return true;
}

static int parsePort(const char* s) {
    if (!isDigits(s))
        throw std::runtime_error("Port must be a positive integer.");

    errno = 0;
    char* end = NULL;
    long v = std::strtol(s, &end, 10);

    if (errno != 0 || end == s || *end != '\0')
        throw std::runtime_error("Invalid port format.");
    if (v < 1 || v > 65535)
        throw std::runtime_error("Port must be in range 1..65535.");

    return static_cast<int>(v);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
        return 1;
    }

    try {
        int port = parsePort(argv[1]);
        std::string password = argv[2];

        if (password.empty())
            throw std::runtime_error("Password must not be empty.");

        Server irc(port, password);
        irc.init();
        irc.run();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
