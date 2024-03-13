CC = gcc
CFLAGS = -fsanitize=address -Wall -Wextra -Werror -pedantic -g

mycat:
	-$(CC) $(CFLAGS) -o mycat.o main.c
