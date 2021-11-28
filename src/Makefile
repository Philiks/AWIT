CC=gcc
CFLAGS=-I.
OUTPUT=awit
OBJ=main.o chunk.o compiler.o debug.o memory.o object.o scanner.o table.o value.o vm.o
DEPS=chunk.h common.h compiler.h debug.h memory.h object.h scanner.h table.h value.h vm.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) -pg 

$(OUTPUT): $(OBJ)
	$(CC) -o $(OUTPUT) $(OBJ) -pg

clean:
	rm -f *.o $(OUTPUT)
