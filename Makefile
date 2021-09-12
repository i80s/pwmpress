#
# Copyright (c) 2021 i80s
# Author: i80s
# https://github.com/i80s/
#

CC ?= gcc
CFLAGS += -Wall
HEADERS =

ifneq ($(N),)
  CFLAGS += -DDEFAULT_GPIO=$(N)
endif

pwmpress: pwmpress.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f pwmpress *.o

