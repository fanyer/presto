/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef DOM3_SAVE

#include "modules/dom/src/domsave/lsserializer.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/attr.h"
#include "modules/dom/src/domcore/chardata.h"
#include "modules/dom/src/domcore/domconfiguration.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domsave/lsoutput.h"
#include "modules/dom/src/opera/domhttp.h"

#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/util/str.h"
#include "modules/util/tree.h"
#include "modules/util/tempbuf.h"
#include "modules/url/url2.h"
#include "modules/upload/upload.h"
#include "modules/xmlutils/xmlserializer.h"
#include "modules/xmlutils/xmldocumentinfo.h"

#include "modules/security_manager/include/security_manager.h"

DOM_LSSerializer::DOM_LSSerializer()
	: config(NULL),
	  newLine(NULL),
	  filter(NULL),
	  whatToShow(static_cast<unsigned>(SHOW_ALL))
{
}

DOM_LSSerializer::~DOM_LSSerializer()
{
	OP_DELETEA(newLine);
}

/* static */ OP_STATUS
DOM_LSSerializer::Make(DOM_LSSerializer *&serializer, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(serializer = OP_NEW(DOM_LSSerializer, ()), runtime, runtime->GetPrototype(DOM_Runtime::LSSERIALIZER_PROTOTYPE), "LSSerializer"));
	RETURN_IF_ERROR(DOM_DOMConfiguration::Make(serializer->config, environment));

	ES_Value valueTrue;
	DOMSetBoolean(&valueTrue, TRUE);

	ES_Value valueFalse;
	DOMSetBoolean(&valueFalse, FALSE);

	RETURN_IF_ERROR(serializer->config->AddParameter("discard-default-content", &valueTrue, DOM_DOMConfiguration::acceptBoolean));
	RETURN_IF_ERROR(serializer->config->AddParameter("format-pretty-print", &valueFalse, DOM_DOMConfiguration::acceptBoolean));
	RETURN_IF_ERROR(serializer->config->AddParameter("ignore-unknown-character-denormalizations", &valueTrue, DOM_DOMConfiguration::acceptTrue));
	RETURN_IF_ERROR(serializer->config->AddParameter("xml-declaration", &valueTrue, DOM_DOMConfiguration::acceptBoolean));

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_LSSerializer::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_newLine:
		if (newLine)
			DOMSetString(value, newLine);
		else
			DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_domConfig:
		DOMSetObject(value, config);
		return GET_SUCCESS;

	case OP_ATOM_filter:
		DOMSetObject(value, filter);
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_LSSerializer::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_newLine:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;

		PUT_FAILED_IF_ERROR(UniSetStr(newLine, value->value.string));
		return PUT_SUCCESS;

	case OP_ATOM_domConfig:
		return PUT_READ_ONLY;

	case OP_ATOM_filter:
		if (value->type != VALUE_OBJECT)
			return PutNameDOMException(TYPE_MISMATCH_ERR, value);
		else
		{
			filter = value->value.object;

			ES_Value wts;
			OP_BOOLEAN result;

			PUT_FAILED_IF_ERROR(result = origining_runtime->GetName(filter, UNI_L("whatToShow"), &wts));

			if (result == OpBoolean::IS_TRUE && wts.type == VALUE_NUMBER)
				whatToShow = (int) wts.value.number;
			else
				whatToShow = static_cast<unsigned>(SHOW_ALL);

			return PUT_SUCCESS;
		}
	}

	return PUT_FAILED;
}

/* virtual */ void
DOM_LSSerializer::GCTrace()
{
	GCMark(config);
	GCMark(filter);
}

class DOM_LSSerializer_State
	: public DOM_Object
#ifdef DOM_HTTP_SUPPORT
	, public MessageObject
	, public ES_ThreadListener
