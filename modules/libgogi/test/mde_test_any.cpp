#ifdef WIN32
#include <windows.h>
#endif

#include "../mde.h"
#include <string.h>
#include <stdio.h>

#define MDE_SCREEN_W 800
#define MDE_SCREEN_H 600

void InitTest(MDE_Screen *screen);
void ShutdownTest();
void PulseTest(MDE_Screen *screen);

class TestWindow : public MDE_Screen
{
public:
	TestWindow();
	~TestWindow();

	// == Inherit MDE_View ========================================
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);

	// == Inherit MDE_Screen ======================================
	virtual void OutOfMemory();
	virtual MDE_FORMAT GetFormat();
	virtual MDE_BUFFER *LockBuffer();
	virtual void UnlockBuffer(MDE_Region *update_region);

public:
	unsigned int *data;
	MDE_BUFFER screen;
};

TestWindow::TestWindow()
{
	Init(MDE_SCREEN_W, MDE_SCREEN_H);
	data = new unsigned int[MDE_SCREEN_W * MDE_SCREEN_H];
}

TestWindow::~TestWindow()
{
	delete [] data;
}

void TestWindow::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	printf("TestWindow::OnPaint\n");
}

MDE_FORMAT TestWindow::GetFormat()
{
	return MDE_FORMAT_BGRA32;
}

void TestWindow::OutOfMemory()
{
	printf("TestWindow::OutOfMemory\n");
}

MDE_BUFFER *TestWindow::LockBuffer()
{
	printf("TestWindow::LockBuffer\n");
	MDE_InitializeBuffer(MDE_SCREEN_W, MDE_SCREEN_H, MDE_FORMAT_BGRA32, data, NULL, &screen);
	return &screen;
}

void TestWindow::UnlockBuffer(MDE_Region *update_region)
{
	printf("TestWindow::UnlockBuffer with %d updaterectangles.\n", update_region->num_rects);
}

int main()
{
	TestWindow *tmp = new TestWindow();

	InitTest(tmp);

	for(int i = 0; i < 5; i++)
	{
		tmp->Validate(true);
		PulseTest(tmp);
	}

	ShutdownTest();

	printf("Test is done.\n");
	return 0;
}

#ifdef WIN32
int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	main();
	return 0;
}
#endif
