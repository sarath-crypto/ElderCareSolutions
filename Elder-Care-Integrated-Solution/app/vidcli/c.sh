g++ -c ../global.cpp `pkg-config --cflags --libs opencv4` -I../
g++ -c ../syscam.cpp `pkg-config --cflags --libs opencv4` -I../
g++ vidcli.cpp syscam.o global.o `pkg-config --cflags --libs opencv4` -g -I../ -lmysqlcppconn -Wno-psabi -o vidcli
