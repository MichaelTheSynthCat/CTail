# Makefile for ctail.c
CC = gcc
CFLAGS = -std=c11 -g -Wall -Wextra -pedantic -O2 -DNDEBUG

all: ctail 

ctail: ctail.c
	${CC} ${CFLAGS} $? -o $@

clean:
	rm -f ctail


