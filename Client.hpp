/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: skaynar <skaynar@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:37:57 by skaynar           #+#    #+#             */
/*   Updated: 2025/12/24 18:37:59 by skaynar          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <iostream>

class Client {
private:
    int         _fd;
    std::string _nickname;
    std::string _username;
    std::string _buffer; // Yarım kalan mesajları birleştirmek için

public:
    Client(int fd);
    ~Client();

    int         getFd() const;
    void        addBuffer(std::string str);
    std::string getBuffer() const;
    void        clearBuffer();
    // İleride buraya: bool isRegistered, bool isOp vb. eklenecek.
};

#endif