#include "core/pch.h"
#include "../mde.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <string.h>

//#define DEBUG_REPAINT_REGION

static struct MDE_BUFFER *testimage[7];
static MDE_View *menuview;

#define col_bg MDE_RGB(64, 64, 64)
#define col_b1 MDE_RGB(128, 128, 128)
#define col_b2 MDE_RGB(0, 0, 0)

// == Load bitmapfunction for testbitmaps =========================================

MDE_BUFFER *LoadTGA(const char *filename)
{
	MDE_BUFFER *buf = NULL;

	FILE *file;
	if( (file = fopen(filename,"rb")) )
	{
		short int width, height, bpp;

		fseek(file, 12, SEEK_SET);	fread(&width, sizeof(short int), 1, file);
		fseek(file, 14, SEEK_SET);	fread(&height, sizeof(short int), 1, file);
		fseek(file, 16, SEEK_SET);	fread(&bpp, sizeof(short int), 1, file);
		bpp &= 0xFF;

		if(bpp != 24 && bpp != 32)
		{
			fclose(file);
			return NULL;
		}

		buf = MDE_CreateBuffer(width, height, MDE_FORMAT_BGRA32, 0);

		if(buf)
		{
			fseek(file, 18, SEEK_SET);
			unsigned int *data = (unsigned int*) buf->data;
			for(int j=height-1; j>=0; j--)
			{
				for(int i=0; i<width; i++)
				{
					MDE_COL_B(data[i+j*width]) = (unsigned char) fgetc(file);
					MDE_COL_G(data[i+j*width]) = (unsigned char) fgetc(file);
					MDE_COL_R(data[i+j*width]) = (unsigned char) fgetc(file);
					MDE_COL_A(data[i+j*width]) = bpp == 24	? (unsigned char) 255
															: (unsigned char) fgetc(file);
				}
			}
		}
		fclose(file);
	}
	return buf;
}

// == MoveView ====================================================================

class MoveView : public MDE_View
{
public:
	virtual void OnMouseDown(int x, int y, int button, int clicks);
	virtual void OnMouseUp(int x, int y, int button);
	virtual void OnMouseMove(int x, int y);
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
	virtual void Pulse() {}
public:
	static int down_x, down_y, move;
};

int MoveView::down_x = 0;
int MoveView::down_y = 0;
int MoveView::move = false;

void MoveView::OnMouseDown(int x, int y, int button, int clicks)
{
	if (clicks == 2)
	{
		SetVisibility(!IsVisible());
		menuview->Invalidate(MDE_MakeRect(0, 0, menuview->m_rect.w, menuview->m_rect.h), true);
		return;
	}

	down_x = x;
	down_y = y;
	move = true;
	if (button == 1)
		SetZ(MDE_Z_TOP);
	if (button == 2)
		SetZ(MDE_Z_BOTTOM);
}

void MoveView::OnMouseUp(int x, int y, int button)
{
	move = false;
}

void MoveView::OnMouseMove(int x, int y)
{
	if (move)
		SetRect(MDE_MakeRect(m_rect.x + x - down_x, m_rect.y + y - down_y, m_rect.w, m_rect.h));
}

void MoveView::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MDE_SetColor(col_b1, screen);
	MDE_DrawLine(0, 0, m_rect.w - 1, 0, screen);
	MDE_DrawLine(0, 0, 0, m_rect.h - 1, screen);
	MDE_SetColor(col_b2, screen);
	MDE_DrawLine(m_rect.w - 1, 1, m_rect.w - 1, m_rect.h - 1, screen);
	MDE_DrawLine(1, m_rect.h - 1, m_rect.w - 1, m_rect.h - 1, screen);
}

// == MENU ====================================================================

class MenuButtonView : public MoveView
{
public:
	MenuButtonView(const MDE_RECT &rect, MDE_View* testview);
	virtual void OnMouseDown(int x, int y, int button, int clicks);
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
private:
	MDE_View* testview;
};

MenuButtonView::MenuButtonView(const MDE_RECT &rect, MDE_View* testview) : testview(testview)
{
	SetRect(rect);
}