#endif // DOM_HTTP_SUPPORT
{
protected:
	DOM_LSSerializer *serializer;
	DOM_Node *node;
	DOM_Runtime *origining_runtime;

#ifdef DOM_HTTP_SUPPORT
	OutputConverter *converter;
	OpString uri;
	DOM_HTTPOutputStream *httpoutputstream;
	OpString8 encoding;

	MessageHandler *mh;
	URL url;
	ES_Thread *thread;
#endif // DOM_HTTP_SUPPORT

	TempBuffer buffer;

public:
	DOM_LSSerializer_State(DOM_LSSerializer *serializer, DOM_Node *node, DOM_Runtime *origining_runtime)
		: serializer(serializer)
		, node(node)
		, origining_runtime(origining_runtime)
#ifdef DOM_HTTP_SUPPORT
		, converter(NULL)
		, httpoutputstream(NULL)
		, mh(NULL)
		, thread(NULL)
#endif // DOM_HTTP_SUPPORT
	{
		buffer.SetExpansionPolicy(TempBuffer::AGGRESSIVE);
		buffer.SetCachedLengthPolicy(TempBuffer::TRUSTED);
	}

#ifdef DOM_HTTP_SUPPORT
	virtual ~DOM_LSSerializer_State()
	{
		if (mh)
		{
			url.StopLoading(mh);

			mh->UnsetCallBacks(this);

			OP_DELETE(mh);

			if (thread)
			{
				ES_ThreadListener::Remove();

				if (thread->GetBlockType() != ES_BLOCK_NONE)
					thread->Unblock();

				thread = NULL;
			}
		}

		OP_DELETE(converter);
	}
#endif // DOM_HTTP_SUPPORT

	virtual void GCTrace()
	{
		GCMark(serializer);
		GCMark(node);
	}

#ifdef DOM_HTTP_SUPPORT
	void SetConverter(OutputConverter *new_converter)
	{
		converter = new_converter;
	}

	void SetURI(OpString& new_uri)
	{
		uri.TakeOver(new_uri);
	}

	void SetHTTPOutputStream(DOM_HTTPOutputStream *new_httpoutputstream)
	{
		httpoutputstream = new_httpoutputstream;
	}
#endif // DOM_HTTP_SUPPORT

	int Serialize(ES_Value *return_value)
	{
#ifdef DOM_HTTP_SUPPORT
		if (mh)
		{
			DOMSetBoolean(return_value, url.Status(TRUE) == URL_LOADED);
			return ES_VALUE;
		}
#endif // DOM_HTTP_SUPPORT

		URL url;
		OP_STATUS status;

		XMLSerializer::Configuration configuration;
		XMLDocumentInformation docinfo;

		if (node->IsA(DOM_TYPE_DOCUMENT))
		{
			const XMLDocumentInformation *docinfo_base = ((DOM_Document *) node)->GetXMLDocumentInfo();

			BOOL include_xml_declaration = TRUE;
			ES_Value value;

			CALL_FAILED_IF_ERROR(serializer->GetConfig()->GetParameter(UNI_L("xml-declaration"), &value));

			if (value.type == VALUE_BOOLEAN && value.value.boolean == FALSE)
				include_xml_declaration = FALSE;

			if (include_xml_declaration)
			{
				XMLVersion version;
				XMLStandalone standalone;
				const uni_char *encoding;

				if (docinfo_base)
				{
					version = docinfo_base->GetVersion();
					standalone = docinfo_base->GetStandalone();
					encoding = docinfo_base->GetEncoding();
				}
				else
				{
					version = XMLVERSION_1_0;
					standalone = XMLSTANDALONE_NONE;
					encoding = NULL;
				}

#ifdef DOM_HTTP_SUPPORT
				OpString encoding16;
				if (converter)
				{
					const char *encoding8 = converter->GetDestinationCharacterSet();
					if (encoding8)
					{
						CALL_FAILED_IF_ERROR(encoding16.Set(encoding8));
						encoding = encoding16.CStr();
					}
				}

				CALL_FAILED_IF_ERROR(this->encoding.Set(encoding));
#endif // DOM_HTTP_SUPPORT

				CALL_FAILED_IF_ERROR(docinfo.SetXMLDeclaration(version, standalone, encoding));
			}

			if (docinfo_base && docinfo_base->GetDoctypeDeclarationPresent())
				CALL_FAILED_IF_ERROR(docinfo.SetDoctypeDeclaration(docinfo_base->GetDoctypeName(), docinfo_base->GetPublicId(), docinfo_base->GetSystemId(), docinfo_base->GetInternalSubset()));

			configuration.document_information = &docinfo;
		}

		ES_Value value;

		CALL_FAILED_IF_ERROR(serializer->GetConfig()->GetParameter(UNI_L("split-cdata-sections"), &value));
		if (value.type == VALUE_BOOLEAN && value.value.boolean == FALSE)
			configuration.split_cdata_sections = FALSE;

		CALL_FAILED_IF_ERROR(serializer->GetConfig()->GetParameter(UNI_L("discard-default-content"), &value));
		if (value.type == VALUE_BOOLEAN && value.value.boolean == FALSE)
			configuration.discard_default_content = FALSE;

		CALL_FAILED_IF_ERROR(serializer->GetConfig()->GetParameter(UNI_L("format-pretty-print"), &value));
		if (value.type == VALUE_BOOLEAN && value.value.boolean == TRUE)
			configuration.format_pretty_print = TRUE;

		XMLSerializer *serializer;

		CALL_FAILED_IF_ERROR(XMLSerializer::MakeToStringSerializer(serializer, &buffer));
		// Now we own |serializer| so no returns until it's deleted
		status = serializer->SetConfiguration(configuration);

		if (OpStatus::IsSuccess(status))
		{
			HTML_Element *element = node->GetThisElement();
			if (!element)
				element = node->GetPlaceholderElement();
			if (element)
				status = serializer->Serialize(origining_runtime->GetFramesDocument()->GetMessageHandler(), url, element, NULL, NULL);
		}

		OP_DELETE(serializer);

		CALL_FAILED_IF_ERROR(status);

		return Finish(return_value, origining_runtime);
	}

#ifdef DOM_HTTP_SUPPORT
	OP_STATUS SetCallbacks(URL_ID id)
	{
		RETURN_IF_ERROR(mh->SetCallBack(this, MSG_URL_DATA_LOADED, id));
		RETURN_IF_ERROR(mh->SetCallBack(this, MSG_HEADER_LOADED, id));
		RETURN_IF_ERROR(mh->SetCallBack(this, MSG_NOT_MODIFIED, id));
		RETURN_IF_ERROR(mh->SetCallBack(this, MSG_URL_LOADING_FAILED, id));
		RETURN_IF_ERROR(mh->SetCallBack(this, MSG_URL_MOVED, id));

		return OpStatus::OK;
	}
#endif // DOM_HTTP_SUPPORT

	int Finish(ES_Value *return_value, ES_Runtime *origining_runtime)
	{
#ifdef DOM_HTTP_SUPPORT
		if (uri.CStr())
		{
			mh = OP_NEW(MessageHandler, (NULL));
			if (!mh)
				return ES_NO_MEMORY;

			URL reference = origining_runtime->GetFramesDocument()->GetURL();

			url = g_url_api->GetURL(reference, uri.CStr(), static_cast<const uni_char *>(NULL), TRUE);

			BOOL allowed = FALSE;
			CALL_FAILED_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_LOADSAVE,
			                                                 OpSecurityContext((DOM_Runtime *) origining_runtime),
			                                                 OpSecurityContext(url, reference),
			                                                 allowed));

			if (!allowed)
				return ES_EXCEPT_SECURITY;

			CALL_FAILED_IF_ERROR(SetCallbacks(url.Id(TRUE)));

			OutputConverter *converter;
			CALL_FAILED_IF_ERROR(OutputConverter::CreateCharConverter(encoding.CStr(), &converter));

			TempBuffer bufferc;
			const unsigned char *src = (const unsigned char *) buffer.GetStorage();
			unsigned char *dest;
			int src_len = buffer.Length() * sizeof(uni_char);
			int dest_len = 0, read = 0, src_ptr = 0;
			OP_MEMORY_VAR int dest_ptr = 0;

			while (src_ptr < src_len)
			{
				CALL_FAILED_IF_ERROR(bufferc.Expand(dest_len > src_len ? dest_len + 256 : src_len));

				dest = (unsigned char *) bufferc.GetStorage();
				dest_len = bufferc.GetCapacity() * sizeof(uni_char);

				int result = converter->Convert(src + src_ptr, src_len - src_ptr, dest + dest_ptr, dest_len - dest_ptr, &read);

				if (result < 0)
				{
					OP_DELETE(converter);
					return ES_NO_MEMORY;
				}

				src_ptr += read;
				dest_ptr += result;
			}

			OP_DELETE(converter);

			Upload_BinaryBuffer *upload = OP_NEW(Upload_BinaryBuffer, ());
			if (!upload)
				return ES_NO_MEMORY;

			TRAPD(status, upload->InitL((unsigned char *) bufferc.GetStorage(), dest_ptr, UPLOAD_COPY_BUFFER, "text/xml", encoding.CStr(), ENCODING_BINARY));
			if (OpStatus::IsError(status))
			{
				OP_DELETE(upload);
				return OpStatus::IsMemoryError(status) ? ES_NO_MEMORY : ES_FAILED;
			}

			TRAP(status, upload->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION));
			if (OpStatus::IsError(status))
			{
				OP_DELETE(upload);
				return OpStatus::IsMemoryError(status) ? ES_NO_MEMORY : ES_FAILED;
			}

			url.SetHTTP_Method(HTTP_METHOD_PUT);
			url.SetHTTP_Data(upload, TRUE);

			CommState state = url.Load(mh, reference);

			if (state == COMM_LOADING)
			{
				thread = GetCurrentThread(origining_runtime);
				thread->AddListener(this);
				thread->Block();

				DOMSetObject(return_value, *this);
				return ES_SUSPEND | ES_RESTART;
			}

			DOMSetBoolean(return_value, FALSE);
		}
		else if (httpoutputstream)
		{
			CALL_FAILED_IF_ERROR(httpoutputstream->SetData(reinterpret_cast<const unsigned char *>(buffer.GetStorage()), UNICODE_SIZE(buffer.Length()), FALSE, NULL));
			DOMSetBoolean(return_value, TRUE);
		}
		else
