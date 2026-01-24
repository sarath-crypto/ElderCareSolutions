#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <vector>

#include "global.h"

using namespace std;
using namespace cv;

enum type{USB_CAMERA = 1,NO_CAMERA = 255};

class syscam{
	private:
		VideoCapture usb;
	public:
		unsigned char type;
		string usbid;
		syscam(string);
		~syscam();
		bool get_frame(Mat &);
};
