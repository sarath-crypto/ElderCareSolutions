g++ -c ../syscam.cpp `pkg-config --cflags --libs opencv4` -llccv -I../
g++ -c ../global.cpp `pkg-config --cflags --libs opencv4` -llccv -I../
g++ vidcli.cpp syscam.o global.o `pkg-config --cflags --libs opencv4` -llccv  -I../ -lmysqlcppconn -Wno-psabi -o vidcli
