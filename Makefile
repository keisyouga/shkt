CFLAGS = -g -Wall -Wextra -Werror $(OPTION)
LDLIBS = -mwindows
OBJS = main.o shkt.res ini.o array.o table.o
WINDRES = windres
#OPTION = -DUNICODE -DDBG_PRINT_ENABLED
OPTION = -DUNICODE

shkt.exe: $(OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

clean:
	$(RM) $(OBJS)

%.res: %.rc
	$(WINDRES) -i $< -O COFF -o $@

#CC=x86_64-w64-mingw32-gcc
#WINDRES=x86_64-w64-mingw32-windres
CC=i686-w64-mingw32-gcc
WINDRES=i686-w64-mingw32-windres
