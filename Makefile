SRC =		mtlib.c

NAME =		mtlib.a

CFLAGS =	-O3 -pthread -Wall -Werror -Wextra -I .

CC =		clang $(CFLAGS)

OBJ =		$(SRC:.c=.o)

TEST = 		test_mtlib

all:		$(NAME)

$(NAME):	$(OBJ)
			ar rc $(NAME) $(OBJ)
			ranlib $(NAME)

test:		
			rm -f $(OBJ) $(NAME)
			$(CC) -o $(TEST) mtlib.c mtlib_test.c

clean:		
			rm -f $(OBJ)

fclean:		clean
			rm -f $(NAME) $(TEST)

re:			fclean all

.PHONY:		all clean fclean re