void MenuButtonView::OnMouseDown(int x, int y, int button, int clicks)
{
	testview->SetVisibility(!testview->IsVisible());
	Invalidate(MDE_MakeRect(0, 0, m_rect.w, m_rect.h));
}

void MenuButtonView::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	unsigned int color = testview->IsVisible() ? MDE_RGB(0, 128, 0) : col_bg;
	MDE_SetColor(color, screen);
	MDE_DrawRectFill(rect, screen);
	MoveView::OnPaint(rect, screen);
}

class MenuView : public MoveView
{
public:
	MenuView();
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
	virtual void OnAdded();
};

MenuView::MenuView()
{
}

void MenuView::OnAdded()
{
	int x = 5;
	MDE_View *tmp = m_screen->m_first_child;
	while(tmp)
	{
		AddChild(new MenuButtonView(MDE_MakeRect(x, 5, 10, 10), tmp));
		x += 15;
		tmp = tmp->m_next;
	}

	SetRect(MDE_MakeRect(10, 10, x, 20));
}

void MenuView::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MDE_SetColor(col_bg, screen);
	MDE_DrawRectFill(rect, screen);

	MoveView::OnPaint(rect, screen);
}

// == BlurView ====================================================================

class BlurView : public MoveView
{
public:
	BlurView();
	~BlurView();
	void PutPixel(int x, int y, unsigned char col);
	virtual void Pulse();
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
private:
	int g;
	MDE_BUFFER *blurbuffer;
};

BlurView::BlurView() : g(0)
{
	SetRect(MDE_MakeRect(50, 50, 200, 200));
	blurbuffer = MDE_CreateBuffer(m_rect.w, m_rect.h, MDE_FORMAT_INDEX8, 256);
	op_memset(blurbuffer->data, 0, blurbuffer->stride * blurbuffer->h);
}

BlurView::~BlurView()
{
	delete blurbuffer;
}

void BlurView::PutPixel(int x, int y, unsigned char col)
{
//	if (x >= 0 && y >= 0 && x < blurbuffer->w && y < blurbuffer->h)
		((unsigned char *)blurbuffer->data)[x + y * blurbuffer->stride] = col;
}

void BlurView::Pulse()
{
	g += 16;
}

void BlurView::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	int i, x;

	// Create palette
	for(x = 0; x < 256; x++)
	{
		blurbuffer->pal[x * 3 + 0] = x;
		blurbuffer->pal[x * 3 + 1] = MDE_MIN((long)(x * x / 100.0f), 255);
		blurbuffer->pal[x * 3 + 2] = MDE_MIN(x * 2, 255);
	}

	int w = blurbuffer->w;
	int h = blurbuffer->h;
	// Draw pixels
	for(x = g; x < 300 + g; x++)
		PutPixel((int) (w/2 + (w/2-2) * cos(x/23.0)*sin((x)/85.0)),
				(int) (h/2 + (h/2-2) * sin((x)/16.5+g/100.0)*cos(x/17.0)),
				(unsigned char) (155+100*cos(x/30.0)));

	// Do blur
	int to = blurbuffer->w * blurbuffer->h - blurbuffer->w * 2;
	unsigned char *dst8 = (unsigned char *) blurbuffer->data;
	for(i = blurbuffer->w; i < to; i++)
		dst8[i] = (unsigned char) (long(dst8[i + blurbuffer->w] + dst8[i - blurbuffer->w] + dst8[i - 1] + dst8[i + 1]) >> 2);

	// Blit to screen
	MDE_DrawBuffer(blurbuffer, rect, rect.x, rect.y, screen);
	MoveView::OnPaint(rect, screen);

	Invalidate(rect);
}

// == TEST 01 ====================================================================

class TestView01 : public MoveView
{
public:
	TestView01();
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
};

TestView01::TestView01()
{
	SetRect(MDE_MakeRect(50, 50, 50, 50));
}

