NAME := $(shell basename $(PWD))
all: $(NAME)-64.so $(NAME)-32.so
CFLAGS += -U_FORTIFY_SOURCE

include ../Makefile
