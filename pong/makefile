CXX = g++
CC = gcc
CFLAGS = -Wall
CXXFLAGS = -Wall
LDFLAGS = -lwayland-client

pong: xdg-shell-protocol.o pong.o
	$(CXX) $^ $(LDFLAGS) -o $@

xdg-shell-protocol.o: ./build/xdg-shell-protocol.c
	$(CC) $(CFLAGS) -c $< -o $@

pong.o: ./code/wayland_pong.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o pong
