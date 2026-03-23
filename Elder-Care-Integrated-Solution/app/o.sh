g++ -c motiondetector.cpp 	`pkg-config --cflags --libs opencv4` -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O1
g++ -c scanner.cpp        	`pkg-config --cflags --libs opencv4` -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O1
g++ -c filters.cpp        	-ldl -lm -Wall -Wextra -Wunused-variable -O1
g++ -c udps.cpp           	`pkg-config --cflags --libs opencv4` -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O1
g++ -c syscam.cpp         	`pkg-config --cflags --libs opencv4` -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O1 
g++ -c global.cpp 	  	`pkg-config --cflags --libs opencv4` -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O1
g++ -c fft.cpp            	`pkg-config --cflags --libs opencv4` -lkissfft-float -Wno-psabi  -ldl -lm -Wall -Wextra -Wunused-variable -O1
g++ -c ds18b20.cpp		-lwiringPi -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O1
g++ -c fb.cpp             	`pkg-config --cflags --libs opencv4` -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O1
