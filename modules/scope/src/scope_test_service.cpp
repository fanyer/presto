/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Test class for the new service API in STP/1.
** This class is only used by the selftest scope.service_api and should
** only compile when the selftest is enabled.
**
** Jan Borsodi
*/

#include "core/pch.h"

#if defined(SCOPE_SUPPORT) && defined(SELFTEST)

#include "modules/scope/src/scope_test_service.h"
#include "modules/scope/scope_element_inspector.h"
#include "modules/style/css_dom.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/doc/frm_doc.h"

/* Manual implementation */

OP_STATUS
OtScopeTestService::Clear()
{
	data_value = 0;
	data_name.Empty();
	event_id = 0;

	return OpStatus::OK;
}

OP_STATUS OtScopeTestService::DoGetData(OutData &out)
{
	out.SetRuntimeID(data_value);
	out.SetName(data_name);
	return OpStatus::OK;
}

OP_STATUS OtScopeTestService::DoSetData(const InData &in)
{
	data_value = in.GetRuntimeID();
	data_name.Set(in.GetTitle());
	return OpStatus::OK;
}

OP_STATUS OtScopeTestService::DoAdd(const InAdd &in, OutAdd &out)
{
	const OpValueVector<UINT32> &values = in.GetValueList();
	unsigned int result = 0;
	for (unsigned int i = 0; i < values.GetCount(); ++i)
	{
		result += values.Get(i);
	}
	out.SetResult(result);
	return OpStatus::OK;
}

OP_STATUS OtScopeTestService::DoNop()
{
	return OpStatus::OK;
}

OP_STATUS OtScopeTestService::DoGetRepeatedData(const GetRepeatedDataArg &in, RepeatedData &out)
{
	for(unsigned i = 0; i<in.GetIntegers(); ++i)
		out.GetIntegerListRef().Add(1984);

	for(unsigned i = 0; i<in.GetBooleans(); ++i)
		out.GetBooleanListRef().Add(TRUE);

	for(unsigned i = 0; i<in.GetStrings(); ++i)
	{
		OpAutoPtr<OpString> str(OP_NEW(OpString, ()));
		RETURN_OOM_IF_NULL(str.get());
		RETURN_IF_ERROR(str->Set(UNI_L("Failure is not an option. Everyone has to succeed.")));
		out.GetStringListRef().Add(str.release());
	}

	for(unsigned i = 0; i<in.GetNested(); ++i)
	{
		OpAutoPtr<RepeatedData::Nested> nested(OP_NEW(RepeatedData::Nested, ()));
		RETURN_OOM_IF_NULL(nested.get());

		for(unsigned j = 0; j<in.GetNested(); ++j)
		{
			OpAutoPtr<RepeatedData::Nested::MoreNested> more_nested(OP_NEW(RepeatedData::Nested::MoreNested, ()));
			RETURN_OOM_IF_NULL(more_nested.get());
			more_nested->SetA(1984);
			more_nested->SetB(1984);
			more_nested->SetC(1984);
			more_nested->SetD(1984);
			RETURN_IF_ERROR(nested->GetMoreNestedListRef().Add(more_nested.release()));
		}

		RETURN_IF_ERROR(out.GetNestedListRef().Add(nested.release()));
	}

	return OpStatus::OK;
}

OP_STATUS
OtScopeTestService::DoAsyncCommand(const AsyncArgs &in, unsigned async_tag)
{
	if (in.GetValue() == 0)
		return OpStatus::ERR;
	else if (in.GetValue() < 0) // Negative value means to send response in AsynCommandCallback(), will be called by selftest
	{
		last_async_command_tag = async_tag;
		return OpStatus::OK;
	}

	AsyncData out;
	out.SetResult(in.GetValue()*2);
	return SendAsyncCommand(out, async_tag);
}

OP_STATUS
OtScopeTestService::AsyncCommandCallback(unsigned async_tag, int value)
{
	if (value == 0)
	{
		RETURN_IF_ERROR(SendAsyncError(async_tag, Command_AsyncCommand, OpStatus::ERR_PARSING_FAILED)); // Should trigger error response "Parsing failed while executing command"
		return OpStatus::OK;
	}
	AsyncData data;
	data.SetResult(value);
	return SendAsyncCommand(data, async_tag);
}

