NAME = ircserv
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -Iinclude

SRCS =  src/main.cpp \
        src/Server.cpp \
        src/ServerHelpers.cpp \
        src/Client.cpp \
        src/Channel.cpp \
        src/commands/AuthCommands.cpp \
        src/commands/ChannelCommands.cpp \
        src/commands/MessagingCommands.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all
