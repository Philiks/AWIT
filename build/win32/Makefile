CC=i686-w64-mingw32-gcc
CFLAGS=-I.
OUTPUT=awit
SOURCE=../../src/
OBJ=$(addprefix $(SOURCE), main.o chunk.o compiler.o debug.o memory.o object.o scanner.o table.o value.o vm.o)
DEPS=$(addprefix $(SOURCE), chunk.h common.h compiler.h debug.h memory.h object.h scanner.h table.h value.h vm.h)

.PHONY: $(OUTPUT) clean

all: $(OUTPUT) clean

%.o: %.c $(DEPS)
	$(CC) -c -o $@ -g $< $(CFLAGS)

$(OUTPUT): $(OBJ)
	$(CC) -o $(OUTPUT) $(OBJ)

clean:
	rm -f $(OBJ)
