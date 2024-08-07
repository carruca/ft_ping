NAME    	= ft_ping
SRCSPATH	= src/
OBJSPATH	= obj/
SRCSFILES	= ft_ping.c
SRCS			= $(addprefix $(SRCSPATH), $(SRCSFILES))
OBJS			= $(patsubst $(SRCSPATH)%, $(OBJSPATH)%, $(SRCS:.c=.o))
CC      	= gcc
CFLAGS  	= -Wall -Werror -Wextra
LDFLAGS 	= -lm
FSANITIZE = -fsanitize=address -g3

all: $(NAME)

$(OBJSPATH)%.o: $(SRCSPATH)%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(NAME): $(OBJS)
	$(CC) $(OBJS) -o $(NAME) $(LDFLAGS)

SILENT += print
print:
	echo $(SRCS)
	echo $(OBJS)

config: $(NAME)
	sudo chown root:root $(NAME)
	sudo chmod u+s $(NAME)

debug: LDFLAGS += $(FSANITIZE)
debug: $(NAME)

clean:
	$(RM) $(OBJS)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.SILENT: $(SILENT)
