all: test2

test2: test2.c discord.c discord.h strmap.c strmap.h
	gcc -o test2 test2.c discord.c discord.h litesocket/litesocket.h litesocket/litesocket.c strmap.h strmap.c -g -Wall -Wextra -Wno-unused-parameter -pedantic -fsanitize=address -lpthread -lssl -lcrypto

format:
	clang-format-10 --style=llvm -i discord.h
	clang-format-10 --style=llvm -i discord.c
	clang-format-10 --style=llvm -i test2.c
