/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Karianne Ekern (karie)
*/

#ifndef PRINTER_ADAPTER_H
#define PRINTER_ADAPTER_H

#ifdef _PRINT_SUPPORT_

#include "modules/util/opstring.h"
#include "modules/pi/OpPrinterController.h"

class PrinterAdapter
{
 public:

	PrinterAdapter() {}
	virtual ~PrinterAdapter(){}

	enum ColorMode 
	{ 
		Color, 
		GrayScale 
	};

	enum PrintRange 
	{ 
		AllPages, 
		Selection, 
		PageRange 
	};

	enum PageOrder 
	{ 
		FirstPageFirst, 
		LastPageFirst 
	};

	enum Orientation 
	{ 
		Portrait, 
		Landscape 
	};

	enum PageSize
	{
		A0,
		A1,
		A2,
		A3,
		A4,
		A5,
		A6,
		A7,
		A8,
		A9,
		B0,
		B1,
		B10,
		B2,
		B3,
		B4,
		B5,
		B6,
		B7,
		B8,
		B9,
		C5E,
		Comm10E,
		DLE,
		Executive,
		Folio,
		Ledger,
		Legal,
		Letter,
		Tabloid,
		Custom,
	};

	virtual void Init() = 0; // temporary
	virtual void NewPage() = 0;

	// Accessors
	virtual int GetFromPage() const = 0;
	virtual int GetToPage() const = 0;
	virtual void GetOutputFileName(OpString& filename) const = 0;
	virtual PrinterAdapter::PrintRange GetPrintRange() const = 0;
	virtual int GetMaxPage() const = 0;
	virtual int GetMinPage() const = 0;
	virtual PrinterAdapter::PageOrder GetPageOrder() const = 0;
	virtual int GetNumCopies() const = 0;
	virtual PrinterAdapter::ColorMode GetColorMode() const = 0;
	virtual PrinterAdapter::Orientation GetOrientation () const = 0;
	virtual PrinterAdapter::PageSize GetPageSize() const = 0;
	virtual void GetPrinterName(OpString& printer_name) const = 0;
	virtual void GetPrintProgram(OpString& print_program) const = 0;
	virtual bool GetOutputToFile() const = 0;
	virtual int GetWidth () const = 0;
	virtual int GetHeight () const = 0;
	virtual int GetLogicalDpiX () const = 0;
	virtual int GetLogicalDpiY () const = 0;

  
	// Manipulators
	virtual void SetFromTo(int from, int to) = 0;
	virtual void SetMinMax(int min_page, int max_page) = 0;
	virtual void SetPageSize(PrinterAdapter::PageSize newPageSize) = 0;
	virtual void SetNumCopies(int numCopies) = 0;
	virtual void SetColorMode(PrinterAdapter::ColorMode newColorMode) = 0;
	virtual void SetPrintProgram(const OpStringC& printProg) = 0;
	virtual void SetOutputFileName(const OpStringC& fileName) = 0;
	virtual void SetPrinterName(const OpStringC& name) = 0;
	virtual void SetPrintRange(PrinterAdapter::PrintRange range) = 0;
	virtual void SetOrientation(Orientation orientation) = 0;
	virtual void SetPageOrder(PageOrder pageOrder) = 0;
	virtual void SetOutputToFile(bool enable) = 0;
	virtual void SetPrinterSelectionOption(const OpStringC& option) = 0;
  
	virtual bool OutputToFile () const = 0;

};

#endif // _PRINT_SUPPORT_

#endif // PRINTER_ADAPTER_H
