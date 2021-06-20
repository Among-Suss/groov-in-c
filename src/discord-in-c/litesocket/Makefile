all: test

test: test.c litesocket.h litesocket.c
	gcc -o test test.c litesocket.h litesocket.c -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address -pedantic -lpthread -lssl -lcrypto

format:
	clang-format-10 --style=llvm -i *.h
	clang-format-10 --style=llvm -i *.c