SRCFILE=src/selfbot.cpp
LIBDIR=libircclient
FLAGS=-I$(LIBDIR)/include -Itinyexpr -Llibircclient -lircclient -std=c++14 -O2
OUTPUT=bot

all: $(SRCFILE) tinyexpr
	$(CXX) $(FLAGS) $(SRCFILE) -o $(OUTPUT)

tinyexpr:
	$(CC) tinyexpr/tinyexpr.c -o tinyexpr/tinyexpr.o -O2
