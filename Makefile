#
# Copyright (c) 2021 i80s
# Author: i80s
# https://github.com/i80s/
#

CC ?= gcc
CFLAGS += -Wall
HEADERS =

pwmpress: pwmpress.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -DDEFAULT_GPIO=7

clean:
	rm -f pwmpress *.o

