NAME    = ping
#CFLAGS  = -Wall -Werror -Wextra
SRC     = ping.c
OBJ     = $(SRC:.c=.o)
CC      = gcc

all: $(NAME) config

config:
	sudo chown root:root $(NAME)
	sudo chmod u+s $(NAME)

$(NAME): $(OBJ)
	$(CC) $(OBJ) -o $(NAME)

clean:
	$(RM) $(OBJ)

fclean: clean
	$(RM) $(NAME)

re: fclean all
