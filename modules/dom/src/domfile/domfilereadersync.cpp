/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domfile/domfile.h"
#include "modules/dom/src/domfile/domfilereadersync.h"
#include "modules/dom/src/domfile/domfileexception.h"
#include "modules/dom/src/domglobaldata.h"

#ifdef DOM_WEBWORKERS_SUPPORT
/* virtual */ int
DOM_FileReaderSync_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_FileReaderSync* reader;
	CALL_FAILED_IF_ERROR(DOM_FileReaderSync::Make(reader, static_cast<DOM_Runtime*>(origining_runtime)));
	DOMSetObject(return_value, reader);
	return ES_VALUE;
}
#endif // DOM_WEBWORKERS_SUPPORT

/* static */ OP_STATUS
DOM_FileReaderSync::Make(DOM_FileReaderSync*& reader, DOM_Runtime* runtime)
{
	return DOMSetObjectRuntime(reader = OP_NEW(DOM_FileReaderSync, ()),
							   runtime,
							   runtime->GetPrototype(DOM_Runtime::FILEREADERSYNC_PROTOTYPE),
							   "FileReaderSync");
}

/* virtual */ void
DOM_FileReaderSync::GCTrace()
{
	DOM_FileReader_Base::GCTrace(GetRuntime());
}

/* static */ int
DOM_FileReaderSync::readAs(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(reader, DOM_TYPE_FILEREADERSYNC, DOM_FileReaderSync);

	if (argc > 0)
	{
		DOM_CHECK_ARGUMENTS("o");

		DOM_ARGUMENT_OBJECT(blob, 0, DOM_TYPE_BLOB, DOM_Blob);
		OpFileLength start_pos = static_cast<OpFileLength>(blob->m_sliced_start);
		if (start_pos != blob->m_sliced_start) // Overflow?
			return DOM_CALL_INTERNALEXCEPTION(WRONG_ARGUMENTS_ERR);

		reader->EmptyResult();

		OP_STATUS status;
		if (OpStatus::IsError(status = blob->MakeReadView(reader->m_current_read_view, 0, blob->GetSliceLength())))
		{
			reader->CloseBlob();
			if (OpStatus::IsMemoryError(status))
				return ES_NO_MEMORY;
			return DOM_FileException::CreateException(return_value, DOM_FileError::NOT_FOUND_ERR, origining_runtime);
		}

		// data is one of the enum OutputType values.
		status = reader->InitResult(static_cast<OutputType>(data), argc, argv, blob);
		if (OpStatus::IsError(status))
		{
			reader->CloseBlob();
			if (OpStatus::IsMemoryError(status))
				return ES_NO_MEMORY;
			return DOM_FileException::CreateException(return_value, DOM_FileError::ENCODING_ERR, origining_runtime);
		}
	}

	OP_ASSERT(reader->m_current_read_view);

	/* We have an open blob/file if we get here. If a file, just need to
	   read some, send some events and add to the result. For in-memory
	   blob resources, we read it all in one go. */
	OpFileLength bytes_read;
	OP_STATUS status = reader->ReadFromBlob(&bytes_read);
	if (OpStatus::IsError(status) || reader->m_current_read_view->AtEOF())
	{
		reader->CloseBlob();
		DOM_FileError::ErrorCode error_code = DOM_FileError::NOT_READABLE_ERR;
		if (OpStatus::IsSuccess(status))
		{
			status = reader->FlushToResult();
			error_code = DOM_FileError::ENCODING_ERR;
		}

		if (OpStatus::IsError(status))
			return DOM_FileException::CreateException(return_value, error_code, origining_runtime);

		CALL_FAILED_IF_ERROR(reader->GetResult(reader, return_value, GetEmptyTempBuf(), &(g_DOM_globalData->string_with_length_holder)));
		return ES_VALUE;
	}

	// Read another small chunk at a time to not lock up the thread.
	return ES_SUSPEND | ES_RESTART;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_WITH_DATA_START(DOM_FileReaderSync)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileReaderSync, DOM_FileReaderSync::readAs, DOM_FileReader::ARRAY_BUFFER, "readAsArrayBuffer", NULL)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileReaderSync, DOM_FileReaderSync::readAs, DOM_FileReader::TEXT, "readAsText", "-s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileReaderSync, DOM_FileReaderSync::readAs, DOM_FileReader::DATA_URL, "readAsDataURL", NULL)
DOM_FUNCTIONS_WITH_DATA_END(DOM_FileReaderSync)
