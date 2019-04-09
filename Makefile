bin/plume: plume.c
	gcc -Wall -lfbclient -pthread -obin/plume plume.c
	strip bin/plume

bin/plume32.exe: plume.c
	i686-w64-mingw32-gcc -Wall -Iinclude/Win/ ./lib/Win32/fbclient.dll -static -pthread -obin/plume32.exe plume.c
	i686-w64-mingw32-strip bin/plume32.exe

bin/plume64.exe: plume.c
	x86_64-w64-mingw32-gcc -Wall -Iinclude/Win/ ./lib/Win64/fbclient.dll -static -pthread -obin/plume64.exe plume.c
	x86_64-w64-mingw32-strip bin/plume64.exe
