SRCFILE=src/selfbot.cpp
LIBDIR=libircclient
FLAGS=-I$(LIBDIR)/include -Llibircclient -lircclient -std=c++14
OUTPUT=bot

all: $(SRCFILE)
	$(CXX) $(FLAGS) $(SRCFILE) -o $(OUTPUT)
