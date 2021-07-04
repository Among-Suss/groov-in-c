all: youtube-func

youtube-func: youtube-func.cpp
	gcc -o youtube-func.o youtube-func.cpp -lcurl -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address -pedantic

format:
	clang-format-10 --style=llvm -i *.h
	clang-format-10 --style=llvm -i *.c