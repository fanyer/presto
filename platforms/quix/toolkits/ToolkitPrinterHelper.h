/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef TOOLKIT_PRINTER_HELPER_H
#define TOOLKIT_PRINTER_HELPER_H

#include <stdlib.h>

struct DestinationInfo
{
	struct PaperItem
	{
		PaperItem() { name = 0; width = length = 0; }
		~PaperItem() { free(name); }

		char* name;
		int   width;
		int   length;
	};

	DestinationInfo()
	{
		name = info = location = media = 0;
		copies = 1;
		is_default = false;
		paperlist = 0;
		paperlist_size = 0;
		default_paper = 0;
	}

	~DestinationInfo()
	{
		free(name);
		free(info);
		free(location);
		free(media);

		for (int i=0; i<paperlist_size; i++)
			free(paperlist[i]);
		free(paperlist);
	};


	char* name;
	char* info;
	char* location;
	char* media;
	PaperItem** paperlist;
	int paperlist_size;
	int default_paper;
	int copies;
	bool is_default;
};

class ToolkitPrinterHelper
{
public:
	virtual ~ToolkitPrinterHelper() {}

	/**
	 * Informs if there if information available and access to 
	 * to an underlying printer interface. If false is returned
	 * a printer dialog can normally only print to disk
	 *
	 * A printer implementation may decide not to show a dialog at
	 * all if this function returns false. A notifcation should then
	 * be shown in its place
	 *
	 * @return the state
	 */
	virtual bool HasConnection() = 0;
	
	/** Set the name of the destination printer
	  * @param printername Destination printer to use
	  * @return true on success or false on failure
	  */
	virtual bool SetPrinter(const char* printername) = 0;

	/** Set the number of copies to print
	  * @param copies Number of copies
	  */
	virtual void SetCopies(int copies) = 0;

	/** Print a file
	  * @param filename Postscript file to print
	  * @param jobname Name for this job
	  * @return true on success or false on failure
	  */
	virtual bool PrintFile(const char* filename, const char* jobname) = 0;

	/**
	 * Allocates and returns a pointer to a array of 
	 * valid printer destinations. Ovnership of returned 
	 * data is transferred to the caller. Use @ref 
	 * FreeDestinations() to release data.
	 *
	 * @param info Printer data on return. The last element will
	 *        always be 0
	 *
	 * @return true on success, otherwise false.
	 */
	virtual bool GetDestinations(DestinationInfo**& info) = 0;

	/**
	 * Release data that was allocated by @ref GetDestinations
	 *
	 * @param info Printer data 
	 */
	virtual void ReleaseDestinations(DestinationInfo**& info) = 0;

	/**
	 * Get available paper sizes for the specified destination
	 * The function will populate 'paperlist', 'paperlist_size'
	 * and 'default_paper'. The list and size can be 0 on a
	 * successful return if no paper sizes can be determined
	 *
	 * @param info Destination data. The instance must
	 *        be one of those created by @ref GetDestinations
	 *        prior to calling this function
	 *
	 * @return true on success, otherwise false.
	 */
	virtual bool GetPaperSizes(DestinationInfo& info) = 0;
};

#endif // TOOLKIT_PRINTER_HELPER_H
