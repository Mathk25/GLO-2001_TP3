all: ufs glofs
cleanufs: clean

ufsTest: UFS.o mainTest.o DisqueStegano.o lodepng.o
	gcc mainTest.o UFS.o DisqueStegano.o lodepng.o -o ufsTest
	
ufs_complet: UFS_remise.o main_ufs.o DisqueStegano.o lodepng.o
	gcc main_ufs.o UFS_remise.o DisqueStegano.o lodepng.o -o ufs_complet

TestsUnitaires : UFS.o TestsUnitaires.o DisqueStegano.o lodepng.o
	gcc TestsUnitaires.o UFS.o DisqueStegano.o lodepng.o -o TestsUnitaires

ufs: UFS.o main_ufs.o DisqueStegano.o lodepng.o
	gcc main_ufs.o UFS.o DisqueStegano.o lodepng.o  -g -o ufs

glofs: glofs.c UFS.o DisqueStegano.o
	gcc -D_FILE_OFFSET_BITS=64 -Wall glofs.c UFS.o DisqueStegano.o lodepng.o -o glofs -g -lfuse 

TestsUnitaires.o: TestsUnitaires.c
	gcc -c -g TestsUnitaires.c

mainTest.o: mainTest.c
	gcc -c -g mainTest.c

UFS.o: UFS.c
	gcc -c -g UFS.c

UFS_remise.o: UFS.c
	gcc -c UFS.c -o UFS_remise.o

main_ufs.o: main_ufs.c
	gcc -c -g main_ufs.c

TestDisqueStegano: lodepng.o DisqueStegano.o TestDisqueStegano.c
	gcc -g lodepng.o DisqueStegano.o TestDisqueStegano.c -o TestDisqueStegano -lrt

DisqueStegano.o: DisqueStegano.c
	gcc -g -c DisqueStegano.c -o DisqueStegano.o 

lodepng.o: lodepng.c lodepng.h
	gcc -g -c lodepng.c -o lodepng.o 

clean: 
	rm -f *.o ufs ufsTest glofs prompt TestsUnitaires TestDisqueStegano *.*~
