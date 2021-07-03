all: youtube-func

# Librarys
INCLUDE = -Iusr/local/include
LDFLAGS = -Lusr/local/lib 
LDLIBS = -lcurl

youtube-func: youtube-func.h youtube-func.c youtube-util.h youtube-util.c
	gcc -o youtube-func youtube-func.c youtube-util.c $(INCLUDE) $(LDFLAGS) $(LDLIBS) -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address -pedantic -lpthread -lssl -lcrypto

format:
	clang-format-10 --style=llvm -i *.h
	clang-format-10 --style=llvm -i *.c