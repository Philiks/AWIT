CC=gcc
CFLAGS=-I.
# Use 'make PF=-pg' to enable gprof profiler.
PF=
OUTPUT=awit
OBJ=main.o chunk.o compiler.o debug.o memory.o object.o scanner.o table.o value.o vm.o
DEPS=chunk.h common.h compiler.h debug.h memory.h object.h scanner.h table.h value.h vm.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(PF)

$(OUTPUT): $(OBJ)
	$(CC) -o $(OUTPUT) $(OBJ) $(PF)

clean:
	rm -f *.o $(OUTPUT)
