CC := gcc
CFLAGS:= -O3 -march=armv8-a+crypto -D_POSIX_SOURCE -D_GNU_SOURCE -falign-functions=64 -Wall
#CFLAGS := -O3 -D_POSIX_SOURCE -D_GNU_SOURCE -falign-functions=64 -Wall
LIBS := -lpthread -lrt -lm

# Updated util library
UTILS := ../../util/util.o

all: obj bin out data driver_aes_leak driver driver_aes

driver_aes_leak: obj/driver_aes_leak.o $(UTILS)
	$(CC) -o bin/$@ $^ $(LIBS)

driver_aes: obj/driver_aes.o $(UTILS)
	$(CC) -o bin/$@ $^ $(LIBS)

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

