clean:
	rm -rf bin/*

bin/plume32: plume.c
	gcc -m32 -Wall -lfbclient -pthread -obin/plume32 plume.c
	strip bin/plume32

bin/plume64: plume.c
	gcc -m64 -Wall -lfbclient -pthread -obin/plume64 plume.c
	strip bin/plume64

bin/plume32.exe: plume.c
	i686-w64-mingw32-gcc -Wall -I../Firebird.w32/include/ ../Firebird.w32/fbclient.dll -static -pthread -obin/plume32.exe plume.c
	i686-w64-mingw32-strip bin/plume32.exe

bin/plume64.exe: plume.c
	x86_64-w64-mingw32-gcc -Wall -I../Firebird.w64/include/ ../Firebird.w64/fbclient.dll -static -pthread -obin/plume64.exe plume.c
	x86_64-w64-mingw32-strip bin/plume64.exe
