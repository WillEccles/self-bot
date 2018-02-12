SRCFILE=src/selfbot.cpp
LIBDIR=libircclient
FLAGS=-I$(LIBDIR)/include -Itinyexpr -Ltinyexpr -ltinyexpr -Llibircclient -lircclient -std=c++14 -O2
OUTPUT=bot

all: $(SRCFILE) tinyexpr.a
	$(CXX) $(FLAGS) $(SRCFILE) -o $(OUTPUT)

tinyexpr.a: tinyexpr/tinyexpr.o
	ar cr tinyexpr/libtinyexpr.a tinyexpr/tinyexpr.o
	ranlib tinyexpr/libtinyexpr.a

.c.o:
	$(CC) -g -O2 -O3 -c -o $@ $<
