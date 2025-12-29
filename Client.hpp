/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilknurhancer <ilknurhancer@student.42.f    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:37:57 by skaynar           #+#    #+#             */
/*   Updated: 2025/12/27 18:47:49 by ilknurhance      ###   ########.fr       */
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
    bool        _registered;

public:
    Client(int fd);
    ~Client();

    int         getFd() const;
    void        addBuffer(std::string str);
    std::string getBuffer() const;
    void        clearBuffer();
    
    bool        isAuthenticated() const;
    void        setAuthenticated(bool auth);
    bool isRegistered() const;
    void setRegistered(bool reg);

    // Nickname
    void                setNickname(const std::string& nick);
    const std::string&  getNickname() const;
    bool                hasNickname() const;

    // Username
    void                setUsername(const std::string& user);
    const std::string&  getUsername() const;
    bool                hasUsername() const;


    // İleride buraya: bool isRegistered, bool isOp vb. eklenecek.
};

#endif