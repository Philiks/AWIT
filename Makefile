CC=gcc
CFLAGS=-I.
OUTPUT=cawit
OBJ=main.o chunk.o debug.o memory.o value.o vm.o
DEPS=chunk.h common.h debug.h memory.h value.h vm.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(OUTPUT): $(OBJ)
	$(CC) -o $(OUTPUT) $(OBJ)

clean:
	rm -f *.o $(OUTPUT)