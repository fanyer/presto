/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domfile/domfilereader.h"
#include "modules/dom/src/domfile/domfilereadersync.h"

#include "modules/dom/src/domfile/domfile.h"
#include "modules/dom/src/domfile/domblob.h"
#include "modules/dom/src/domfile/domfileerror.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domprogressevent.h"
#include "modules/dom/src/domglobaldata.h"

#include "modules/encodings/decoders/inputconverter.h"

#include "modules/formats/uri_escape.h"
#include "modules/formats/base64_decode.h"

#include "modules/pi/OpSystemInfo.h"

/* virtual */ int
DOM_FileReader_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_FileReader* reader;
	CALL_FAILED_IF_ERROR(DOM_FileReader::Make(reader, static_cast<DOM_Runtime*>(origining_runtime)));
	DOMSetObject(return_value, reader);
	return ES_VALUE;
}

/* virtual */
DOM_FileReader::~DOM_FileReader()
{
	GetEnvironment()->RemoveFileReader(this);
}

/* static */ OP_STATUS
DOM_FileReader::Make(DOM_FileReader*& reader, DOM_Runtime* runtime)
{
	return DOMSetObjectRuntime(reader = OP_NEW(DOM_FileReader, ()), runtime, runtime->GetPrototype(DOM_Runtime::FILEREADER_PROTOTYPE), "FileReader");
}

/* static */ void
DOM_FileReader::ConstructFileReaderObjectL(ES_Object* object, DOM_Runtime* runtime)
{
	PutNumericConstantL(object, "EMPTY", EMPTY, runtime);
	PutNumericConstantL(object, "LOADING", LOADING, runtime);
	PutNumericConstantL(object, "DONE", DONE, runtime);
}

/* virtual */ void
DOM_FileReader::GCTrace()
{
	GCMark(event_target);
	GCMark(m_error);
	DOM_FileReader_Base::GCTrace(GetRuntime());
}

void
DOM_FileReader::Abort()
{
	GetEnvironment()->RemoveFileReader(this);
	if (m_current_read_view)
	{
		CloseBlob();
		g_main_message_handler->UnsetCallBack(this, MSG_DOM_READ_FILE);
		m_delayed_progress_event_pending = FALSE;
	}
	m_public_state = DONE;
}


/* virtual */ ES_GetState
DOM_FileReader::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_GetState res = GetEventProperty(property_name, value, static_cast<DOM_Runtime*>(origining_runtime));
		if (res != GET_FAILED)
			return res;
	}
	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_FileReader::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_result:
		if (value)
			GET_FAILED_IF_ERROR(GetResult(this, value, GetEmptyTempBuf(), &(g_DOM_globalData->string_with_length_holder)));
		return GET_SUCCESS;

	case OP_ATOM_error:
		DOMSetObject(value, m_error);
		return GET_SUCCESS;

	case OP_ATOM_readyState:
		DOMSetNumber(value, m_public_state);
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_FileReader::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_PutState res = PutEventProperty(property_name, value, static_cast<DOM_Runtime*>(origining_runtime));
		if (res != PUT_FAILED)
			return res;
	}
	return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_FileReader::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_result:
	case OP_ATOM_error:
	case OP_ATOM_readyState:
		return PUT_SUCCESS;
	}

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_FileReader::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	enumerator->AddPropertyL("onabort");
	enumerator->AddPropertyL("onerror");
	enumerator->AddPropertyL("onload");
#ifdef PROGRESS_EVENTS_SUPPORT
	enumerator->AddPropertyL("onloadstart");
	enumerator->AddPropertyL("onloadend");
	enumerator->AddPropertyL("onprogress");
#endif // PROGRESS_EVENTS_SUPPORT

	return GET_SUCCESS;
}

DOM_FileReader_Base::~DOM_FileReader_Base()
{
	OP_DELETE(m_text_result_decoder);
	OP_DELETEA(m_data_url_content_type);
	CloseBlob();
	OP_ASSERT(!m_current_read_view);
}

