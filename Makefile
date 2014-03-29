#CC = c99
#CFLAGS = -O2 -Wall
CC = gcc
CFLAGS = -std=c99 -O2 -Wall

all : julian

julian : julian.c
	$(CC) $(CFLAGS) -o julian julian.c

#julian.1 : julian.pod
#	pod2man -c '' -r '' julian.pod julian.1