void TestView01::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MDE_SetColor(rand(), screen);
	MDE_RECT r3 = { -5, -5, 300, 200 };
	MDE_DrawRectFill(r3, screen);

	MDE_SetColor(0xFFFFFFFF, screen);
	MDE_RECT r4 = { 1, 1, 5, 5};
	MDE_DrawRect(r4, screen);
	MDE_DrawRectInvert(r4, screen);

	MDE_DrawRect(MDE_MakeRect(10, 1, -1, -1), screen);
	MDE_DrawRect(MDE_MakeRect(15, 1, 0, 0), screen);
	MDE_DrawEllipse(MDE_MakeRect(20, 1, -1, -1), screen);
	MDE_DrawEllipse(MDE_MakeRect(25, 1, 0, 0), screen);
	MDE_DrawEllipse(MDE_MakeRect(30, 1, 1, 1), screen);
	MDE_DrawEllipse(MDE_MakeRect(35, 1, 2, 2), screen);
	MDE_DrawEllipse(MDE_MakeRect(40, 1, 3, 3), screen);

	MDE_RECT r = { 5, 5, 30, 20 };
	MDE_SetColor(MDE_RGB(0, 255, 0), screen);
	MDE_DrawEllipse(r, screen);
	r.y -= 10;
	r.w = 60;
	MDE_DrawEllipse(r, screen);
	r.x = -20;
	r.y = 30;
	MDE_DrawEllipse(r, screen);

	MDE_RECT r2 = { 5, 15, 10, 35 };
	MDE_SetColor(MDE_RGB(0, 128, 0), screen);
	MDE_DrawEllipseFill(r2, screen);

	Invalidate(MDE_MakeRect(5, 5, 10, 10));

	MoveView::OnPaint(rect, screen);
}

// == TEST 02 ====================================================================

class TestView02 : public MoveView
{
public:
	TestView02(int x, int y, MDE_BUFFER *image);
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
private:
	MDE_BUFFER *image;
};

TestView02::TestView02(int x, int y, MDE_BUFFER *image) : image(image)
{
	SetRect(MDE_MakeRect(x, y, image->w + 2, image->h + 2));
//	SetRect(MDE_MakeRect(x, y, image->w, image->h));
}

void TestView02::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MDE_DrawBuffer(image, MDE_MakeRect(1, 1, image->w, image->h), 0, 0, screen);
//	MDE_DrawBufferStretch(image, MDE_MakeRect(0, 0, m_rect.w, m_rect.h), MDE_MakeRect(0, 0, image->w, image->h), screen);
	MoveView::OnPaint(rect, screen);
}

// == TEST 03 ====================================================================

class TestView03 : public MoveView
{
public:
	TestView03();
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
};

TestView03::TestView03()
{
	SetRect(MDE_MakeRect(100, 300, 350, 220));
}

void TestView03::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MDE_SetColor(col_bg, screen);
	MDE_DrawRectFill(rect, screen);

	MDE_SetColor(MDE_RGB(0, 0, 0), screen);
	MDE_DrawRect(MDE_MakeRect(0, 0, m_rect.w, m_rect.h), screen);
	MDE_DrawEllipse(MDE_MakeRect(0, 0, m_rect.w, m_rect.h), screen);

	int i, x = 10;
	MDE_SetColor(MDE_RGB(255, 0, 0), screen);
	MDE_DrawRectFill(MDE_MakeRect(290, 10, 50, 50), screen);
	MDE_DrawRectFill(MDE_MakeRect(290, 70, 51, 51), screen);
	MDE_SetColor(MDE_RGB(255, 255, 255), screen);
	MDE_DrawEllipse(MDE_MakeRect(290, 10, 50, 50), screen);
	MDE_DrawEllipse(MDE_MakeRect(290, 70, 51, 51), screen);

