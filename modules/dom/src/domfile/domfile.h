/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_FILE_H
#define DOM_FILE_H

#include "modules/dom/src/domfile/domblob.h"

class DOM_FileReader;
class DOM_FileReaderSync;
class Upload_Handler;

/**
 * The File interface in the W3C File API.
 */
class DOM_File
	: public DOM_Blob
{
private:
	DOM_File()
		: DOM_Blob(BlobFile)
		, m_file(NULL)
		, m_delete_on_destruct(FALSE)
		, m_sliced_from(NULL)
	{
	}

	const uni_char *m_file;

	BOOL m_delete_on_destruct;
	/**< If TRUE, then delete the underlying file on destruct. */

	DOM_File *m_sliced_from;
	/**< If a slice, what it was sliced from. */

	OpString m_content_type;

	OP_STATUS GetFilename(TempBuffer *buf);

	friend class DOM_File_PersistentItem;
	friend class DOM_FileReadView;

public:
	static OP_STATUS Make(DOM_File *&file, const uni_char *path, BOOL disguise_as_blob, BOOL delete_on_destruct, DOM_Runtime *runtime);
	/**< @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static OP_STATUS Make(DOM_File *&file, DOM_File *source, BOOL disguise_as_blob, DOM_Runtime *runtime);
	/**< @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual ~DOM_File();

	virtual OP_STATUS MakeSlice(DOM_Blob *&sliced_blob, double slice_start, double slice_end);
	/**< @returns OpStatus::OK, OpStatus::ERR_NO_MEMORY or
	              OpStatus::ERR (for slices causing arithmetic overflows). */

	virtual OP_STATUS MakeReadView(DOM_BlobReadView *&blob_view, OpFileLength start_position, OpFileLength view_length);
	/**< @returns OpStatus::OK, OpStatus::ERR_NO_MEMORY on OOM. */

	/* see base class documentation. */
	virtual OP_STATUS MakeClone(ES_Runtime *target_runtime, DOM_Blob *&target_object);

#ifdef ES_PERSISTENT_SUPPORT
	/* see base class documentation. */
	virtual OP_STATUS MakeClone(ES_PersistentItem *&target_item);

	/* see base class documentation. */
	static OP_STATUS MakeClone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_Blob *&target_object);
#endif // ES_PERSISTENT_SUPPORT

	virtual const uni_char *GetContentType();
	/**< @returns the Content Type if known, otherwise NULL. */

	virtual double GetSize();
	/**< @returns the size or 0 if not known. */

	const uni_char* GetPath() { return m_file; }
	/**< @returns file's full path. */

	const uni_char *GetFilePath();
	/**< @returns the correct file path if this is a slice. */

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_FILE || DOM_Blob::IsA(type); }
	virtual void GCTrace();

	OP_STATUS GetFilename(OpString &result);
	/**< @returns the filename of the underlying file. */

#ifdef _FILE_UPLOAD_SUPPORT_
	OP_STATUS Upload(Upload_Handler *&result, const uni_char *name, const uni_char *filename, const char *mime_type, BOOL as_standalone);

	void UploadL(Upload_Handler *&result, const uni_char *name, const uni_char *filename, const char *mime_type, BOOL as_standalone);
#endif // _FILE_UPLOAD_SUPPORT_

	/* 16k + 2 to make it a multiple of three (allows whole buffer base64 encoding.) */
	enum { DOM_FILE_READ_BUFFER_SIZE = 16386 };

	friend class DOM_FileReader;
	friend class DOM_FileReaderSync;
};

/** Internal class for representing an ongoing
    read of a DOM_File object. */
class DOM_FileReadView
	: public DOM_BlobReadView
{
public:
	static OP_STATUS Make(DOM_FileReadView *&read_view, DOM_File *file, OpFileLength start_position);

	virtual ~DOM_FileReadView();

	/** See base class documentation. */
	virtual BOOL AtEOF();

	/** See base class documentation. */
	virtual OP_STATUS ReadAvailable(DOM_FileReader_Base *reader, const unsigned char *bytes, unsigned length, OpFileLength *bytes_read);

	virtual void GCTrace()
	{
	}

private:
	DOM_FileReadView()
		: length_remaining(-1.0)
	{
	}

	OpFile file;

	double length_remaining;
	/**< Number of bytes left to read, or -1.0 if to the (unknown) end. */

	friend class DOM_File;
};

#endif // DOM_FILE_H
