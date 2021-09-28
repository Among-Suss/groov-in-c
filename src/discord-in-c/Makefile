all: test2


test2: test2.c cJSON.h cJSON.c log.h log.c utils.h media.c media.h discord.c discord.h litesocket/litesocket.h litesocket/litesocket.c strmap.h strmap.c sbuf.h sbuf.c youtube_fetch.h youtube_fetch.c
	gcc -o test2 test2.c cJSON.h cJSON.c -DLOG_USE_COLOR log.h log.c media.c media.h discord.c discord.h litesocket/litesocket.h litesocket/litesocket.c strmap.h strmap.c sbuf.h sbuf.c youtube_fetch.h youtube_fetch.c -g -Wall -Wextra -Wno-unused-parameter -pedantic -O3 -lpthread -lssl -lcrypto -lsodium -lopus -logg -lm -lcurl

sanitize: test2.c cJSON.h cJSON.c log.h log.c utils.h media.c media.h discord.c discord.h litesocket/litesocket.h litesocket/litesocket.c strmap.h strmap.c sbuf.h sbuf.c youtube_fetch.h youtube_fetch.c
	gcc -o test2 test2.c cJSON.h cJSON.c -DLOG_USE_COLOR log.h log.c media.c media.h discord.c discord.h litesocket/litesocket.h litesocket/litesocket.c strmap.h strmap.c sbuf.h sbuf.c youtube_fetch.h youtube_fetch.c -g -Wall -Wextra -Wno-unused-parameter -pedantic -O3 -lpthread -lssl -lcrypto -lsodium -lopus -logg -lm -lcurl -fsanitize=address

sanitize-ubuntu: test2.c cJSON.h cJSON.c log.h log.c utils.h media.c media.h discord.c discord.h litesocket/litesocket.h litesocket/litesocket.c strmap.h strmap.c sbuf.h sbuf.c youtube_fetch.h youtube_fetch.c
	gcc -o test2 test2.c cJSON.h cJSON.c -DLOG_USE_COLOR log.h log.c media.c media.h discord.c discord.h litesocket/litesocket.h litesocket/litesocket.c strmap.h strmap.c sbuf.h sbuf.c youtube_fetch.h youtube_fetch.c -g -Wall -Wextra -Wno-unused-parameter -pedantic -lpthread -lssl -lcrypto -lsodium -lopus -logg -lm -lcurl -fsanitize=address


format:
	clang-format-10 --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i discord.h
	clang-format-10 --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i discord.c
	clang-format-10 --style="{BasedOnStyle: llvm, AllowShortFunctionsOnASingleLine: None, SortIncludes: false}" -i test2.c

#-fsanitize=address
