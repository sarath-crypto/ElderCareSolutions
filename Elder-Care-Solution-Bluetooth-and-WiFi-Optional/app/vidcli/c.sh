g++ -c ../syscam.cpp `pkg-config --cflags --libs opencv4` -llccv -I../
g++ dbcam.cpp syscam.o `pkg-config --cflags --libs opencv4` -llccv  -I../ -lmysqlcppconn -Wno-psabi -o dbcam
