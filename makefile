CC = gcc 
CFLAGS = -Wall -g -D_POSIX_C_SOURCE=199309L
EXE = voting

all: $(EXE)

.PHONY: clean
clean:
	rm -f *.o core $(EXE)

$(EXE): %: %.o
	$(CC) $(CFLAGS) -o $@ $^

run:
	./voting 10 1

runv:
	@echo Running exercise1 with valgrind
	@valgrind --leak-check=full --show-leak-kinds=all --log-file=valgrind.log ./voting 10 10

