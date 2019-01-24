plume:
	gcc -Wall -lfbclient -pthread -oplume plume.c
	strip plume

plume32.exe:
	i686-w64-mingw32-gcc -Wall -Iinclude/Win/ ./lib/Win32/fbclient.dll -static -pthread -oplume32.exe plume.c
	i686-w64-mingw32-strip plume32.exe

plume64.exe:
	x86_64-w64-mingw32-gcc -Wall -Iinclude/Win/ ./lib/Win64/fbclient.dll -static -pthread -oplume64.exe plume.c
	x86_64-w64-mingw32-strip plume64.exe
