OBJDIR=obj
CC=gcc
CFLAGS=-g -Wall -Wextra -Wno-unused-parameter
MACROS=-DLOG_USE_COLOR -DLOGGING
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

$(OBJDIR)/%.o: %.c %.h
	$(CC) -o $@ -c $(MACROS) $(*F).c $(CFLAGS)

$(OBJDIR):
	mkdir -p $(OBJDIR)



clean:
	rm -rf test2
	rm -rf obj

clean-deep: clean
	rm -rf audiotmpfile*
	rm -rf output.log

format:
	clang-format --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i *.h
	clang-format --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i *.c
	clang-format --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i litesocket/*.h
	clang-format --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i litesocket/*.c