OP_STATUS
DOM_FileReader_Base::FlushToResult()
{
	if (m_current_output_type == DATA_URL && m_data_url_use_base64 && m_result_pending_len > 0)
	{
		MIME_Encode_Error base64_encoding_status;
		int output_len;
		char* output = NULL;
		base64_encoding_status = MIME_Encode_SetStr(output, output_len, reinterpret_cast<char *>(m_result_pending), m_result_pending_len, NULL, GEN_BASE64_ONELINE);
		if (base64_encoding_status != MIME_NO_ERROR)
			return OpStatus::ERR_NO_MEMORY;
		OP_STATUS status = m_result.Append(output, output_len);
		OP_DELETEA(output);
		return status;
	}
	else if (m_current_output_type == TEXT && m_result_pending_len > 0)
	{
		if (m_text_result_decoder)
			return OpStatus::ERR; // Not decodable or we would have decoded it already.

		// We delayed converting because we couldn't decide charset encoding.
		OP_ASSERT(m_result_pending_len == 1);

		// Our single byte should be converted as utf-8 (the default) which
		// makes decoding it trivial.
		if ((m_result_pending[0] & 0x7f) != m_result_pending[0])
			return OpStatus::ERR; // Not decodable.

		uni_char ch = m_result_pending[0];
		return m_binary_result.Append(reinterpret_cast<char *>(&ch), sizeof(uni_char));
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_FileReader_Base::InitResult(OutputType output_type, int argc, ES_Value* argv, DOM_Blob* blob)
{
	m_current_output_type = output_type;

	switch (m_current_output_type)
	{
	case TEXT:
		if (argc > 1 && argv[1].type == VALUE_STRING)
			RETURN_IF_ERROR(m_text_result_charset.SetUTF8FromUTF16(argv[1].value.string));
		break;

	case DATA_URL:
		{
			const uni_char *content_type = blob->GetContentType();
			if (content_type)
			{
				m_data_url_content_type = SetNewStr(content_type);
				if (!m_data_url_content_type)
					return OpStatus::ERR_NO_MEMORY;
			}
		}
		break;

	case ARRAY_BUFFER:
	case BINARY_STRING:
		break; // Nothing special.
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_FileReader_Base::AppendToResult(const unsigned char *prefix_buffer, unsigned prefix_length, const unsigned char *data, unsigned data_length)
{
	OP_ASSERT(m_result_pending_len == 0 || !"ReadOldData() wasn't called");

	unsigned total_data_length = prefix_length + data_length;

	TempBuffer tempbuf;
	if (m_current_output_type == TEXT)
	{
		if (!m_text_result_decoder)
		{
			if (!m_text_result_charset.IsEmpty())
				RETURN_IF_MEMORY_ERROR(InputConverter::CreateCharConverter(m_text_result_charset.CStr(), &m_text_result_decoder));

			if (!m_text_result_decoder)
			{
				unsigned char first_byte = total_data_length < 1 ? 0 : (prefix_length > 0 ? prefix_buffer[0] : data[0]);
				// Detect charset or delay until we have enough data to properly detect charset.
				if (total_data_length == 0 || total_data_length == 1 && (first_byte == 0xfe || first_byte == 0xff))
				{
					// Delay decision, until FlushToResult in the worst case.
					m_result_pending_len = total_data_length;
					op_memcpy(m_result_pending + prefix_length, data, data_length);
					return OpStatus::OK;
				}
				else
				{
					const char *charset = "utf-8";
					if (total_data_length >= 2)
					{
						unsigned char bytes[2];
						if (prefix_length > 1)
						{
							bytes[0] = prefix_buffer[0];
							bytes[1] = prefix_buffer[1];
						}
						else if (prefix_length == 1)
						{
							bytes[0] = prefix_buffer[0];
							bytes[1] = data[0];
						}
						else
						{
							bytes[0] = data[0];
							bytes[1] = data[1];
						}

						if (bytes[0] == 0xfe && bytes[1] == 0xff || bytes[0] == 0xff && bytes[1] == 0xfe)
							charset = "utf-16";
					}
					RETURN_IF_ERROR(InputConverter::CreateCharConverter(charset, &m_text_result_decoder));
				}
			}
		}

		OP_ASSERT(m_text_result_decoder);
		while (total_data_length > 0)
		{
			// This assumes that the input converts 1 byte -> 1 uni_char. If it's wrong we allocate
			// too much or need to do more iterations.
			unsigned expected_needed_char_len = total_data_length + 1;
			RETURN_IF_ERROR(tempbuf.Expand(expected_needed_char_len));

			uni_char* dest = tempbuf.GetStorage();
			// Leave room to insert a trailing \0.
			unsigned dest_len = sizeof(uni_char)*(tempbuf.GetCapacity() - 1);
			tempbuf.SetCachedLengthPolicy(TempBuffer::UNTRUSTED);
			int bytes_read = 0;
			int bytes_written = 0;
			if (prefix_length > 0)
			{
				bytes_written = m_text_result_decoder->Convert(prefix_buffer, prefix_length, dest, dest_len, &bytes_read);
				if (bytes_written == -1)
					return OpStatus::ERR_NO_MEMORY;
			}

			int bytes_read_next = 0;
			int bytes_written_next = m_text_result_decoder->Convert(data, data_length, dest + bytes_written, dest_len - bytes_written, &bytes_read_next);
			if (bytes_written_next == -1)
				return OpStatus::ERR_NO_MEMORY;

			bytes_written += bytes_written_next;
			bytes_read += bytes_read_next;

			dest[bytes_written / sizeof(uni_char)] = '\0';
			total_data_length -= bytes_read;
			data += bytes_read;

			if (bytes_read == 0)
			{
				if (total_data_length > static_cast<int>(sizeof(m_result_pending)))
				{
					// A decoder we can't handle.
					OP_ASSERT(!"What kind of decoder did we trip on now?");
					return OpStatus::ERR;
				}

				m_result_pending_len += data_length;
				op_memcpy(m_result_pending + prefix_length, data, data_length);
				data += data_length;
				total_data_length -= data_length;
			}

			RETURN_IF_ERROR(m_binary_result.Append(reinterpret_cast<char *>(dest), bytes_written));
			tempbuf.Clear();
		}
	}
	else if (m_current_output_type == ARRAY_BUFFER || m_current_output_type == BINARY_STRING)
	{
		OP_ASSERT(prefix_length == 0);
		return m_binary_result.Append(reinterpret_cast<const char *>(data), data_length);
	}
	else if (m_current_output_type == DATA_URL)
	{
		if (m_result.Length() == 0)
		{
			RETURN_IF_ERROR(m_result.Append("data:"));
			OpString8 str;
			RETURN_IF_ERROR(str.SetUTF8FromUTF16(m_data_url_content_type));
			RETURN_IF_ERROR(m_result.Append(str.CStr()));
			m_data_url_use_base64 = !(m_data_url_content_type && uni_strncmp(m_data_url_content_type, "text/", 5) == 0);
			if (m_data_url_use_base64)
				RETURN_IF_ERROR(m_result.Append(";base64"));
			RETURN_IF_ERROR(m_result.Append(','));
		}

		if (m_data_url_use_base64)
		{
			MIME_Encode_Error base64_encoding_status;
			if (total_data_length > 0)
			{
				int to_consume = (total_data_length / 3) * 3;

				unsigned char *data_buffer = const_cast<unsigned char *>(data);
				if (prefix_length > 0)
				{
					/* Unfortunate copying together of pieces, but will only conceivably
					   happen for medium-sized file input progress chunks where the read
					   buffer isn't filled up. */
					data_buffer = OP_NEWA(unsigned char, total_data_length);
					RETURN_OOM_IF_NULL(data_buffer);
					op_memcpy(data_buffer, prefix_buffer, prefix_length);
					op_memcpy(data_buffer + prefix_length, data, data_length);
				}
				char* output = NULL;
				int output_len;
				base64_encoding_status = MIME_Encode_SetStr(output, output_len, reinterpret_cast<const char *>(data_buffer), to_consume, NULL, GEN_BASE64_ONELINE);
				if (prefix_length > 0)
				{
					OP_DELETEA(data_buffer);
					data_buffer = NULL;
				}
				if (base64_encoding_status != MIME_NO_ERROR)
					return OpStatus::ERR_NO_MEMORY;
				OP_ASSERT(static_cast<unsigned>(output_len) == (total_data_length / 3) * 4);

				/* The leftover from 'data' is what's after the bytes consumed by
				   the encoder (minus the bytes that were prefixed from the previous chunk.) */
				data += to_consume - prefix_length;
				total_data_length -= to_consume;
				OP_STATUS status = m_result.Append(output, output_len);
				OP_DELETEA(output);
				RETURN_IF_ERROR(status);

				if (total_data_length > 0)
				{
					// Move any remaining bytes (one or two) to our pending buffer.
					OP_ASSERT(total_data_length <= 2);
					m_result_pending_len = total_data_length;
					m_result_pending[0] = data[0];
					if (total_data_length > 1)
						m_result_pending[1] = data[1];
				}
			}
		}
		else
		{
			OP_ASSERT(prefix_length == 0);
			int data_url_escape_flags = UriEscape::NonSpaceCtrl | UriEscape::UnsafePrintable | UriEscape::URIExcluded | UriEscape::Range_80_ff;
			int needed_len = UriEscape::GetEscapedLength(reinterpret_cast<const char *>(data), total_data_length, data_url_escape_flags);
			RETURN_IF_ERROR(tempbuf.Expand(needed_len + 1));
			uni_char* dest = tempbuf.GetStorage();
			UriEscape::Escape(dest, reinterpret_cast<const char *>(data), total_data_length, data_url_escape_flags);
			dest[needed_len] = '\0';
			tempbuf.SetCachedLengthPolicy(TempBuffer::UNTRUSTED);
		}

		m_result.Append(tempbuf.GetStorage(), tempbuf.Length());
	}
	else
		OP_ASSERT(!"We can't get here yet since we don't support this type");

	return OpStatus::OK;
}

unsigned
DOM_FileReader_Base::ReadOldData(unsigned char *buf)
{
	unsigned unread_bytes = m_result_pending_len;
	m_result_pending_len = 0;
	if (unread_bytes > 0)
		op_memcpy(buf, m_result_pending, unread_bytes);
	return unread_bytes;
}

void
DOM_FileReader_Base::EmptyResult()
{
	m_result.FreeStorage();
	OP_DELETE(m_text_result_decoder);
	m_text_result_decoder = NULL;

	op_free(m_data_url_content_type);
	m_data_url_content_type = NULL;
	m_result_pending_len = 0;

	m_binary_result.FreeStorage();

	m_current_output_type = NONE;
}

OP_STATUS
DOM_FileReader_Base::GetResult(DOM_Object *this_object, ES_Value* value, TempBuffer* buffer, ES_ValueString* value_string)
{
	OP_ASSERT(value);
	switch (m_current_output_type)
	{
	case TEXT:
		DOM_Object::DOMSetStringWithLength(value, &(g_DOM_globalData->string_with_length_holder), reinterpret_cast<uni_char *>(m_binary_result.GetStorage()), static_cast<unsigned>(m_binary_result.Length()) / sizeof(uni_char));
		break;
	case DATA_URL:
		DOM_Object::DOMSetString(value, &m_result);
		break;
	case BINARY_STRING:
	{
		unsigned long length = m_binary_result.Length();
		RETURN_IF_ERROR(buffer->Expand(length));

		const unsigned char *src = reinterpret_cast<const unsigned char *>(m_binary_result.GetStorage());
		uni_char *dest = buffer->GetStorage();
		for (unsigned long i = 0; i < length; i++)
			dest[i] = static_cast<uni_char>(src[i]);

		DOM_Object::DOMSetStringWithLength(value, value_string, dest, length);
		break;
	}
	case ARRAY_BUFFER:
		if (!m_array_buffer)
		{
			unsigned long length = m_binary_result.Length();
			RETURN_IF_ERROR(this_object->GetRuntime()->CreateNativeArrayBufferObject(&m_array_buffer, length, reinterpret_cast<unsigned char *>(m_binary_result.GetStorage())));
			m_binary_result.ReleaseStorage();
		}
		DOM_Object::DOMSetObject(value, m_array_buffer);
		break;
	default:
		DOM_Object::DOMSetNull(value);
		break;
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_FileReader_Base::ReadFromBlob(OpFileLength* bytes_read)
{
	unsigned char short_buf[10]; // ARRAY OK sof 2012-03-26
	unsigned prefix_length = ReadOldData(short_buf);

	RETURN_IF_ERROR(m_current_read_view->ReadAvailable(this, short_buf, prefix_length, bytes_read));
	return OpStatus::OK;
}

void
DOM_FileReader_Base::CloseBlob()
{
	if (m_current_read_view)
	{
		OP_DELETE(m_current_read_view);
		m_current_read_view = NULL;
	}
}

void
DOM_FileReader_Base::GCTrace(DOM_Runtime *runtime)
{
	if (m_current_read_view)
		m_current_read_view->GCTrace();

	if (m_array_buffer)
		runtime->GCMark(m_array_buffer);
}

void
DOM_FileReader::SendEvent(DOM_EventType type, ES_Thread* interrupt_thread, ES_Thread** created_thread /* = NULL */)
{
	if (created_thread)
		*created_thread = NULL;

	if (m_quiet_read)
		return;

	if (GetEventTarget() && GetEventTarget()->HasListeners(type, NULL, ES_PHASE_AT_TARGET))
	{
#ifdef PROGRESS_EVENTS_SUPPORT
		DOM_ProgressEvent* read_event;
		if (OpStatus::IsSuccess(DOM_ProgressEvent::Make(read_event, GetRuntime())))
		{
			read_event->InitProgressEvent(m_current_read_total != 0, static_cast<OpFileLength>(m_current_read_loaded), static_cast<OpFileLength>(MAX(m_current_read_loaded, m_current_read_total)));
			read_event->InitEvent(type, this);
			GetEnvironment()->SendEvent(read_event, interrupt_thread, created_thread);
		}
#else
		DOM_Event* read_event;
		if (OpStatus::IsSuccess(DOMSetObjectRuntime(read_event = OP_NEW(DOM_Event, ()), GetRuntime(),
			GetRuntime()->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "ProgressEvent")))
		{
			read_event->InitEvent(type, this);
			GetEnvironment()->SendEvent(read_event, interrupt_thread);
		}
#endif // PROGRESS_EVENTS_SUPPORT
	}
}

void
DOM_FileReader::SetError(DOM_FileError::ErrorCode code)
{
	m_error = NULL;
	if (code != DOM_FileError::WORKING_JUST_FINE)
	{
		DOM_FileError* new_error;
		if (OpStatus::IsSuccess(DOM_FileError::Make(new_error, code, GetRuntime())))
			m_error = new_error;
	}
}

/* virtual */ void
DOM_FileReader::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(static_cast<unsigned>(par1) == m_id);
	OP_ASSERT(msg == MSG_DOM_READ_FILE || msg == MSG_DOM_PROGRESS_EVENT_TICK);

	if (static_cast<unsigned>(par2) != m_current_read_serial)
	{
		// Old message, just ignore
		return;
	}

	BOOL send_progress_event = FALSE;
	if (msg == MSG_DOM_PROGRESS_EVENT_TICK)
	{
		send_progress_event = m_public_state != DONE;
		m_delayed_progress_event_pending = FALSE;
	}

	if (msg == MSG_DOM_READ_FILE)
	{
		/* We have an open blob/file if we get here. If a file, just need to
		   read some, send some events and add to the result. For in-memory
		   blob resources, we read it all in one go. */
		OpFileLength bytes_read;
		OP_STATUS status = ReadFromBlob(&bytes_read);
		if (OpStatus::IsSuccess(status) && bytes_read > 0)
		{
			m_current_read_loaded += bytes_read;
			send_progress_event = TRUE;
		}

		if (OpStatus::IsError(status) || m_current_read_view->AtEOF())
		{
			GetEnvironment()->RemoveFileReader(this);
			g_main_message_handler->UnsetCallBack(this, MSG_DOM_READ_FILE);
			CloseBlob();
			m_delayed_progress_event_pending = FALSE;
			m_public_state = DONE;
			DOM_FileError::ErrorCode error_code = DOM_FileError::NOT_READABLE_ERR;
			if (OpStatus::IsSuccess(status))
			{
				status = FlushToResult();
				error_code = DOM_FileError::ENCODING_ERR;
			}

			if (OpStatus::IsError(status))
			{
				SendEvent(ONERROR, NULL);
				SetError(error_code);
			}
			else
			{
#ifdef PROGRESS_EVENTS_SUPPORT
				/* Issue final 'progress' before 'load' and 'loadend'. */
				SendEvent(ONPROGRESS, NULL);
				send_progress_event = FALSE;
#endif // PROGRESS_EVENTS_SUPPORT
				SendEvent(ONLOAD, NULL);
			}

#ifdef PROGRESS_EVENTS_SUPPORT
			SendEvent(ONLOADEND, NULL);
#endif // PROGRESS_EVENTS_SUPPORT
		}
		else
			g_main_message_handler->PostMessage(MSG_DOM_READ_FILE, m_id, m_current_read_serial);
	}

	if (send_progress_event)
	{
		// Send a progress message if it's not too often.
		// Must not be sent more often than once every
		// DOM_FILE_READ_PROGRESS_EVENT_INTERVAL milliseconds.
		double current_time = g_op_time_info->GetRuntimeMS();
		unsigned elapsed_time = static_cast<unsigned>(current_time - m_current_read_last_progress_msg_time);
		if (elapsed_time >= DOM_FILE_READ_PROGRESS_EVENT_INTERVAL)
		{
#ifdef PROGRESS_EVENTS_SUPPORT
			SendEvent(ONPROGRESS, NULL);
#endif // PROGRESS_EVENTS_SUPPORT
			m_current_read_last_progress_msg_time = current_time;
		}
		else if (!m_delayed_progress_event_pending)
		{
			g_main_message_handler->PostMessage(MSG_DOM_PROGRESS_EVENT_TICK, m_id, m_current_read_serial,
				DOM_FILE_READ_PROGRESS_EVENT_INTERVAL - elapsed_time);
			m_delayed_progress_event_pending = TRUE;
		}
	}
}

class DOM_FileReaderRestartObject : public DOM_Object
{
public:
	ES_Object* m_abort_restart_object;
	DOM_Blob* m_blob;

	virtual void GCTrace() { GCMark(m_abort_restart_object); GCMark(m_blob); }
};

/* static */ int
DOM_FileReader::readBlob(DOM_Object *&reader, DOM_Blob *blob, const unsigned char *&buffer, unsigned &length, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_FileReaderSync *sync_reader;
	if (blob)
	{
		/* Optimizations: Blob'ed ArrayBuffers are returned directly..same with
		   slices of them. */
		if (blob->GetType() == DOM_Blob::BlobBuffer)
		{
			DOM_BlobBuffer *blob_buffer = static_cast<DOM_BlobBuffer *>(blob);
			if (blob_buffer->IsSlice())
				length = static_cast<unsigned>(blob_buffer->GetSliceLength());
			else
				length = static_cast<unsigned>(blob_buffer->GetSize());
			unsigned start_offset = static_cast<unsigned>(blob_buffer->GetSliceStart());
			buffer = blob_buffer->GetBuffer() + start_offset;
			if (blob_buffer->GetArrayBufferView())
				DOMSetObject(return_value, blob_buffer->GetArrayBufferView());
			else
				DOMSetUndefined(return_value);
			return ES_VALUE;
		}
		else if (blob->GetType() == DOM_Blob::BlobSlice)
		{
			DOM_BlobSlice *blob_slice = static_cast<DOM_BlobSlice *>(blob);
			if (blob_slice->GetSlicedBlob()->GetType() == DOM_Blob::BlobBuffer)
			{
				OP_ASSERT(blob_slice->GetSize() == static_cast<double>(blob_slice->GetSliceLength()));
				length = static_cast<unsigned>(blob_slice->GetSize());

				unsigned start_offset = static_cast<unsigned>(blob_slice->GetSliceStart());
				DOM_BlobBuffer *blob_buffer = static_cast<DOM_BlobBuffer *>(blob_slice->GetSlicedBlob());
				buffer = blob_buffer->GetBuffer() + start_offset;
				if (blob_buffer->GetArrayBufferView())
					DOMSetObject(return_value, blob_buffer->GetArrayBufferView());
				else
					DOMSetUndefined(return_value);
				return ES_VALUE;
			}
		}

		CALL_FAILED_IF_ERROR(DOM_FileReaderSync::Make(sync_reader, origining_runtime));
		sync_reader->SetWithProgressEvents(FALSE);
		ES_Value arg;
		DOMSetObject(&arg, blob);
		int result = DOM_FileReaderSync::readAs(sync_reader, &arg, 1, return_value, origining_runtime, DOM_FileReader::ARRAY_BUFFER);
		if (result & ES_SUSPEND)
			reader = sync_reader;
		else if (result == ES_VALUE)
		{
			OP_ASSERT(return_value->type == VALUE_OBJECT && ES_Runtime::IsNativeArrayBufferObject(return_value->value.object));
			buffer = ES_Runtime::GetArrayBufferStorage(return_value->value.object);
			length = ES_Runtime::GetArrayBufferLength(return_value->value.object);
		}
		return result;
	}
	else
	{
		OP_ASSERT(reader && reader->IsA(DOM_TYPE_FILEREADERSYNC));
		sync_reader = static_cast<DOM_FileReaderSync *>(reader);
		int result = DOM_FileReaderSync::readAs(sync_reader, NULL, -1, return_value, origining_runtime, DOM_FileReader::ARRAY_BUFFER);
		if (result == ES_VALUE)
		{
			OP_ASSERT(return_value->type == VALUE_OBJECT && ES_Runtime::IsNativeArrayBufferObject(return_value->value.object));
			buffer = ES_Runtime::GetArrayBufferStorage(return_value->value.object);
			length = ES_Runtime::GetArrayBufferLength(return_value->value.object);
		}
		return result;
	}
}

/* static */ int
DOM_FileReader::readAs(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(reader, DOM_TYPE_FILEREADER, DOM_FileReader);

	DOM_Blob* blob;
	DOM_FileReaderRestartObject* restart_object = NULL;
	BOOL do_abort = FALSE;
	if (argc < 0)
	{
		OP_ASSERT(return_value->type == VALUE_OBJECT);
		restart_object = static_cast<DOM_FileReaderRestartObject *>(ES_Runtime::GetHostObject(return_value->value.object));
		blob = restart_object->m_blob;
		if (restart_object && restart_object->m_abort_restart_object)
			do_abort = TRUE;
		DOMSetUndefined(return_value);
	}
	else
	{
		DOM_CHECK_ARGUMENTS("o");
		DOM_ARGUMENT_OBJECT_EXISTING(blob, 0, DOM_TYPE_BLOB, DOM_Blob);
		if (reader->m_public_state == LOADING)
			do_abort = TRUE;
	}

	if (do_abort)
	{
		ES_Value abort_value;
		int abort_argc = 0;
		if (restart_object && restart_object->m_abort_restart_object)
		{
			DOMSetObject(&abort_value, restart_object->m_abort_restart_object);
			abort_argc = -1;
		}
		int ret = abort(this_object, NULL, abort_argc, &abort_value, origining_runtime);

		if (ret != ES_FAILED)
		{
			if (ret & ES_RESTART)
			{
				if (!restart_object)
				{
					CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(restart_object = OP_NEW(DOM_FileReaderRestartObject, ()), origining_runtime));
					restart_object->m_blob = blob;
				}
				restart_object->m_abort_restart_object = abort_value.value.object;
				DOMSetObject(return_value, restart_object);
			}
			else
				*return_value = abort_value;
			return ret;
		}
	}

	OpFileLength start_pos = static_cast<OpFileLength>(blob->m_sliced_start);
	if (start_pos != blob->m_sliced_start) // Overflow?
		return DOM_CALL_INTERNALEXCEPTION(WRONG_ARGUMENTS_ERR);

	reader->EmptyResult();
	reader->SetError(DOM_FileError::WORKING_JUST_FINE);
	reader->m_public_state = EMPTY;
	reader->m_current_read_serial++;

	reader->m_public_state = LOADING;
	reader->m_current_read_total = blob->GetSize();

	ES_Thread* current_thread = GetCurrentThread(origining_runtime);
#ifdef PROGRESS_EVENTS_SUPPORT
	reader->SendEvent(ONLOADSTART, current_thread);
#endif // PROGRESS_EVENTS_SUPPORT
	reader->m_current_read_last_progress_msg_time = g_op_time_info->GetRuntimeMS() - DOM_FILE_READ_PROGRESS_EVENT_INTERVAL;

	OP_ASSERT(reader->m_current_read_view == NULL);
	OP_STATUS status;
	if (OpStatus::IsError(status = blob->MakeReadView(reader->m_current_read_view, 0, blob->GetSliceLength())) ||
		OpStatus::IsError(status = g_main_message_handler->SetCallBack(reader, MSG_DOM_READ_FILE, reader->m_id)))
	{
		reader->CloseBlob();
		reader->SendEvent(ONERROR, current_thread);
		reader->SetError(DOM_FileError::NOT_FOUND_ERR);
#ifdef PROGRESS_EVENTS_SUPPORT
		reader->SendEvent(ONLOADEND, current_thread);
#endif // PROGRESS_EVENTS_SUPPORT
		if (OpStatus::IsMemoryError(status))
			return ES_NO_MEMORY;
		return ES_FAILED;
	}

	// data is one of the enum OutputType values.
	status = reader->InitResult(static_cast<OutputType>(data), argc, argv, blob);
	if (OpStatus::IsError(status))
	{
		reader->CloseBlob();
		reader->SendEvent(ONERROR, current_thread);
		reader->Abort();
		if (OpStatus::IsMemoryError(status))
		{
			reader->SetError(DOM_FileError::NOT_READABLE_ERR);
			return ES_NO_MEMORY;
		}
		reader->SetError(DOM_FileError::ENCODING_ERR);
#ifdef PROGRESS_EVENTS_SUPPORT
		reader->SendEvent(ONLOADEND, current_thread);
#endif // PROGRESS_EVENTS_SUPPORT
		return ES_FAILED;
	}

	reader->GetEnvironment()->AddFileReader(reader);
	g_main_message_handler->PostMessage(MSG_DOM_READ_FILE, reader->m_id, reader->m_current_read_serial);

	return ES_FAILED;
}

class DOM_FileReaderAbortRestartObject : public DOM_Object
{
public:
	DOM_EventType m_next_event;
};


/* static */ int
DOM_FileReader::abort(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	// This function is also used as a helper function for ::readAs()
	DOM_THIS_OBJECT(reader, DOM_TYPE_FILEREADER, DOM_FileReader);
	// Have to dispatch three sync event: error, abort, loadend.
	DOM_EventType next_event;
	DOM_FileReaderAbortRestartObject* restart_object = NULL;
	if (argc < 0)
	{
		OP_ASSERT(return_value->type == VALUE_OBJECT);
		restart_object = static_cast<DOM_FileReaderAbortRestartObject*>(ES_Runtime::GetHostObject(return_value->value.object));
		DOMSetUndefined(return_value);
		next_event = restart_object->m_next_event;
	}
	else
	{
		if (!reader->m_current_read_view)
			return ES_FAILED;

		reader->Abort();
		reader->EmptyResult();
		reader->SetError(DOM_FileError::ABORT_ERR);
		next_event = ONERROR;
	}

	DOM_EventType events[] =
	{
		ONERROR,
		ONABORT,
#ifdef PROGRESS_EVENTS_SUPPORT
		ONLOADEND,
#endif // PROGRESS_EVENTS_SUPPORT
		DOM_EVENT_NONE
	};

	for (int i = 0; events[i] != DOM_EVENT_NONE; i++)
	{
		if (next_event == events[i])
		{
			ES_Thread* event_thread;
			reader->SendEvent(next_event, GetCurrentThread(origining_runtime), &event_thread);
			next_event = events[i+1];
			if (event_thread)
			{
				// Must make sure the event handler runs before we change any state (goes for
				// when this is used as a helper function for readAs as well).
				if (!restart_object)
					CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(restart_object = OP_NEW(DOM_FileReaderAbortRestartObject, ()), origining_runtime));
				restart_object->m_next_event = next_event;
				DOMSetObject(return_value, restart_object);
				return ES_SUSPEND | ES_RESTART;
			}
		}
	}

	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_FileReader)
	DOM_FUNCTIONS_FUNCTION(DOM_FileReader, DOM_FileReader::abort, "abort", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_FileReader, DOM_Node::dispatchEvent, "dispatchEvent", 0)
DOM_FUNCTIONS_END(DOM_FileReader)

DOM_FUNCTIONS_WITH_DATA_START(DOM_FileReader)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileReader, DOM_FileReader::readAs, DOM_FileReader::ARRAY_BUFFER, "readAsArrayBuffer", NULL)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileReader, DOM_FileReader::readAs, DOM_FileReader::BINARY_STRING, "readAsBinaryString", NULL)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileReader, DOM_FileReader::readAs, DOM_FileReader::TEXT, "readAsText", "-s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileReader, DOM_FileReader::readAs, DOM_FileReader::DATA_URL, "readAsDataURL", NULL)

	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileReader, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileReader, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_FileReader)
