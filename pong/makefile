CXX = g++
CC = gcc
CFLAGS = -Wall
CXXFLAGS = -Wall -O1 -fsanitize=address -fno-omit-frame-pointer
LDFLAGS = -fsanitize=address
LDLIBS = -lwayland-client 

pong: xdg-shell-protocol.o wayland_pong.o pong.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

xdg-shell-protocol.o: ./build/xdg-shell-protocol.c
	$(CC) $(CFLAGS) -c $< -o $@

wayland_pong.o: ./code/wayland_pong.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

pong.o: ./code/pong.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o pong
