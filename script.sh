make -f Make.libfs
make -f Make.main
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
./main disk.txt

