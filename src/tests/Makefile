tests: youtube_fetch_test

MAINDIR=../discord-in-c
OBJDIR=$(MAINDIR)/obj
CC=gcc
CFLAGS=-g
MACROS=-DLOG_USE_COLOR -DLOGGING
LFLAGS=-lpthread -lssl -lcrypto -lsodium -lopus -logg -lm -lcurl

DEPS=$(OBJDIR)/litesocket.o $(OBJDIR)/cJSON.o $(OBJDIR)/discord.o $(OBJDIR)/log.o $(OBJDIR)/media.o $(OBJDIR)/sbuf.o $(OBJDIR)/strmap.o $(OBJDIR)/youtube_fetch.o
youtube_fetch_test: youtube_fetch.test.c $(OBJDIR) $(DEPS)
	@$(CC) -o youtube_fetch.test $(MACROS) youtube_fetch.test.c $(DEPS) $(CFLAGS) $(LFLAGS)
	@./youtube_fetch.test

$(OBJDIR)/litesocket.o: $(MAINDIR)/litesocket/litesocket.c $(MAINDIR)/litesocket/litesocket.h $(MAINDIR)/litesocket/litesocket.structs.h
	@$(CC) -o $(OBJDIR)/litesocket.o -c $(MACROS) $(MAINDIR)/litesocket/litesocket.c $(CFLAGS)

$(OBJDIR)/%.o: $(MAINDIR)/%.c $(MAINDIR)/%.h
	@$(CC) -o $@ -c $(MACROS) $(MAINDIR)/$(*F).c $(CFLAGS)

$(OBJDIR):
	@mkdir -p $(OBJDIR)