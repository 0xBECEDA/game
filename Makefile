#OBJS specifies which files to compile as part of the project
OBJS = game.cpp
OBJ = game_server.c
ASMOBJS = game.S
#CC specifies which compiler we're using
CC = gcc

#COMPILER_FLAGS specifies the additional compilation options we're using
# -w suppresses all warnings
COMPILER_FLAGS = -w

#LINKER_FLAGS specifies the libraries we're linking against
LINKER_FLAGS = -lSDL2 -lm
LIB = -lpthread
#OBJ_NAME specifies the name of our exectuable
OBJ_NAME = game_server
OBJS_NAME = game
#This is the target that compiles our executable

INC = inc

VPATH = make:sdl

CFLAGS  += -m32 -g
LDFLAGS += -m32 -g

CFLAGS  += $(shell pkg-config --cflags  sdl2  -lpthread)
LDFLAGS += $(shell pkg-config --libs    sdl2	lpthread)

all : $(OBJS)
	$(CC) $(OBJS) $(COMPILER_FLAGS) $(LINKER_FLAGS) $(LIB) -o $(OBJS_NAME)
	$(CC) $(OBJ) $(COMPILER_FLAGS) $(LINKER_FLAGS) $(LIB) -o $(OBJ_NAME)
asm : $(OBJS)
	$(CC) $(OBJS) $(COMPILER_FLAGS) -S $(LINKER_FLAGS) $(CFLAGS) `pkg-config --cflags --libs sdl2` -o $(OBJ_NAME).S
sdlwrap.o: sdlwrap.c
	$(CC) -c $(CFLAGS) -I$(INC) $^ -o $@
asm_asm : sdlwrap.o $(ASMOBJS)
	gcc $(ASMOBJS) sdlwrap.o -m32 -g -lSDL2 -Iinc -o asm_hello_SDL
remade : remade.s sdlwrap.o
	gcc remade.s sdlwrap.o -m32 -g -lSDL2 -Iinc -o remade
