all: discord-commands

discord-commands: discord-commands.c
	gcc -o discord-commands youtube/youtube-util.c youtube/youtube-func.c discord-utils.c discord-commands.c -lcurl -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address -pedantic

discord-config-reader: discord-config-reader.c
	gcc -o discord-config-reader discord-config-reader.c ../strmap.h ../strmap.c -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address -pedantic

format:
	clang-format-10 --style=llvm -i *.h
	clang-format-10 --style=llvm -i *.c