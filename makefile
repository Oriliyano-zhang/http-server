SrcFiles=$(wildcard *.c)
Target=$(patsubst %.c,%.o,$(SrcFiles))

server:$(Target)
	gcc -o server $(Target) -g -Wall

%.o:%.c
	gcc -c $< -o $@

.PHONY:clean all
clean:
	-rm -rf $(Target)
