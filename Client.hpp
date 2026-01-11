/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilknurhancer <ilknurhancer@student.42.f    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:37:57 by skaynar           #+#    #+#             */
/*   Updated: 2026/01/11 19:33:02 by ilknurhance      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client {
private:
    int         _fd;
    std::string _nickname;
    std::string _username;
    std::string _buffer;         // partial recv biriktirme
    bool        _authenticated;  // PASS doğrulandı mı?
    bool        _registered;     // PASS+NICK+USER tamam mı?

public:
    Client(int fd);
    ~Client();

    int         getFd() const;

    // Input buffering
    void        addBuffer(const std::string& str);
    std::string getBuffer() const;
    void        clearBuffer();

    // Auth/Register state
    bool        isAuthenticated() const;
    void        setAuthenticated(bool auth);

    bool        isRegistered() const;
    void        setRegistered(bool reg);

    // Nickname
    void                setNickname(const std::string& nick);
    const std::string&  getNickname() const;
    bool                hasNickname() const;

    // Username
    void                setUsername(const std::string& user);
    const std::string&  getUsername() const;
    bool                hasUsername() const;
};

#endif
