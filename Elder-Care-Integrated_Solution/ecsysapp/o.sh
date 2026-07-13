g++ -c global.cpp 	  	 -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O0 -g
g++ -c udps.cpp           	 -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O0 -g
g++ -c fb.cpp             	`pkg-config --cflags --libs libdrm` `pkg-config --cflags --libs opencv4` -Wno-psabi -ldl -lm -Wall -Wextra -Wunused-variable -O0 -g
