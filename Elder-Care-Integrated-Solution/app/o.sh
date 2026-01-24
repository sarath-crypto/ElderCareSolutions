g++ -c global.cpp 	  `pkg-config --cflags --libs opencv4` -g
g++ -c motiondetector.cpp `pkg-config --cflags --libs opencv4` -Wno-psabi  -ldl -lm -g 
g++ -c scanner.cpp        `pkg-config --cflags --libs opencv4` -Wno-psabi  -ldl -lm -g 
g++ -c syscam.cpp         `pkg-config --cflags --libs opencv4` -Wno-psabi  -ldl -lm -g 
g++ -c filters.cpp        -g 
g++ -c fft.cpp            `pkg-config --cflags --libs opencv4` -lkissfft-float -Wno-psabi  -ldl -lm  -O1 -Wall -g 
g++ -c udps.cpp           `pkg-config --cflags --libs opencv4` -Wno-psabi  -ldl -lm -Wall -g 
g++ -c fb.cpp             `pkg-config --cflags --libs opencv4` -Wno-psabi  -ldl -lm -Wall -g 
