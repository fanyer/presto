/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_FILEREADER_H
#define DOM_FILEREADER_H

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domruntime.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domfile/domfileerror.h"

#include "modules/util/bytearray.h"

class OpFile;
class DOM_File;
class ES_Thread;
class DOM_Blob;
class DOM_BlobReadView;

// The File API says to not send progress events more often than every 50ms
#define DOM_FILE_READ_PROGRESS_EVENT_INTERVAL 50

class DOM_FileReader_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_FileReader_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::FILEREADER_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

/** Functionality used both by DOM_FileReader and DOM_FileReaderSync. */
class DOM_FileReader_Base
{
public:
	enum OutputType
	{
		NONE,
		ARRAY_BUFFER,
		BINARY_STRING,
		TEXT,
		DATA_URL
	};

	OP_STATUS AppendToResult(const unsigned char *prefix_data, unsigned prefix_length, const unsigned char *data, unsigned data_length);

	/** Read back data that was never really consumed last AppendToResult().
	    Never more than a few bytes (like 2 or 3). */
	unsigned ReadOldData(unsigned char *buf);

	/** Control if progress events should be sent while reading. */
	void SetWithProgressEvents(BOOL flag) { m_quiet_read = !flag; }

protected:
	DOM_FileReader_Base()
		: m_text_result_decoder(NULL),
		  m_result_pending_len(0),
		  m_data_url_content_type(NULL),
		  m_current_output_type(NONE),
		  m_current_read_view(NULL),
		  m_quiet_read(FALSE),
		  m_array_buffer(NULL)
	{
	}

	virtual ~DOM_FileReader_Base();

	ByteArray m_binary_result;
	TempBuffer m_result;

	InputConverter* m_text_result_decoder;

	unsigned char m_result_pending[6]; // ARRAY OK bratell 2011-02-22
	unsigned m_result_pending_len;

	uni_char *m_data_url_content_type;
	BOOL m_data_url_use_base64;

	OutputType m_current_output_type;
	DOM_BlobReadView *m_current_read_view;

	BOOL m_quiet_read;

	OpString8 m_text_result_charset;

	ES_Object *m_array_buffer;

	OP_STATUS ReadFromBlob(OpFileLength *bytes_read);

	OP_STATUS InitResult(OutputType output_type, int argc, ES_Value* argv, DOM_Blob* blob);
	OP_STATUS FlushToResult();
	void EmptyResult();
	OP_STATUS GetResult(DOM_Object *this_object, ES_Value *value, TempBuffer *buf, ES_ValueString *value_string);

	/** Closes and deletes the underlying blob, if there is any. */
	void CloseBlob();

	void GCTrace(DOM_Runtime *runtime);
};

/**
 * The FileReader interface in the W3C File API.
 */
class DOM_FileReader
	: public DOM_Object,
	  public MessageObject,
	  public ListElement<DOM_FileReader>,
	  public DOM_EventTargetOwner,
	  public DOM_FileReader_Base
{

private:
	DOM_FileReader()
		: m_id(g_opera->dom_module.next_file_reader_id++),
		  m_public_state(EMPTY),
		  m_current_read_serial(0),
		  m_delayed_progress_event_pending(FALSE),
		  m_current_read_loaded(0),
		  m_current_read_total(0),
		  m_error(NULL)
	{
	}

	unsigned m_id;

	enum ReadState // From spec.
	{
		EMPTY = 0,
		LOADING = 1,
		DONE = 2
	};

	ReadState m_public_state;

	unsigned m_current_read_serial;
	double m_current_read_last_progress_msg_time;
	BOOL m_delayed_progress_event_pending;
	double m_current_read_loaded;
	double m_current_read_total;

	DOM_FileError* m_error;

	void SendEvent(DOM_EventType type, ES_Thread* interrupted_thread, ES_Thread** created_event_thread = NULL);

	/** DOM_FileError::WORKING_JUST_FINE clears the error. */
	void SetError(DOM_FileError::ErrorCode code);

public:
	static OP_STATUS Make(DOM_FileReader*& reader, DOM_Runtime* runtime);

	virtual ~DOM_FileReader();

	/** Adding the constants for prototypes and constructors. */
	static void ConstructFileReaderObjectL(ES_Object* object, DOM_Runtime* runtime);

	/** Stop reading from files and remove locks that keep this object alive. */
	void Abort();

	// From the MessageObject interface.
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// From the DOM_EventTargetOwner interface.
	virtual DOM_Object *GetOwnerObject() { return this; }

	// From the DOM_Object interface.
	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_FILEREADER || DOM_Object::IsA(type); }

	virtual void GCTrace();

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	// Also: dispatchEvent
	DOM_DECLARE_FUNCTION(abort);
	enum {
		FUNCTIONS_dispatchEvent = 1,
		FUNCTIONS_abort,

		FUNCTIONS_ARRAY_SIZE
	};

	static int readBlob(DOM_Object *&reader, DOM_Blob *blob, const unsigned char *&buffer, unsigned &length, ES_Value *return_value, DOM_Runtime *origining_runtime);
	/**< Read the given Blob into memory and return a reference to its contents.
	     If the Blob is an in-memory structure already, the array buffer will
	     be returned synchronously (assuming no OOM.) This method is in
	     a form amenable for use by DOM APIs having to send/transmit Blobs in
	     their reduced form (cf. XMLHttpRequest and WebSocket.)

	     If the read operation has to suspend and restart (currently due to
	     external file reads), this is signalled by returning (ES_SUSPEND | ES_RESTART)
	     along with setting 'reader' to refer to the file reading object that
	     must be re-invoked on restart. A restarted call to readBlob() must
	     supply this reader as first parameter, but leave 'blob' as NULL.

	     Upon successful completion of a series of restarted calls, ES_VALUE is
	     returned. If the Blob contents is stored in-memory external to
	     the ES heap, 'buffer' and 'length' will refer to the buffer and its
	     length, respectively, and 'return_value' will be left undefined (VALUE_UNDEFINED.)

	     If the result is in the ES heap and an ArrayBuffer or typed array,
	     'return_value' contains the VALUE_OBJECT referring to it. 'buffer'
	     and 'length' will in that also be set to refer to the start of the
	     buffer (view) and its length.

	     The ownership of the in-memory buffer is not transferred to the caller
	     upon return.

	     Other DOM call error values are possible: ES_NO_MEMORY on OOM,
	     ES_EXCEPTION on not being able to read the underlying (file) blob. */

	// Also: {add,remove}EventListener
	DOM_DECLARE_FUNCTION_WITH_DATA(readAs);
	enum {
		FUNCTIONS_readAsArrayBuffer = 1,
		FUNCTIONS_readAsBinaryString,
		FUNCTIONS_readAsText,
		FUNCTIONS_readAsDataURL,

		FUNCTIONS_addEventListener,
		FUNCTIONS_removeEventListener,

		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};
};

#endif // DOM_FILEREADER_H
