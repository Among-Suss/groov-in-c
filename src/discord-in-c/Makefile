all: test2

test2: test2.c cJSON.h cJSON.c media.c media.h discord.c discord.h strmap.c strmap.h
	gcc -o test2 test2.c cJSON.h cJSON.c media.c media.h discord.c discord.h litesocket/litesocket.h litesocket/litesocket.c strmap.h strmap.c sbuf.h sbuf.c -g -Wall -Wextra -Wno-unused-parameter -pedantic -lpthread -lssl -lcrypto -lsodium -lopus -logg -lcurl


format:
	clang-format-10 --style=llvm -i discord.h
	clang-format-10 --style=llvm -i discord.c
	clang-format-10 --style=llvm -i test2.c

#-fsanitize=address
