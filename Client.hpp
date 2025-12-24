/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: teraslan <teraslan@student.42istanbul.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:37:57 by skaynar           #+#    #+#             */
/*   Updated: 2025/12/24 19:59:49 by teraslan         ###   ########.fr       */
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
    bool        _authenticated; // Şifre doğrulandı mı?

public:
    Client(int fd);
    ~Client();

    int         getFd() const;
    void        addBuffer(std::string str);
    std::string getBuffer() const;
    void        clearBuffer();
    
    bool        isAuthenticated() const;
    void        setAuthenticated(bool auth);
    // İleride buraya: bool isRegistered, bool isOp vb. eklenecek.
};

#endif