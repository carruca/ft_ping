NAME    = sockets
#CFLAGS  = -Wall -Werror -Wextra
SRC     = sockets.c
OBJ     = $(SRC:.c=.o)
CC      = gcc

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(OBJ) -o $(NAME)

clean:
	$(RM) $(OBJ)

fclean: clean
	$(RM) $(NAME)

re: fclean all
