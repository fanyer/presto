#if !defined(MACOPPRINTERCONTROLLER_H)
#define MACOPPRINTERCONTROLLER_H

#include "modules/pi/OpPrinterController.h"
#include "modules/hardcore/base/op_types.h"

class MacVegaPrinterListener;
class CMacSpecificActions;
class Window;

class MacOpPrinterController : public OpPrinterController
{
public:
	MacOpPrinterController();

	~MacOpPrinterController();

	virtual OP_STATUS Init(const OpWindow * opwindow);

	virtual void SetPrintListener(OpPrintListener *print_listener) { m_printListener = print_listener; }

	virtual const OpPainter *GetPainter();

	virtual OP_STATUS BeginPage();
	virtual OP_STATUS EndPage();
	
	virtual void PrintDocFormatted(PrinterInfo *printer_info);
	virtual void PrintPagePrinted(PrinterInfo *printer_info);
	virtual void PrintDocFinished();
	virtual void PrintDocAborted();

	virtual void GetSize(UINT32 *w, UINT32 *h);
	virtual void GetOffsets(UINT32 *w, UINT32 *h, UINT32 *right, UINT32 *bottom);

	virtual void GetDPI(UINT32* x, UINT32* y);

	virtual void SetScale(UINT32 scale);

	virtual UINT32 GetScale() { return m_scale; }

	virtual BOOL IsMonochrome();

	void StartSession();
	void EndSession();

	static void InitStatic(void);
	static void FreeStatic(void);

	PMPrintSession GetPrintSession() {return s_printSession;}
	PMPrintSettings GetPrintSettings() {return s_printSettings;}
	PMPageFormat GetPageFormat() {return s_pageFormat;}
	
	static PMPageFormat s_pageFormat;
	static PMPrintSession s_printSession;
	static PMPrintSettings s_printSettings;
	static int s_refCount;

	static int GetSelectedStartPage();
	static int GetSelectedEndPage();

	static void PrintWindow(Window *win);

	static Boolean GetPrintDialog(WindowRef sheetParentWindow = NULL, const uni_char* jobTitle = NULL);

	static int IsPrinting() {return m_printing;}

private:

	friend class CMacSpecificActions;
	OpPrintListener *m_printListener;
	OpPainter* m_painter;
	OpBitmap* m_bitmap;
	MacVegaPrinterListener* m_printer_listener;
	
	Rect m_pageRect;
	UINT32 m_scale;
	double m_pageFormatScale;
	UINT32 m_paperWidth;
	UINT32 m_paperHeight;

	static bool m_printing;
	bool m_pageOpen;
};

#endif
