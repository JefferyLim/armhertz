CC := gcc
CFLAGS := -O3 -D_POSIX_SOURCE -D_GNU_SOURCE -falign-functions=64 -Wall
LIBS := -lpthread -lrt -lm

# Updated util library
UTILS := ../../util/util.o

all: obj bin out data driver driver-steady

driver: obj/driver.o $(UTILS)
	$(CC) -o bin/$@ $^ $(LIBS)

driver-steady: obj/driver-steady.o $(UTILS)
	$(CC) -o bin/$@ $^ $(LIBS)

obj/%.o: %.c
	$(CC) -c $(CFLAGS) -DSLEEP=1 -o $@ $<

obj/%-steady.o: %.c
	$(CC) -c $(CFLAGS) -DSLEEP=0 -o $@ $<

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
	rm -rf ../util/*.o

