/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** This file describe a class used to asynchronously export a cache storage
**
** Luca Venturi
**
*/
#ifndef CACHE_EXPORTER_H
#define CACHE_EXPORTER_H

#ifdef PI_ASYNC_FILE_OP

#include "modules/hardcore/mh/messobj.h"
#include "modules/util/opfile/opfile.h"

/// Listener class used to signal the progress during a (tentatively) asynchronous export
class AsyncExporterListener
{
public:
	virtual ~AsyncExporterListener() {}

	/**
		 Signal that the export has started
		 @param bytes_saved bytes saved on disk
		 @param length total length of the file
		 @param rep URL_Rep saved
		 @param name Name of the target file
		 @param user param parameter supplied the the method asking the asynchronous export
	*/
	virtual void NotifyStart(OpFileLength length, URL_Rep *rep, const OpStringC &name, UINT32 param) = 0 ;

	/**
		 Signal the current progress of the export
		 @param bytes_saved bytes saved on disk
		 @param length total length of the file
		 @param rep URL_Rep saved
		 @param name Name of the target file
		 @param user param parameter supplied the the method asking the asynchronous export
	 */
	virtual void NotifyProgress(OpFileLength bytes_saved, OpFileLength length, URL_Rep *rep, const OpStringC &name, UINT32 param) = 0;

	/**
		Signal that the operation is finished (with or without errors). If there are no errors, NotifyProgress() will also be called.
		This method will always be called, unless ExportAsFileAsync() return an error (which means that the exporter has not been created)

		@param ops status; if not sucessful, the operation failed
		@param bytes_saved bytes saved on disk
		@param length total length of the file
		@param rep URL_Rep saved
		@param name Name of the target file
	 */
	virtual void NotifyEnd(OP_STATUS ops, OpFileLength bytes_saved, OpFileLength length, URL_Rep *rep, const OpStringC &name, UINT32 param) = 0;
};

/** Class able to export a cache file asynchronously; this class is intended to be used by ExportAsFileAsync().
 *
 * The object will kill itself when appropriate
 * */
class CacheAsyncExporter: public MessageObject
{
private:
	friend class Cache_Storage;

	/// URL Rep with the content
	URL_Rep *url_rep;
	/// Current position
	OpFileLength pos;
	/// Length of the file
	OpFileLength len;
	/// "Safe" export enabled (this is not really asynchronous)
	BOOL safe;
	/// File name
	OpString name;
	/// User parameter
	UINT32 user_param;
	/// Delete the target file if an error occour
	BOOL delete_on_error;
	/// Listener notified by the progress
	AsyncExporterListener *listener;
	/// File that needs to be read
	OpFileDescriptor *input;
	/// File that will contain the copy of the cache
	OpFile *output;
	/// Buffer used to export the file
	UINT8 *buf;

	/// Notify the error to the listener, then delete the "this" pointer
	void NotifyEndAndDeleteThis(OP_STATUS ops);

protected:
	/** Constructor
	 *
	 * @param progress_listener Listener that will receive the progress informations
	 * @param rep URL_Rep to export
	 * @param safe_mode TRUE if the export has to be done in a "safge" way (synchronous)
	 * @param param user defined parameter that will be passed to the listener, to possibly simplify the UI code
	 * @param delete_if_error if TRUE, in case of error the target file will be deleted
	 * */
	CacheAsyncExporter(AsyncExporterListener *progress_listener, URL_Rep *rep, BOOL safe_mode, BOOL delete_if_error, UINT32 param=0);

	/// Destructor
	virtual ~CacheAsyncExporter();

	/**
	  Start the export. If SafeMode is used, URL::SaveAsFile() will be used, which result basically on a synchronous call,
	  Even if executed in an asynchronous way (e.g., it could hang the phone for a while).
	  In case of error, this object needs to be deleted from the caller method. In case of success, it will delete itself when the export will be
	  finished or an error will occur.
	  @param file_name name of the exported file
	*/
	OP_STATUS StartExport(const OpStringC &file_name);

public:
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
};
#endif // PI_ASYNC_FILE_OP

#endif // CACHE_EXPORTER_H