//	for(i = -5; i < 5; i++)
//		MDE_DrawEllipse(MDE_MakeRect(10 + i * 5, 150, i, i), screen);

	for(i = 0; i < 15; i++)
	{
		MDE_RECT r1 = MDE_MakeRect(x, 10, 10 + i, 50 - i * 3);
		MDE_RECT r2 = r1;
		r2.y += 100;

		MDE_SetColor(MDE_RGB(255, 0, 0), screen);
		MDE_DrawRectFill(r1, screen);
		MDE_DrawRectFill(r2, screen);
		MDE_SetColor(MDE_RGB(255, 255, 255), screen);
		MDE_DrawEllipse(r1, screen);
		MDE_DrawEllipseFill(r2, screen);

		r1.y += 60;
		r2.y += 60;
		r1.h = r1.w;
		r2.h = r2.w;
		MDE_SetColor(MDE_RGB(255, 0, 0), screen);
		MDE_DrawRectFill(r1, screen);
		MDE_DrawRectFill(r2, screen);
		MDE_SetColor(MDE_RGB(255, 255, 255), screen);
		MDE_DrawEllipse(r1, screen);
		MDE_DrawEllipseFill(r2, screen);

		x += r1.w + 1;
	}

	MDE_DrawLine(3, 3, 3, 3, screen);
	MDE_DrawLine(13, 3, 14, 3, screen);
	MDE_DrawLine(23, 3, 23, 4, screen);
	MDE_DrawLine(33, 3, 34, 4, screen);

	MDE_DrawLine(44, 3, 43, 3, screen);
	MDE_DrawLine(53, 4, 53, 3, screen);
	MDE_DrawLine(64, 4, 63, 3, screen);

	MDE_DrawLine(73, 3, 100, 5, screen);

	MoveView::OnPaint(rect, screen);
}

// == TEST 04 ====================================================================

class TestView04 : public MoveView
{
public:
	TestView04();
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
	virtual void OnInvalid();
private:
	MDE_BUFFER *image;
};

TestView04::TestView04() : image(image)
{
	SetRect(MDE_MakeRect(300, 100, 150, 150));
}

void TestView04::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MDE_SetColor(0x00000000, screen);
	MDE_DrawRectFill(rect, screen);

	int pos_on_screen_x = 0, pos_on_screen_y = 0;
	ConvertToScreen(pos_on_screen_x, pos_on_screen_y);

	int i;
	for(i = 0; i < m_region.num_rects; i++)
	{
		MDE_RECT r = m_region.rects[i];
		r.x -= pos_on_screen_x;
		r.y -= pos_on_screen_y;
		MDE_SetColor(i * 32 + (i/2) * 256*64, screen);
		MDE_DrawRectFill(r, screen);
		MDE_SetColor(0xFFFFFFFF, screen);
		MDE_DrawRect(r, screen);
	}

	MoveView::OnPaint(rect, screen);
}

void TestView04::OnInvalid()
{
	Invalidate(MDE_MakeRect(0, 0, m_rect.w, m_rect.h));
}

// == TEST 05 ====================================================================

class TestView05 : public MoveView
{
public:
	TestView05(MDE_BUFFER *image);
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
	virtual bool GetHitStatus(int x, int y);
private:
	MDE_BUFFER *image;
};

TestView05::TestView05(MDE_BUFFER *image) : image(image)
{
	SetRect(MDE_MakeRect(600, 20, image->w, image->h));
	SetTransparent(true);
}

void TestView05::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	image->method = MDE_METHOD_ALPHA;
	MDE_DrawBuffer(image, MDE_MakeRect(0, 0, image->w, image->h), 0, 0, screen);
}

bool TestView05::GetHitStatus(int x, int y)
{
	if (MDE_View::GetHitStatus(x, y))
		return ((unsigned char*)image->data)[x * image->ps + y * image->stride + MDE_COL_OFS_A] == 255;
	return false;
}

// == TEST 06 ====================================================================

class TestView06 : public MoveView
{
public:
	TestView06();
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
	virtual void Pulse();
public:
	int scroll_y;
	float g;
};

TestView06::TestView06() : scroll_y(0), g(0.0f)
{
	SetRect(MDE_MakeRect(650, 150, 100, 100));

	MDE_View *child = new TestView02(10, 10, testimage[3]);
	AddChild(child);

	MDE_View *child2 = new TestView02(10, 300, testimage[3]);
	AddChild(child2);
}

void TestView06::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
#ifdef DEBUG_REPAINT_REGION
	MDE_SetColor(rand() * rand(), screen);
	for(int i = 0; i < 10; i++)
		MDE_DrawLine(rand()%m_rect.w, rand()%m_rect.h, rand()%m_rect.w, rand()%m_rect.h, screen);
