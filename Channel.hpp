#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <map>
#include <set>
#include "Client.hpp"
#include <vector>

class Channel {

    private:
        std::string name;
        std::string topic;//+t mode açıksa sadece operatorler değiştirebilir.onun kontrolünü ekle
        std::string password;//+k aktifse girişte istenir

        bool       isPrivate;//lazım mı emin değilim +i mode için. invite only mode için lazım
        bool       topicSet;//topic daha önce ayarlandı mı kontrolü için
        int        maxUsers;//+l mode için kullanıcı limiti
        bool       inviteOnlyMode; //+i
        bool       topicOperatorOnlyMode; //+t
        bool       keyMode; //+k
        bool       limitMode; //+l

        std::map<int, Client*> users;
        std::set<int> operators;  //MODE-KICK-INVITE-TOPIC için operatörler +o içinde kullanıcaz
        std::set<int> invitedUsers; //+i aktifken kullanılır kullanıcı giriş yaptıktan sonra bu listeden silinir

    public:
        Channel(const std::string& name, const std::string& password, bool isPrivate, int maxUsers);
        ~Channel();

        bool hasUser(int fd) const;
        bool isOperator(int fd) const;
        bool isInvited(int fd) const;

        void addUser(int fd,Client* client);
        void removeUser(int fd);

        //+o mode için
        void addOperator(int fd);
        void removeOperator(int fd);

        void inviteUser(int fd);
        void removeInvite(int fd);

        void setTopic(const std::string& newTopic);
        bool changeTopic(int operatorFd, const std::string& newTopic);

        //broadcast eklenmesi lazım-kanaldaki tüm kullanıcılara mesaj gönderme
        void broadcast(const std::string& message, int senderFd);

        //join kontrolü
        bool isFull() const;
        bool isEmpty() const;
        bool canJoin(int fd,const std::string& pass) const;

        //getterlar
        std::string getName() const;
        std::string getTopic() const;
        bool        getIsPrivate() const;
        bool        getTopicSet() const;
        int         getMaxUsers() const;
        size_t      getUserCount() const;
        const std::map<int, Client*>& getUsers() const;
        std::string getModeString() const;

       //operator işlemleri KICK-INVİTE-TOPİC-MODE
        bool canKick(int fd) const;
        bool canInvite(int fd) const;
        bool canSetTopic(int fd) const;
        bool canChangeMode(int fd) const;

        bool kickUser(int operatorFd, int targetFd);
        bool invite(int operatorFd, int targetFd);
        
        // bool changeMode(int operatorFd, bool makePrivate, const std::string& newPassword);
        //MODE
        // MODE #channel +i,+t,+k,+l <password> <limit>
        /*
           Mode için adımlar:
              1. Gelen değeri parse la
              2. setMode() ile modu uygula
        */
        bool setMode(int operatorFd, char mode, bool enable, const std::string& param);
        bool applyModeString(int operatorFd, const std::string& modes,
                     const std::vector<std::string>& params);



        //channel modes
        /*
            +i : Set/remove Invite-only channel
            +t : Set/remove the restrictions of the TOPIC command to channel operators
            +k : Set/remove the channel key (password)
            +o : Give/take channel operator privilege
            +l : Set/remove the user limit to channel
        */

        void setInviteOnlyMode(bool mode);
        void setTopicOperatorOnlyMode(bool mode);
        void setKeyMode(bool mode);
        void setLimitMode(bool mode);

        bool getInviteOnlyMode() const;
        bool getTopicOperatorOnlyMode() const;
        bool getKeyMode() const;
        bool getLimitMode() const;


};




#endif