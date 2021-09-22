all: test2

test2: test2.c cJSON.h cJSON.c media.c media.h discord.c discord.h strmap.c strmap.h
	gcc -o test2 -DBOT_PREFIX=\"$${BOT_PREFIX:--}\" test2.c cJSON.h cJSON.c media.c media.h discord.c discord.h litesocket/litesocket.h litesocket/litesocket.c strmap.h strmap.c sbuf.h sbuf.c -g -Wall -Wextra -Wno-unused-parameter -pedantic -O3 -lpthread -lssl -lcrypto -lsodium -lopus -logg -lm


format:
	clang-format-10 --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i discord.h
	clang-format-10 --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i discord.c
	clang-format-10 --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i test2.c

#-fsanitize=address