//	MDE_RECT r = MDE_MakeRect(10, 10, m_rect.w - 20, m_rect.h - 20);
//	MDE_RECT r = MDE_MakeRect(1, 1, m_rect.w-2, m_rect.h-2);
//	MDE_DrawRectFill(r, screen);
	// FIX: ScrollRect doesn't work from inside a OnPaint.
//	ScrollRect(r, 0, -5);

#else
	for(int y = 0; y < 1000; y+= testimage[0]->h)
		MDE_DrawBuffer(testimage[0], MDE_MakeRect(0, y - scroll_y, testimage[0]->w, testimage[0]->h), 0, 0, screen);
#endif

//	MoveView::OnPaint(rect, screen);
}

void TestView06::Pulse()
{
	int new_scroll_y = scroll_y;
	new_scroll_y = (int) (sin(g) * 400 + 500);
	g += 0.02f;

	int dx = 0;
	int dy = scroll_y - new_scroll_y;
	scroll_y = new_scroll_y;

	//Validate(false); // FIX: test both true and false
//	MDE_RECT r = MDE_MakeRect(1, 1, m_rect.w - 2, m_rect.h - 2);
	MDE_RECT r = MDE_MakeRect(0, 0, m_rect.w, m_rect.h);

	ScrollRect(r, dx, dy, true);

	Validate(true);
}

// == TEST 07 ====================================================================

class TestView07 : public MoveView
{
public:
	TestView07(MDE_BUFFER *image);
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
	virtual void Pulse();
private:
	MoveView *scrollview;
};

TestView07::TestView07(MDE_BUFFER *image)
{
	MDE_View *child = new TestView02(10, 10, image);
	AddChild(child);

	MDE_View *child2 = new TestView02(10, 10, image);
	child->AddChild(child2);

#ifdef DEBUG_REPAINT_REGION
	child = new TestView01();
	child->SetRect(MDE_MakeRect(100, 10, 50, 50));
	AddChild(child);
#endif

	scrollview = new TestView06();
	scrollview->SetRect(MDE_MakeRect(160, 10, 50, 50));
	AddChild(scrollview);

	child = new TestView04();
	child->SetRect(MDE_MakeRect(30, 100, 150, 150));
	AddChild(child);

	child = new TestView05(testimage[1]);
	child->SetRect(MDE_MakeRect(215, 5, testimage[1]->w, testimage[1]->h));
	AddChild(child);

	child = new TestView02(10, 100, image);
	AddChild(child);

	child2 = new TestView02(10, 10, image);
	child->AddChild(child2);

	SetRect(MDE_MakeRect(250, 150, 300, 300));
}

void TestView07::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MDE_SetColor(col_bg, screen);
	MDE_DrawRectFill(rect, screen);
	MoveView::OnPaint(rect, screen);
}

void TestView07::Pulse()
{
	scrollview->Pulse();
}

// == TEST 08 ====================================================================

class TestView08 : public MoveView
{
public:
	TestView08(MDE_BUFFER *image);
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
	virtual void Pulse();
};

TestView08::TestView08(MDE_BUFFER *image)
{
	SetRect(MDE_MakeRect(20, 150, 200, 200));
}

void TestView08::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MoveView::OnPaint(rect, screen);

	MDE_SetClipRect(MDE_MakeRect(1, 1, m_rect.w - 2, m_rect.h - 2), screen);

	MDE_SetColor(col_bg, screen);
	MDE_DrawRectFill(rect, screen);

	int i;
	#define TESTRAND ((rand()%400)-200)
	for(i = 0; i < 10; i++)
	{
		MDE_SetColor(rand() * rand(), screen);
		MDE_DrawRect(MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), screen);
		MDE_DrawRectFill(MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), screen);
		MDE_DrawEllipse(MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), screen);
		MDE_DrawEllipseFill(MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), screen);
		MDE_DrawRectInvert(MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), screen);
		MDE_DrawLine(TESTRAND, TESTRAND, TESTRAND, TESTRAND, screen);
		MDE_DrawBuffer(testimage[3], MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), TESTRAND, TESTRAND, screen);
		MDE_DrawBufferStretch(testimage[3], MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND),
											MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), screen);
	}