#endif // DOM_HTTP_SUPPORT
			DOMSetString(return_value, &buffer);

		return ES_VALUE;
	}

#ifdef DOM_HTTP_SUPPORT
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
	{
		BOOL stopped = FALSE;

		switch (msg)
		{
		case MSG_URL_DATA_LOADED:
		case MSG_HEADER_LOADED:
		case MSG_NOT_MODIFIED:
		case MSG_URL_LOADING_FAILED:
			if (url.Status(TRUE) != URL_LOADING)
				stopped = TRUE;
			break;

		case MSG_URL_MOVED:
			URL moved = url.GetAttribute(URL::KMovedToURL, TRUE);
			if (!moved.IsEmpty())
				if (moved.GetServerName() != url.GetServerName())
				{
					moved.StopLoading(mh);
					stopped = TRUE;
				}
				else
				{
					mh->UnsetCallBacks(this);
					url = moved;
					if (OpStatus::IsMemoryError(SetCallbacks(url.Id(TRUE))))
						stopped = TRUE;
				}
			break;
		}

		if (stopped)
		{
			ES_ThreadListener::Remove();

			if (thread && thread->GetBlockType() != ES_BLOCK_NONE)
				thread->Unblock();

			thread = NULL;
		}
	}

	virtual OP_STATUS Signal(ES_Thread *signalled_thread, ES_ThreadSignal signal)
	{
		/* The thread is blocked, only ES_SIGNAL_CANCELLED can happen. */
		OP_ASSERT(signal != ES_SIGNAL_FINISHED && signal != ES_SIGNAL_FAILED);
		OP_ASSERT(signal == ES_SIGNAL_CANCELLED);

		ES_ThreadListener::Remove();
		thread = NULL;
		return OpStatus::OK;
	}
