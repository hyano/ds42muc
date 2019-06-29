#
# Makefile for ds42muc
#
# Copyright (c) 2019 Hirokuni Yano
#
# Released under the MIT license.
# see https://opensource.org/licenses/MIT
#
CC	= gcc
CFLAGS	= -Wall -Wextra

all: ds42muc

clean:
	rm -f ds42muc ds42muc.o

ds42muc.o: ds42muc.c

ds42muc: ds42muc.o
	$(CC) ds42muc.o -o ds42muc
