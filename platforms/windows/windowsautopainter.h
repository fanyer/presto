#ifndef WINDOWSAUTOPAINTER_H
#define WINDOWSAUTOPAINTER_H

class WindowsAutoPainter
{
public:

	WindowsAutoPainter(HWND hwnd);

	~WindowsAutoPainter();

	HDC GetHDC() {return dc;}

	void GetPaintStruct(PAINTSTRUCT* ps);

	BOOL IsEmpty();

private:
	HWND hwnd;
	PAINTSTRUCT ps;
	HDC dc;
};

#endif // WINDOWSAUTOPAINTER_H
