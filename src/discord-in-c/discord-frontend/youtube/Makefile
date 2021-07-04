all: youtube-func

youtube-func: youtube-func.h youtube-func.c youtube-util.h youtube-util.c
	gcc -o youtube-func.o youtube-func.c youtube-util.c -lcurl -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address -pedantic

format:
	clang-format-10 --style=llvm -i *.h
	clang-format-10 --style=llvm -i *.c