#CC = c99
#CFLAGS = -O2 -Wall -pedantic
CC = gcc
CFLAGS = -std=c99 -O2 -Wall -pedantic

all : julian julian.1

julian : julian.c
	$(CC) $(CFLAGS) -o julian julian.c

julian.1 : julian.pod
	pod2man -c '' -r 'Version 1.0' julian.pod julian.1