/*	MDE_SetColor(rand() * rand(), screen);

	#define TESTRAND ((rand()%400)-100+5)

	for(int i = 0; i < 10; i++)
	{
		MDE_RECT r = MDE_MakeRect(5, 5 + i * 15, m_rect.w - 10, 10);
		MDE_SetClipRect(r, screen);
		switch(i)
		{
		case 0: MDE_DrawRect(MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), screen); break;
		case 1: MDE_DrawRectFill(MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), screen); break;
		case 2: MDE_DrawEllipse(MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), screen); break;
		case 3: MDE_DrawEllipseFill(MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), screen); break;
		case 4: MDE_DrawRectInvert(MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), screen); break;
		case 5: MDE_DrawLine(TESTRAND, TESTRAND, TESTRAND, TESTRAND, screen); break;
		case 6: MDE_DrawBuffer(testimage[3], MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), TESTRAND, TESTRAND, screen); break;
		case 7: MDE_DrawBufferStretch(testimage[3], MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND),
											MDE_MakeRect(TESTRAND, TESTRAND, TESTRAND, TESTRAND), screen); break;
		}
	}*/
}

void TestView08::Pulse()
{
	Invalidate(MDE_MakeRect(0, 0, m_rect.w, m_rect.h));
}

#ifdef MDE_SUPPORT_ROTATE

// == TEST 09 ====================================================================

#define PI 3.141592653589793

class TestView09 : public MoveView
{
public:
	TestView09(int x, int y, MDE_BUFFER *image);
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
	virtual void OnMouseMove(int x, int y);
	virtual bool OnMouseWheel(int delta, bool vertical);
	virtual void Pulse();
private:
	MDE_BUFFER *image;
	double angle;
	double angle_smooth;
	double scale;
};

TestView09::TestView09(int x, int y, MDE_BUFFER *image) : image(image), angle(0), angle_smooth(0), scale(1.0)
{
	SetRect(MDE_MakeRect(x, y, image->w, image->h));
//	SetRect(MDE_MakeRect(x, y, image->w*4, image->h*4));
}

void TestView09::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MDE_BUFFER *tmpbuf = MDE_CreateBuffer(image->w, image->h, image->format, 0);
	MDE_DrawBuffer(image, MDE_MakeRect(0, 0, image->w, image->h), 0, 0, tmpbuf);

	MDE_RotateBuffer(tmpbuf, -angle_smooth, scale, MDE_RGBA(0, 0, 0, 0), true);
	MDE_DrawBuffer(tmpbuf, rect, rect.x, rect.y, screen);
//	MDE_DrawBufferStretch(tmpbuf, MDE_MakeRect(0, 0, m_rect.w, m_rect.h), MDE_MakeRect(0, 0, image->w, image->h), screen);

	MDE_DeleteBuffer(tmpbuf);
}

void TestView09::OnMouseMove(int x, int y)
{
	MoveView::OnMouseMove(x, y);
	angle = -PI + PI * 2 * x / image->h;
}

bool TestView09::OnMouseWheel(int delta, bool vertical)
{
	scale += delta / 1000.0;
	return true;
}

void TestView09::Pulse()
{
	angle_smooth += double(angle - angle_smooth) / 10;
	Invalidate(MDE_MakeRect(0, 0, m_rect.w, m_rect.h));
}

#endif // MDE_SUPPORT_ROTATE

// == AlphaRotView ====================================================================

// Some horrible ugly old code taken from a old demoeffect. please ignore ;-)

class AlphaRotView : public MoveView
{
public:
	AlphaRotView(MDE_BUFFER *image, unsigned int bg_col);
	virtual void Pulse();
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
private:
	MDE_BUFFER *image;
	unsigned int bg_col;

	#define NUM 100
	float g1,g2,g3;
	float cosa,sina,temp;
	int i,j,jj;
	int order[NUM];
	float x[NUM], y[NUM], z[NUM];
	float px[NUM], py[NUM], pz[NUM];
	float animator;
};

AlphaRotView::AlphaRotView(MDE_BUFFER *image, unsigned int bg_col) : image(image), bg_col(bg_col)
{
	SetRect(MDE_MakeRect(500, 300, 250, 250));

	g1 = g2 = g3 = 0;
	animator = 0;
	Pulse();
}

