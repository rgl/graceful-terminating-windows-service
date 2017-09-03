all: graceful-terminating-windows-service.zip

graceful-terminating-windows-service.exe: main.c
	gcc -o $@ -std=gnu99 -pedantic -Os -Wall -m64 -municode main.c
	strip $@

graceful-terminating-windows-service.zip: graceful-terminating-windows-service.exe
	zip -9 $@ $<

clean:
	rm -f graceful-terminating-windows-service.*

.PHONY: all clean
