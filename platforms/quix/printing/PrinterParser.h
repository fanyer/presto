// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#ifndef __PRINTER_PARSER_H__
#define __PRINTER_PARSER_H__

struct PrinterListEntry
{
	OpString name;
	OpString host;
	OpString comment;
};

namespace PrinterParser
{
	void Parse(OpVector<PrinterListEntry>& printerList, int &current_item);
	void AddPrinter(OpVector<PrinterListEntry>& printerList,
					const OpStringC &name,
					const OpStringC &host,
					const OpStringC &comment);
	
	void ParsePrintcapEntry(OpVector<PrinterListEntry>& printerList, 
							const OpStringC& entry);
	
	void ParseStdPrintcap(OpVector<PrinterListEntry>& printerList,  
						  const OpStringC& filepath);
	
	void ParseStdPrintcapMultiline(OpVector<PrinterListEntry>& printerList, 
								   const OpStringC& filepath);
	
	void ParseCUPS(OpVector<PrinterListEntry>& printerList, 
						  OpString8& etcLpDefault);
	
	void ParseEtcLpPrinters(OpVector<PrinterListEntry>& printerList, 
							const OpStringC& path);

	void ParsePrintersConf(OpVector<PrinterListEntry>& printerList, 
						   OpString8& defaultPrinter,
						   const OpStringC& filePath);

};

#endif