void AlphaRotView::Pulse()
{
	animator += 0.01f;
	j=-50;
	float k = 0;
	for(i=0;i<NUM;i++)
	{
		x[i] = (float) sin(k)*90;
		y[i] = (float) cos(k)*90;
		z[i] = (float) sin(k/10.4f)*70;
		k++;
	}

	float speed = 0.5;
	g1+=0.07f*speed;
	g2+=0.05f*speed;
	g3+=0.0336f*speed;

	for(i=0;i<NUM;i++)
	{
		// 3D rotation
		cosa=(float) cos(g1);
		sina=(float) sin(g1);
		px[i]=x[i]*cosa-y[i]*sina;
		py[i]=x[i]*sina+y[i]*cosa;
		pz[i]=z[i];
		
		cosa=(float) cos(g2);
		sina=(float) sin(g2);
		temp=py[i]*cosa+pz[i]*sina;
		pz[i]=py[i]*sina-pz[i]*cosa;
		py[i]=temp;
		
		cosa=(float) cos(g3);
		sina=(float) sin(g3);
		temp=px[i]*cosa+pz[i]*sina;
		pz[i]=-px[i]*sina+pz[i]*cosa;
		px[i]=temp;

		// 3D -> 2D projection
		px[i]=(px[i])/((pz[i])/150+1)+m_rect.w/2;
		py[i]=(py[i])/((pz[i])/150+1)+m_rect.h/2;
	}
	for(i=0;i<NUM;i++)
		order[i]=i;
	for(i=0;i<NUM;i++)
		for(j=0;j<NUM - 1;j++)
		{
			if (pz[order[j]] < pz[order[j+1]])
			{
				int tmp = order[j];
				order[j] = order[j+1];
				order[j+1] = tmp;
			}
		}
	Invalidate(MDE_MakeRect(0, 0, m_rect.w, m_rect.h));
}

void AlphaRotView::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MDE_SetColor(bg_col, screen);
	MDE_DrawRectFill(MDE_MakeRect(0, 0, m_rect.w, m_rect.h), screen);
	image->method = MDE_METHOD_COLOR;

	for(i=0;i<NUM;i++)
	{
		float size = pz[order[i]] / 100.0f;
		size = MDE_MIN(1.0f, size);
		size = MDE_MAX(0.0f, size);
		size = 1 - size;
		if (pz[order[i]] > -150)
		{
//			int w = (int) image->w * size;
//			int h = (int) image->h * size;
			int w = (int) (image->w * 30.0f/(40.0f + pz[order[i]]/6.0f));
			int h = (int) (image->h * 30.0f/(40.0f + pz[order[i]]/6.0f));
			int x = (int) px[order[i]] - w/2;
			int y = (int) py[order[i]] - h/2;
			if (w > 0 && h > 0)
			{
				int r = (int) (255 -(pz[order[i]] + 140) / 5.0f);
				int g = (int) (255 -(pz[order[i]] + 140) / 1.4f);
				int b = (int) (255 -(pz[order[i]] + 140));
				MDE_SetColor(MDE_RGB(r, g, b), image);
				MDE_DrawBufferStretch(image, MDE_MakeRect(x, y, w, h), MDE_MakeRect(0, 0, image->w, image->h), screen);
			}
		}
	}
	MoveView::OnPaint(rect, screen);
}

// ===============================================================================

