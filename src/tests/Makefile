tests: youtube_fetch_test

MAINDIR=../discord-in-c
OBJDIR=../discord-in-c/obj
CC=gcc
CFLAGS=-g -Wall -Wextra -Wno-unused-parameter
LFLAGS=-lpthread -lssl -lcrypto -lsodium -lopus -logg -lm -lcurl

DEPS=$(OBJDIR)/litesocket.o $(OBJDIR)/cJSON.o $(OBJDIR)/discord.o $(OBJDIR)/log.o $(OBJDIR)/media.o $(OBJDIR)/sbuf.o $(OBJDIR)/strmap.o $(OBJDIR)/youtube_fetch.o
youtube_fetch_test: $(MAINDIR)/test2 youtube_fetch.test.c
	@$(CC) -o youtube_fetch.test $(DEPS) $(CFLAGS) $(LFLAGS)
	@./youtube_fetch.test

