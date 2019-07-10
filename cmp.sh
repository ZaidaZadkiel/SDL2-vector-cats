rm vectores.bin
g++ vectores_sdl.cpp -O3 -fexpensive-optimizations -lSDL2 -lSDL2_image -std=c++11  -o vectores.bin
./vectores.bin