void InitTest(MDE_Screen *screen)
{
	int i, x, y;
	MDE_RECT r1 = {5, 5, 5, 5};
	MDE_RECT r2 = {5, 10, 5, 5};
	MDE_RECT r3 = {0, 0, 5, 5};
	MDE_RECT r4 = {5, 0, 5, 5};
	MDE_RECT r5 = {0, 5, 5, 5};
	MDE_RECT r6 = {10, 5, 5, 5};
	MDE_ASSERT(!MDE_RectIntersects(r1, r2));
	MDE_ASSERT(!MDE_RectIntersects(r1, r3));
	MDE_ASSERT(!MDE_RectIntersects(r1, r4));
	MDE_ASSERT(!MDE_RectIntersects(r1, r5));
	MDE_ASSERT(!MDE_RectIntersects(r1, r6));
	MDE_ASSERT(!MDE_RectContains(r1, 7, 4));
	MDE_ASSERT(!MDE_RectContains(r1, 4, 7));
	MDE_ASSERT(!MDE_RectContains(r1, 10, 7));
	MDE_ASSERT(!MDE_RectContains(r1, 7, 10));

	MDE_RECT r7 = {0, 0, 100, 100};
	MDE_RECT r8 = {10, 10, 10, 0};
	MDE_ASSERT(!MDE_RectIntersects(r7, r8));

	testimage[0] = LoadTGA("rose.tga");
	testimage[1] = LoadTGA("cube.tga");
	testimage[4] = LoadTGA("biohazard.tga");
	testimage[5] = LoadTGA("emil.tga");
	testimage[6] = LoadTGA("test.tga");
	testimage[1]->method = MDE_METHOD_ALPHA;
	testimage[4]->method = MDE_METHOD_ALPHA;

	MDE_DrawBuffer(testimage[1], MDE_MakeRect(10, 10, testimage[1]->w, testimage[1]->h), 0, 0, testimage[4]);
	MDE_DrawBuffer(testimage[1], MDE_MakeRect(40, 20, testimage[1]->w, testimage[1]->h), 0, 0, testimage[4]);

	testimage[3] = MDE_CreateBuffer(64, 64, MDE_FORMAT_INDEX8, 256);
	for(i = 0; i < 256; i++)
	{
		testimage[3]->pal[i * 3] = (unsigned char) i;
		testimage[3]->pal[i * 3 + 1] = (unsigned char) i;
		testimage[3]->pal[i * 3 + 2] = (unsigned char) 0;
	}
	for(i = 0; i < testimage[3]->w * testimage[3]->h; i++)
	{
		((char*)testimage[3]->data)[i] = (unsigned char) (i * 256 / testimage[3]->w);
	}
	MDE_SetColor(MDE_RGB(111, 0, 96), testimage[3]);
	testimage[3]->method = MDE_METHOD_COPY;

	// Create a 24bit bitmap
	testimage[2] = MDE_CreateBuffer(64, 64, MDE_FORMAT_BGR24, 0);
	unsigned char *data8 = (unsigned char *) testimage[2]->data;
	for(y = 0; y < 64; y++)
		for(x = 0; x < 64; x++)
		{
			data8[0] = x * 4;
			data8[1] = y * 4;
			data8[2] = (x+y)*2;
			data8 += 3;
		}


	MDE_View *tmp;

	screen->AddChild(new TestView05(testimage[4]));
	screen->AddChild(new TestView02(500, 50, testimage[2]));
	screen->AddChild(new TestView02(500, 200, testimage[0]));		//
	screen->AddChild(new TestView03());							//
	screen->AddChild(new TestView04());
	screen->AddChild(new TestView06());

	tmp = new TestView07(testimage[3]);
	screen->AddChild(tmp);
	tmp->SetZ(MDE_Z_TOP);

	screen->AddChild(new TestView08(testimage[3]));
#ifdef MDE_SUPPORT_ROTATE
	screen->AddChild(new TestView09(20, 35, testimage[5]));
	screen->AddChild(new TestView09(130, 35, testimage[6]));
#endif
	screen->AddChild(new AlphaRotView(testimage[1], MDE_RGB(32, 32, 32)));

	tmp = new TestView05(testimage[4]);
	tmp->SetRect(MDE_MakeRect(510, 330, tmp->m_rect.w, tmp->m_rect.h));
	screen->AddChild(tmp);
	tmp->SetZ(MDE_Z_TOP);

	screen->AddChild(new BlurView());

	screen->AddChild(menuview = new MenuView());
}

void ShutdownTest()
{
	int i;
	for(i = 0; i < 7; i++)
		MDE_DeleteBuffer(testimage[i]);
}

void PulseTest(MDE_Screen *screen)
{
	MoveView *view = (MoveView *) screen->m_first_child;
	while(view)
	{
		view->Pulse();
		view = (MoveView *) view->m_next;
	}
}
