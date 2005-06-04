# Project: netmag
# Makefile created by Dev-C++ 4.9.6.4

CC   = g++.exe
OBJ  = netmag.o
LIBS =  -L"C:/DEV-CPP/lib" # -lopengl32 -lglu32 -lglaux -lkernel32 -luser32 -lgdi32 -lwinspool -lcomdlg32 -ladvapi32 -lshell32 -lole32 -loleaut32 -luuid -lodbc32 -lodbccp32
INCS =  -I"C:/DEV-CPP/include"  -I"C:/DEV-CPP/include/g++-3"  -I"C:/DEV-CPP/include" 
BIN  = netmag.exe
CFLAGS = $(INCS) -D__GNUWIN32__ -Wall -DWIN32 -DNDEBUG -D_WINDOWS -D_MBCS -g3 -mwindows

.PHONY: all all-before all-after clean clean-custom

all: all-before netmag.exe all-after

remake: clean all

clean: clean-custom
	rm -f $(OBJ) $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o "netmag.exe" $(LIBS) $(CFLAGS)

netmag.o: netmag.cpp
	$(CC) -c netmag.cpp -o netmag.o $(CFLAGS)

run: all
	netmag.exe
