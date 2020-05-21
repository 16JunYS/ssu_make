CC = gcc
USER = OSLAB
WELCOME = "WELCOME MESSAGE"
OS ?= LINUX

test_make1 : test_code1.o
	$(CC) -o test_make1  test_code1.o
test_code1.o : test_code1.c
	$(CC) -c test_code1.c

