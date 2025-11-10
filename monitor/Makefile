# Define the compiler and the flags
CC = g++
CFLAGS = -Wall -g -O3

# Define the source file and the output file
SRC = monitor.c
OUT = monitor

# The default target, will build the output file
all: $(OUT)

# Rule to compile the source file and link it
$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

# Clean up the compiled files
clean:
	rm -f $(OUT)

# Phony targets (not filenames)
.PHONY: all clean
