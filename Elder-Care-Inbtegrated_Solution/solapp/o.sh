g++ -c ../ecsysapp/global.cpp `pkg-config --cflags --libs opencv4` -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O0 -I../ecsysapp 
g++ -c udps.cpp               -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O0 
g++ -c fft.cpp -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O0 
g++ -c filters.cpp -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O0 



