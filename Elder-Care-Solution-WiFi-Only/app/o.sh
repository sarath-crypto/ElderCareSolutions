#g++ -c motiondetector.cpp `pkg-config --cflags --libs opencv4` -Wno-psabi  -ldl -lm -g 
#g++ -c scanner.cpp        `pkg-config --cflags --libs opencv4` -Wno-psabi  -ldl -lm -g 
#g++ -c syscam.cpp         `pkg-config --cflags --libs opencv4` -llccv -Wno-psabi  -ldl -lm -g 
#g++ -c filters.cpp        -g 
#g++ -c fft.cpp            -lkissfft-float -Wno-psabi  -ldl -lm  -O1 -Wall -g 
#g++ -c udps.cpp           -Wno-psabi  -ldl -lm -Wall -g 
#g++ -c global.cpp 	   -g 
g++ -c fb.cpp             `pkg-config --cflags --libs opencv4` -llccv -Wno-psabi  -ldl -lm -Wall -g 
