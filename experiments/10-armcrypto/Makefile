CC:= gcc
CFLAGS:= -O3 -march=armv8-a+crypto -D_POSIX_SOURCE -D_GNU_SOURCE -falign-functions=64 -Wall
LIBS:= -lpthread -lrt -lm
UTILS:= ../../util/util.o

all: obj bin out data driver

driver: obj/driver.o $(UTILS)
	$(CC) -o bin/$@ $^ $(LIBS)

obj/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

obj:
	mkdir -p $@

bin:
	mkdir -p $@

out:
	mkdir -p $@

data:
	mkdir -p $@

clean:
	rm -rf bin obj out
	rm -rf ../../util/*.o

.PHONY: all clean
