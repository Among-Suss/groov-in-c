tests: youtube_fetch_test

MAINDIR=../discord-in-c
OBJDIR=../discord-in-c/obj
CC=gcc
CFLAGS=-g -Wall -Wextra -Wno-unused-parameter
MACROS=-DLOG_USE_COLOR -DLOGGING
LFLAGS=-lpthread -lssl -lcrypto -lsodium -lopus -logg -lm -lcurl

DEPS=$(OBJDIR)/litesocket.o $(OBJDIR)/cJSON.o $(OBJDIR)/discord.o $(OBJDIR)/log.o $(OBJDIR)/media.o $(OBJDIR)/sbuf.o $(OBJDIR)/strmap.o $(OBJDIR)/youtube_fetch.o
youtube_fetch_test: youtube_fetch.test.c
	@$(CC) -o youtube_fetch.test youtube_fetch.test.c $(DEPS) $(CFLAGS) $(LFLAGS)
	@./youtube_fetch.test

$(OBJDIR)/litesocket.o: litesocket/litesocket.c litesocket/litesocket.h litesocket/litesocket.structs.h
	$(CC) -o $(OBJDIR)/litesocket.o -c $(MACROS) litesocket/litesocket.c $(CFLAGS)

$(OBJDIR)/%.o: %.c %.h
	$(CC) -o $@ -c $(MACROS) $(*F).c $(CFLAGS)

$(OBJDIR):
	mkdir -p $(OBJDIR)