/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ihancer <ihancer@student.42istanbul.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 18:37:57 by skaynar           #+#    #+#             */
/*   Updated: 2026/01/31 13:28:43 by ihancer          ###   ########.fr       */
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
    std::string _buffer;        
    bool        _authenticated; 
    bool        _registered;  

public:
    Client(int fd);
    ~Client();

    int         getFd() const;
    void        addBuffer(const std::string& str);
    std::string getBuffer() const;
    void        clearBuffer();
    bool        isAuthenticated() const;
    void        setAuthenticated(bool auth);
    bool        isRegistered() const;
    void        setRegistered(bool reg);

    void                setNickname(const std::string& nick);
    const std::string&  getNickname() const;
    bool                hasNickname() const;

    void                setUsername(const std::string& user);
    const std::string&  getUsername() const;
    bool                hasUsername() const;
};

#endif