OP_STATUS
OtScopeTestService::DoFormatError(const FormatErrorArg &in)
{
	switch (in.GetIndex())
	{
	case 0:
		SCOPE_BADREQUEST_IF_ERROR(OpStatus::OK, this, UNI_L("Should not be visible"));
		break;
	case 1:
		SCOPE_BADREQUEST_IF_ERROR(OpStatus::ERR_NO_MEMORY, this, UNI_L("Should not be visible"));
		break;
	case 2:
		SCOPE_BADREQUEST_IF_ERROR(OpStatus::ERR, this, UNI_L("An error ocurred"));
		break;
	case 3:
		SCOPE_BADREQUEST_IF_ERROR1(OpStatus::ERR, this, UNI_L("An error ocurred: %i"), 1337);
		break;
	case 4:
		SCOPE_BADREQUEST_IF_ERROR2(OpStatus::ERR, this, UNI_L("%s: %i"), UNI_L("An integer"), 1338);
		break;
	case 5:
		SCOPE_BADREQUEST_IF_ERROR3(OpStatus::ERR, this, UNI_L("%s: %i, %i"), UNI_L("Two integers"), 1337, 1338);
		break;
	case 6:
		SCOPE_BADREQUEST(this, UNI_L("An error ocurred"));
		break;
	case 7:
		SCOPE_BADREQUEST1(this, UNI_L("An error ocurred: %i"), 1337);
		break;
	case 8:
		SCOPE_BADREQUEST2(this, UNI_L("%s: %i"), UNI_L("An integer"), 1338);
		break;
	case 9:
		SCOPE_BADREQUEST3(this, UNI_L("%s: %i, %i"), UNI_L("Two integers"), 1337, 1338);
		break;
	default:
		SCOPE_BADREQUEST_IF_ERROR(OpStatus::ERR, this, UNI_L("Unsupported execution path"));
		break;
	}

	return OpStatus::OK;
}

OP_STATUS
OtScopeTestService::DoTestMixedStrings(const MixedStringType &in, MixedStringType &out)
{
	RETURN_IF_ERROR(out.SetType1(in.GetType1()));
	RETURN_IF_ERROR(out.SetType2(in.GetType2()));

	if (in.HasType3())
		RETURN_IF_ERROR(out.SetType3(in.GetType3()));
	if (in.HasType4())
		RETURN_IF_ERROR(out.SetType4(in.GetType4()));

	const OpAutoVector<OpString> &type5 = in.GetType5List();
	for (unsigned i = 0; i < type5.GetCount(); ++i)
	{
		RETURN_IF_ERROR(out.AppendToType5List(*type5.Get(i)));
	}

	const OpAutoVector<TempBuffer> &type6 = in.GetType6List();
	for (unsigned i = 0; i < type6.GetCount(); ++i)
	{
		RETURN_IF_ERROR(out.AppendToType6List(*type6.Get(i)));
	}

	return OpStatus::OK;
}

OP_STATUS
OtScopeTestService::DoTestMixedBytes(const MixedByteType &in, MixedByteType &out)
{
	RETURN_IF_ERROR(out.SetType1(in.GetType1()));
	RETURN_IF_ERROR(out.SetType2(in.GetType2()));

	if (in.HasType3())
		RETURN_IF_ERROR(out.SetType3(in.GetType3()));
	if (in.HasType4())
		RETURN_IF_ERROR(out.SetType4(in.GetType4()));

	const OpAutoVector<ByteBuffer> &type5 = in.GetType5List();
	for (unsigned i = 0; i < type5.GetCount(); ++i)
	{
		RETURN_IF_ERROR(out.AppendToType5List(*type5.Get(i)));
	}

	const OpProtobufValueVector<OpData> &type6 = in.GetType6List();
	for (unsigned i = 0; i < type6.GetCount(); ++i)
	{
		RETURN_IF_ERROR(out.AppendToType6List(type6.Get(i)));
	}

	return OpStatus::OK;
}


class OtScopeQueryListener
	: public CSS_QuerySelectorListener
{
public:
	OtScopeQueryListener()
		: element(NULL)
	{
	}

	virtual OP_STATUS MatchElement(HTML_Element *element)
	{
		this->element = element;
		return OpStatus::OK;
	}

	HTML_Element *GetElement() const { return element; }
private:
	HTML_Element *element;
};

OP_STATUS
OtScopeTestService::DoInspectElement(const InspectElementArg &in)
{
#ifdef SCOPE_ECMASCRIPT_DEBUGGER
	Window *window = g_windowManager->GetWindow(in.GetWindowID());

	if (!window)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("No window with that ID"));

	FramesDocument *doc = window->GetCurrentDoc();

	if (!doc)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Window has no document"));

	HLDocProfile *profile = doc->GetHLDocProfile();
	HTML_Element *root = profile->GetRoot();

	OtScopeQueryListener listener;
	CSS_DOMException exception;

	RETURN_IF_ERROR(CSS_QuerySelector(profile, in.GetSelector().CStr(), root, 0, &listener, exception));

	if (!listener.GetElement())
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("No element matched"));

	return OpScopeElementInspector::InspectElement(doc, listener.GetElement());
#else // SCOPE_ECMASCRIPT_DEBUGGER
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SCOPE_ECMASCRIPT_DEBUGGER
}

#endif // SCOPE_SUPPORT && SELFTEST
