// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2012 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
//

#ifndef LOADICONS_PI_H
#define LOADICONS_PI_H

#include "modules/util/adt/opvector.h"

class TransferItemContainer;

/***********************************************************************************
 ** This class is an abstract interface for platforms where getting the icon
 ** associated with a file on disk is expensive.  The platform may choose to 
 ** implement this class using threading. If no separate implementation is provided,
 ** the code will run on the main thread but only get a single icon every 100ms.
 ** This code is used by the transfer manager.
 ***********************************************************************************/
class OpAsyncFileBitmapLoader
{
public:
	/*
	* Listener for status messages during loading of icons
	*/
	class OpAsyncFileBitmapHandlerListener
	{
	public:
		/*
		* Called on the listener for each bitmap that has been loaded for
		* the given transfer item.  
		* 
		* @param item The transfer item the image is associated with
		*/
		virtual void OnBitmapLoaded(TransferItemContainer *item) = 0;

		/*
		* Called on the listener after all bitmaps have been loaded.
		* 
		*/
		virtual void OnBitmapLoadingDone() = 0;
	};

	static OP_STATUS Create(OpAsyncFileBitmapLoader** new_asyncfilebitmaploader);

	virtual ~OpAsyncFileBitmapLoader() {}

	/*
	* Initialize the bitmap loader
	* 
	* @param listener The listener that will receive progress updates
	*/
	virtual OP_STATUS Init(OpAsyncFileBitmapHandlerListener *listener) = 0;

	/*
	* Start the process of fetching icons for the transfer items. 
	* 
	* @param transferitems The transfer items to fetch icons for. 
	* These items are are ONLY valid for the duration of this call
	* so the implementation MUST copy the relevant information and
	* use that.
	*/
	virtual void Start(OpVector<TransferItemContainer>& transferitems) = 0;
};

#endif // LOADICONS_PI_H
