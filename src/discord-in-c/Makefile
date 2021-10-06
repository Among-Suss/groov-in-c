OBJDIR=obj
CC=gcc
CFLAGS=-Wall -Wextra -Wno-unused-parameter -O2
MACROS=-DLOG_USE_COLOR
LFLAGS=-lpthread -lssl -lcrypto -lsodium -lopus -logg -lm -lcurl



all: test2



DEPS_TEST2=$(OBJDIR)/litesocket.o $(OBJDIR)/cJSON.o $(OBJDIR)/discord.o $(OBJDIR)/log.o $(OBJDIR)/media.o $(OBJDIR)/sbuf.o $(OBJDIR)/strmap.o $(OBJDIR)/youtube_fetch.o
test2: test2.c $(OBJDIR) $(DEPS_TEST2)
	$(CC) -o test2 $(MACROS) test2.c $(DEPS_TEST2) $(CFLAGS) $(LFLAGS)


DEPS_TEST2=$(OBJDIR)/litesocket.o $(OBJDIR)/cJSON.o $(OBJDIR)/discord.o $(OBJDIR)/log.o $(OBJDIR)/media.o $(OBJDIR)/sbuf.o $(OBJDIR)/strmap.o $(OBJDIR)/youtube_fetch.o
sanitize: test2.c $(OBJDIR) $(DEPS_TEST2)
	$(CC) -o test2 $(MACROS) test2.c $(DEPS_TEST2) $(CFLAGS) $(LFLAGS) -fsanitize=address




$(OBJDIR)/litesocket.o: litesocket/litesocket.c litesocket/litesocket.h litesocket/litesocket.structs.h
	$(CC) -o $(OBJDIR)/litesocket.o -c $(MACROS) litesocket/litesocket.c $(CFLAGS)

$(OBJDIR)/cJSON.o: cJSON.c cJSON.h
	$(CC) -o $(OBJDIR)/cJSON.o -c $(MACROS) cJSON.c $(CFLAGS)

$(OBJDIR)/discord.o: discord.c discord.h discord.structs.h
	$(CC) -o $(OBJDIR)/discord.o -c $(MACROS) discord.c $(CFLAGS)

$(OBJDIR)/log.o: log.c log.h
	$(CC) -o $(OBJDIR)/log.o -c $(MACROS) log.c $(CFLAGS)

$(OBJDIR)/media.o: media.c media.h media.structs.h
	$(CC) -o $(OBJDIR)/media.o -c $(MACROS) media.c $(CFLAGS)

$(OBJDIR)/sbuf.o: sbuf.c sbuf.h sbuf.structs.h
	$(CC) -o $(OBJDIR)/sbuf.o -c $(MACROS) sbuf.c $(CFLAGS)

$(OBJDIR)/strmap.o: strmap.c strmap.h
	$(CC) -o $(OBJDIR)/strmap.o -c $(MACROS) strmap.c $(CFLAGS)

$(OBJDIR)/youtube_fetch.o: youtube_fetch.c youtube_fetch.h
	$(CC) -o $(OBJDIR)/youtube_fetch.o -c $(MACROS) youtube_fetch.c $(CFLAGS)

$(OBJDIR):
	mkdir -p $(OBJDIR)



clean:
	rm -rf test2
	rm -rf obj

format:
	clang-format --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i *.h
	clang-format --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i *.c
	clang-format --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i litesocket/*.h
	clang-format --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i litesocket/*.c