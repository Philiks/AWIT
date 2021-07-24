CC=gcc
CFLAGS=-I.
OUTPUT=cawit
OBJ=main.o chunk.o compiler.o debug.o memory.o object.o scanner.o value.o vm.o
DEPS=chunk.h common.h compiler.h debug.h memory.h object.h scanner.h value.h vm.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(OUTPUT): $(OBJ)
	$(CC) -o $(OUTPUT) $(OBJ)

clean:
	rm -f *.o $(OUTPUT)