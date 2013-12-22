sweep: sweep.cpp
	c++ -o sweep sweep.cpp $(shell pkg-config --cflags libpng) $(shell pkg-config --libs libpng)