#endif // DOM_HTTP_SUPPORT
};

/* static */ int
DOM_LSSerializer::write(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime, int data)
{
	if (argc >= 0)
	{
		DOM_THIS_OBJECT(serializer, DOM_TYPE_LSSERIALIZER, DOM_LSSerializer);

		DOM_Node *node;
		OpAutoPtr<OutputConverter> converter;

#ifdef DOM_HTTP_SUPPORT
		OpString uri;
		const uni_char* encoding_uni = NULL;
		DOM_HTTPOutputStream *httpOutputStream = NULL;
#endif // DOM_HTTP_SUPPORT

		if (data == 0 || data == 1)
		{
#ifdef DOM_HTTP_SUPPORT
			if (data == 0)
			{
				DOM_CHECK_ARGUMENTS("oo");

				DOM_EnvironmentImpl *environment = serializer->GetEnvironment();
				ES_Object *output = argv[1].value.object;

				ES_Object *byteStream;
				CALL_FAILED_IF_ERROR(DOM_LSOutput::GetByteStream(byteStream, output, environment));
				if (byteStream)
				{
					if (DOM_Object *object = DOM_HOSTOBJECT(byteStream, DOM_Object))
						if (object->IsA(DOM_TYPE_HTTPOUTPUTSTREAM))
							httpOutputStream = static_cast<DOM_HTTPOutputStream *>(object);

					if (!httpOutputStream)
						return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
				}
				else
				{
					const uni_char *systemId;
					CALL_FAILED_IF_ERROR(DOM_LSOutput::GetSystemId(systemId, output, environment));
					if (systemId)
						CALL_FAILED_IF_ERROR(uri.Set(systemId));

					if (!uri.CStr())
						// FIXME: should signal a "no-output-specified" error.
						return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
				}

				// encoding will point to the tempbuffer so we need to avoid using
				// that or copy the string.
				CALL_FAILED_IF_ERROR(DOM_LSOutput::GetEncoding(encoding_uni, output, environment));
			}
			else if (data == 1)
			{
				DOM_CHECK_ARGUMENTS("os");
				CALL_FAILED_IF_ERROR(uri.Set(argv[1].value.string));
			}

			const char* encoding;
			if (encoding_uni)
				encoding = make_singlebyte_in_tempbuffer(encoding_uni, uni_strlen(encoding_uni));
			else
				encoding = "utf-8";

			OutputConverter* converter_ptr;
			OP_STATUS rc = OutputConverter::CreateCharConverter(encoding, &converter_ptr);
			if (OpStatus::IsError(rc))
			{
				if (OpStatus::IsMemoryError(rc))
					return ES_NO_MEMORY;
				// FIXME: should signal a "unsupported-encoding" error.
				return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
			}
			converter.reset(converter_ptr);
#else // DOM_HTTP_SUPPORT
			return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
#endif // DOM_HTTP_SUPPORT
		}
		else
		{
			DOM_CHECK_ARGUMENTS("o");
			converter = NULL;
		}

		DOM_ARGUMENT_OBJECT_EXISTING(node, 0, DOM_TYPE_NODE, DOM_Node);

		if (!node->OriginCheck(origining_runtime))
			return ES_EXCEPT_SECURITY;

#ifdef DOM_HTTP_SUPPORT
		if (uri.CStr() && !uni_strni_eq(uri.CStr(), "HTTP:", 5))
			return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
#endif // DOM_HTTP_SUPPORT

		DOM_LSSerializer_State *state = OP_NEW(DOM_LSSerializer_State, (serializer, node, origining_runtime));
		CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state, origining_runtime, origining_runtime->GetObjectPrototype(), "Object"));

#ifdef DOM_HTTP_SUPPORT
		state->SetConverter(converter.release());
		state->SetURI(uri);
		state->SetHTTPOutputStream(httpOutputStream);
#endif // DOM_HTTP_SUPPORT

		return state->Serialize(return_value);
	}
	else
	{
		DOM_LSSerializer_State *state = DOM_VALUE2OBJECT(*return_value, DOM_LSSerializer_State);
		return state->Serialize(return_value);
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_WITH_DATA_START(DOM_LSSerializer)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_LSSerializer, DOM_LSSerializer::write, 0, "write", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_LSSerializer, DOM_LSSerializer::write, 1, "writeToURI", "-s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_LSSerializer, DOM_LSSerializer::write, 2, "writeToString", 0)
DOM_FUNCTIONS_WITH_DATA_END(DOM_LSSerializer)

#endif // DOM3_SAVE
