all: discord-config-reader

discord-config-reader: discord-config-reader.c
	gcc -o discord-config-reader discord-config-reader.c ../strmap.h ../strmap.c -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address -pedantic -lpthread -lssl -lcrypto

format:
	clang-format-10 --style=llvm -i *.h
	clang-format-10 --style=llvm -i *.c