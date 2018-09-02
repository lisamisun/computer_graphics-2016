WORKDIR = `pwd`

CC = gcc
CXX = g++
AR = ar
LD = g++
WINDRES = windres

INC = -IUtility/include -IUtility/include/VM -IUtility/include/GL -ISimple\ OpenGL\ Image\ Library/src
CFLAGS = -Wall -fexceptions
RESINC = 
LIBDIR = -Lbin/lib -LExternal/lib/win32
LIB = 
LDFLAGS = -lUtility -lSOIL -lGL -lglut -lGLEW -lGLU

INC_RELEASE = $(INC)
CFLAGS_RELEASE = $(CFLAGS) -O2 -std=c++11
RESINC_RELEASE = $(RESINC)
RCFLAGS_RELEASE = $(RCFLAGS)
LIBDIR_RELEASE = $(LIBDIR)
LIB_RELEASE = $(LIB)
LDFLAGS_RELEASE = $(LDFLAGS) -s
OBJDIR_RELEASE = obj/Release
DEP_RELEASE = 
OUT_RELEASE = bin/Instancing

OBJ_RELEASE = $(OBJDIR_RELEASE)/main.o

all: 
	cd Utility; make all
	cd 'Simple OpenGL Image Library'; make all; make install
	make release
	
clean: 
	cd Utility; make clean
	cd 'Simple OpenGL Image Library'; make clean
	make clean_release
	
run:
	cp -r shaders bin/shaders
	cd bin; ./Instancing

before_release: 
	test -d bin || mkdir -p bin
	test -d $(OBJDIR_RELEASE) || mkdir -p $(OBJDIR_RELEASE)

after_release: 
	chmod -R 777 bin
	chmod -R 777 obj
	
release: before_release out_release after_release

out_release: before_release $(OBJ_RELEASE) $(DEP_RELEASE)
	$(LD) $(LIBDIR_RELEASE) -o $(OUT_RELEASE) $(OBJ_RELEASE)  $(LDFLAGS_RELEASE) $(LIB_RELEASE)

$(OBJDIR_RELEASE)/main.o: main.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -c main.cpp -o $(OBJDIR_RELEASE)/main.o

clean_release: 
	rm -f $(OBJ_RELEASE) $(OUT_RELEASE)
	rm -rf bin
	rm -rf $(OBJDIR_RELEASE)
	rm -rf obj

.PHONY: before_release after_release clean_release

