#include "pch.h"

using namespace cv;
using namespace std;
using namespace std::chrono;

mutex mu;
int threads = 1;
float scale = 1.0f;
float hranges[] = { 0,180 };
const float* phranges[] = { hranges,hranges,hranges };

int imgSize[2], frame[2];	//imgSize[0] = width(x) , imgSize[1] = height(y) |  frame[0] for calculation, frame[1] for output
char win[] = "output";
bool stop = false;

Mat templ = Mat(10, 10, CV_8UC3);
vector<Point> pt;
time_point<system_clock> refz;

struct MaxVals {
	Point point;
	float val;
};

Mat hwnd2mat(HWND hwnd)
{
	HDC hwindowDC, hwindowCompatibleDC;

	int height, width, srcheight, srcwidth;
	HBITMAP hbwindow;
	Mat src;
	BITMAPINFOHEADER  bi;

	hwindowDC = GetDC(hwnd);
	hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);
	SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);

	RECT windowsize;    // get the height and width of the screen
	GetClientRect(hwnd, &windowsize);

	srcheight = windowsize.bottom;
	srcwidth = windowsize.right;
	height = windowsize.bottom / 1;  //change this to whatever size you want to resize to
	width = windowsize.right / 1;

	src.create(height, width, CV_8UC4);

	// create a bitmap
	hbwindow = CreateCompatibleBitmap(hwindowDC, width, height);
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = width;
	bi.biHeight = -height;  //this is the line that makes it draw upside down or not
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	// use the previously created device context with the bitmap
	SelectObject(hwindowCompatibleDC, hbwindow);
	// copy from the window device context to the bitmap device context
	StretchBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, srcwidth, srcheight, SRCCOPY); //change SRCCOPY to NOTSRCCOPY for wacky colors !
	GetDIBits(hwindowCompatibleDC, hbwindow, 0, height, src.data, (BITMAPINFO *)&bi, DIB_RGB_COLORS);  //copy from hwindowCompatibleDC to hbwindow

	// avoid memory leak
	DeleteObject(hbwindow);
	DeleteDC(hwindowCompatibleDC);
	ReleaseDC(hwnd, hwindowDC);

	resize(src, src, Size(imgSize[0], imgSize[1]));
	cvtColor(src, src, CV_BGRA2BGR);
	return src;
}

void mouse_callback(int  event, int  x, int  y, int  flag, void *param)
{
	if (event == EVENT_LBUTTONDOWN)
		pt.push_back(Point(x, y));
}

void FPSPrint(Mat imgBGR)
{
	if (duration_cast<milliseconds>(system_clock::now() - refz).count() >= 1000)
	{
		frame[1] = frame[0];
		frame[0] = 0;
		refz = system_clock::now();
	}
	putText(imgBGR, "FPS:" + to_string(frame[1]), Point(5, 20),FONT_HERSHEY_DUPLEX, 0.7, Scalar(0, 0, 0), 3, 11);
	putText(imgBGR, "FPS:" + to_string(frame[1]), Point(5, 20),FONT_HERSHEY_DUPLEX, 0.7, Scalar(255, 255, 255), 2, 11);
}

bool newTemplate(Mat img, Mat& roi_hist, Rect& rec, int *ch, int *hsize)
{
	if (pt.size() == 2)
	{
		Mat maskroi, hsv_roi;
		rec = Rect(pt[0], pt[1]);

		hsv_roi = img(rec);
		cvtColor(hsv_roi, hsv_roi, CV_BGR2HSV);
		calcHist(&hsv_roi, 1, ch, Mat(), roi_hist, 1, hsize, phranges);
		normalize(roi_hist, roi_hist, 0, 255, NORM_MINMAX);
		pt.clear();
		return true;
	} 
	return false;
}

void MatchingMethod()
{
	Mat img, hsv, dst, roi_hist;
	int ch[] = { 0, 1, 2 }, hsize[] = { 16,16,16 };
	HWND hwndDesktop = GetDesktopWindow();
	bool got = false;
	TermCriteria crit = TermCriteria(TermCriteria::COUNT, 200,1);
	Rect rect;

	while (!stop)
	{
		img = hwnd2mat(hwndDesktop);
		FPSPrint(img);
		if (newTemplate(img, roi_hist, rect, ch, hsize) || got == true)
		{
			got = true;
			cvtColor(img, hsv, COLOR_BGR2HSV);
			calcBackProject(&hsv, 1, ch, roi_hist, dst, phranges);
			meanShift(dst, rect, crit);
			rectangle(img, rect, Scalar(255, 128, 128), 2);		
		}
		imshow(win, img);
		waitKey(1);
		frame[0]++;
		
	}
}

void MatchStart()
{
	vector<future<void>> tasks;
	refz = system_clock::now();

	imgSize[0] = (int)(GetSystemMetrics(SM_CXSCREEN) / scale);
	imgSize[1] = (int)(GetSystemMetrics(SM_CYSCREEN) / scale);

	for (int i = 0; i < threads; i++) {
		tasks.push_back(async(launch::async, MatchingMethod));
		Sleep(50);
	}


	while (!(GetAsyncKeyState(VK_ESCAPE) & 1))
	{
		waitKey(1);
	}
	stop = true;
	tasks.clear();
}

int main()
{
	namedWindow(win, WINDOW_AUTOSIZE);
	setMouseCallback(win, mouse_callback);
	MatchStart();
	destroyAllWindows();

	return 0;
}

//	SetCursorPos((matchLoc.point.x*scale) + 20, (matchLoc.point.y*scale) + 20);
//  mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
