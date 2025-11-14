make
./run_steady.sh steady_add

sed -i 's/add /eor /g' driver.c
make

./run_steady.sh steady_eor

sed -i 's/eor /sub /g' driver.c
make

./run_steady.sh steady_sub

sed -i 's/sub /lsl /g' driver.c
make

./run_steady.sh steady_lsl

sed -i 's/lsl /mul /g' driver.c
make

./run_steady.sh steady_mul

sed -i 's/mul /orr /g' driver.c
make

./run_steady.sh steady_orr


sed -i 's/mul /add /g' driver.c
make

./run_drops.sh drop_add

sed -i 's/add /eor /g' driver.c
make

./run_drops.sh drop_eor


sed -i 's/eor /lsl /g' driver.c
make

./run_drops.sh drop_lsl


sed -i 's/lsl /mul /g' driver.c
make

./run_drops.sh drop_mul

sed -i 's/mul /orr /g' driver.c
make


./run_drops.sh drop_orr
