/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef DOM_HTTP_SUPPORT
#include "modules/dom/src/opera/domformdata.h"

#include "modules/dom/src/domhtml/htmlelem.h"
#include "modules/dom/domtypes.h"
#include "modules/upload/upload.h"
#include "modules/forms/formtraversal.h"

#include "modules/dom/src/domfile/domblob.h"
#include "modules/dom/src/domfile/domfile.h"
#include "modules/dom/src/domfile/domfilereader.h"

/* static */ OP_STATUS
DOM_FormData::Make(DOM_FormData *&formdata, DOM_HTMLFormElement *form, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(formdata = OP_NEW(DOM_FormData, ()), runtime, runtime->GetPrototype(DOM_Runtime::FORMDATA_PROTOTYPE), "FormData"));
	if (form)
		return formdata->PopulateFormData(form, runtime);

	return OpStatus::OK;
}

/* virtual */
DOM_FormData::~DOM_FormData()
{
	value_pairs.Clear();
}

/* virtual */ void
DOM_FormData::GCTrace()
{
	for (NameValue *pair = value_pairs.First(); pair; pair = pair->Suc())
		pair->GCTrace();
}

class DOM_FormDataRestartObject
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_FormDataRestartObject *&restart_object, const uni_char *name, DOM_Blob *blob, const uni_char *filename, DOM_FileReader *reader, DOM_Runtime *runtime);

	DOM_FormDataRestartObject(DOM_Blob *blob, DOM_FileReader *reader)
		: blob(blob),
		  reader(reader)
	{
	}

	virtual void GCTrace()
	{
		GCMark(blob);
		GCMark(reader);
	}

	OpString name;
	DOM_Blob *blob;
	OpString filename;
	DOM_FileReader *reader;
};

/* static */ OP_STATUS
DOM_FormDataRestartObject::Make(DOM_FormDataRestartObject *&restart_object, const uni_char *name, DOM_Blob *blob, const uni_char *filename, DOM_FileReader *reader, DOM_Runtime *runtime)
{
	DOM_FormDataRestartObject *instance;
	RETURN_IF_ERROR(DOMSetObjectRuntime(instance = OP_NEW(DOM_FormDataRestartObject, (blob, reader)), runtime));
	RETURN_IF_ERROR(instance->name.Set(name));
	RETURN_IF_ERROR(instance->filename.Set(filename));

	restart_object = instance;
	return OpStatus::OK;
}

/* static */ int
DOM_FormData::append(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(formdata, DOM_TYPE_FORMDATA, DOM_FormData);
	if (argc < 0)
	{
		DOM_FormDataRestartObject *restart = static_cast<DOM_FormDataRestartObject *>(ES_Runtime::GetHostObject(return_value->value.object));
		DOM_Object *reader = restart->reader;
		OP_ASSERT(reader && reader->IsA(DOM_TYPE_FILEREADERSYNC));
		const unsigned char *byte_data;
		unsigned byte_length = 0;
		int result = DOM_FileReader::readBlob(reader, NULL, byte_data, byte_length, return_value, origining_runtime);
		if (result == ES_VALUE)
		{
			if (return_value->type == VALUE_OBJECT && ES_Runtime::IsNativeArrayBufferObject(return_value->value.object))
			{
				byte_data = ES_Runtime::GetArrayBufferStorage(return_value->value.object);
				byte_length = ES_Runtime::GetArrayBufferLength(return_value->value.object);
			}
			OP_ASSERT(byte_data || byte_length == 0);
			CALL_FAILED_IF_ERROR(formdata->Append(restart->name.CStr(), NULL, restart->blob, restart->filename.CStr(), byte_data, byte_length));
			return ES_FAILED;
		}
		else
			return result;
	}

	DOM_CHECK_ARGUMENTS("s-");

	DOM_Blob *blob = NULL;
	const uni_char *filename = UNI_L("blob");
	const uni_char *value = NULL;
	OpString filename_string;

	/* Overload resolution for append():

	   FormData append(DOMString name, DOMString value);
	   FormData append(DOMString name, Blob value_blob, optional DOMString filename);

	   Determine if 2nd argument is a blob or something (possibly)
	   needing string conversion: */
	DOM_Object *host_object;
	if (argv[1].type == VALUE_OBJECT && (host_object = DOM_HOSTOBJECT(argv[1].value.object, DOM_Object)) && host_object->IsA(DOM_TYPE_BLOB))
		blob = static_cast<DOM_Blob *>(host_object);
	else if (argv[1].type != VALUE_STRING)
	{
		DOMSetNumber(return_value, ES_CONVERT_ARGUMENT(ES_CALL_NEEDS_STRING, 1));
		return ES_NEEDS_CONVERSION;
	}
	else
	{
		OP_ASSERT(argv[1].type == VALUE_STRING);
		value = argv[1].value.string;
	}

	if (blob)
	{
		if (argc > 2 && argv[2].type != VALUE_STRING)
		{
			DOMSetNumber(return_value, ES_CONVERT_ARGUMENT(ES_CALL_NEEDS_STRING, 2));
			return ES_NEEDS_CONVERSION;
		}
		else if (argc > 2)
		{
			OP_ASSERT(argv[2].type == VALUE_STRING);
			filename = argv[2].value.string;
		}
		else if (blob->IsA(DOM_TYPE_FILE) && !blob->IsSlice())
		{
#ifdef _FILE_UPLOAD_SUPPORT_
			DOM_File *file = static_cast<DOM_File *>(blob);
			RETURN_IF_ERROR(file->GetFilename(filename_string));
			if (!filename_string.IsEmpty())
				filename = filename_string.CStr();
#else
			return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
#endif // _FILE_UPLOAD_SUPPORT_
		}

		if (blob->IsA(DOM_TYPE_BLOB) && (!blob->IsA(DOM_TYPE_FILE) || blob->IsSlice()))
		{
			DOM_Object *reader;
			const unsigned char *byte_data;
			unsigned byte_length = 0;
			int result = DOM_FileReader::readBlob(reader, blob, byte_data, byte_length, return_value, origining_runtime);
			if (result & ES_SUSPEND)
			{
				DOM_FormDataRestartObject *restarter;
				CALL_FAILED_IF_ERROR(DOM_FormDataRestartObject::Make(restarter, argv[0].value.string, blob, filename, static_cast<DOM_FileReader *>(reader), origining_runtime));
				DOMSetObject(return_value, restarter);
				return result;
			}
			else if (result != ES_VALUE)
				return result;
			else
			{
				if (return_value->type == VALUE_OBJECT && ES_Runtime::IsNativeArrayBufferObject(return_value->value.object))
				{
					byte_data = ES_Runtime::GetArrayBufferStorage(return_value->value.object);
					byte_length = ES_Runtime::GetArrayBufferLength(return_value->value.object);
				}
				OP_ASSERT(byte_data || byte_length == 0);
				CALL_FAILED_IF_ERROR(formdata->Append(argv[0].value.string, value, blob, filename, byte_data, byte_length));
				return ES_FAILED;
			}
		}
	}
	CALL_FAILED_IF_ERROR(formdata->Append(argv[0].value.string, value, blob, filename, NULL, 0));

	return ES_FAILED;
}

OP_STATUS
DOM_FormData::Append(const uni_char *name, const uni_char *value, DOM_Blob *blob, const uni_char *filename, const unsigned char *byte_data, unsigned byte_length)
{
	NameValue *pair = NULL;
	if (blob)
	{
		NameValueBlob *pair_blob = OP_NEW(NameValueBlob, ());
		RETURN_OOM_IF_NULL(pair_blob);
		OpAutoPtr<NameValueBlob> anchor(pair_blob);

		RETURN_IF_ERROR(pair_blob->name.Set(name));
		pair_blob->blob = blob;
		RETURN_IF_ERROR(pair_blob->filename.Set(filename));
		if (byte_length > 0)
		{
			RETURN_OOM_IF_NULL(pair_blob->byte_data = OP_NEWA(unsigned char, byte_length));
			op_memcpy(pair_blob->byte_data, byte_data, byte_length);
			pair_blob->byte_length = byte_length;
		}
		pair = anchor.release();
	}
	else
	{
		NameValueString *pair_string = OP_NEW(NameValueString, ());
		RETURN_OOM_IF_NULL(pair_string);
		OpAutoPtr<NameValueString> anchor(pair_string);

		RETURN_IF_ERROR(pair_string->name.Set(name));
		RETURN_IF_ERROR(pair_string->string.Set(value));
		pair = anchor.release();
	}

	pair->Into(&value_pairs);
	return OpStatus::OK;
}

OP_STATUS
DOM_FormData::Append(const char *name8, const char *value8, DOM_Blob *blob, const char *filename8)
{
	NameValue *pair = NULL;
	if (blob)
	{
		NameValueBlob *pair_blob = OP_NEW(NameValueBlob, ());
		RETURN_OOM_IF_NULL(pair_blob);
		OpAutoPtr<NameValueBlob> anchor(pair_blob);

		RETURN_IF_ERROR(pair_blob->name.SetFromUTF8(name8));
		pair_blob->blob = blob;
		RETURN_IF_ERROR(pair_blob->filename.SetFromUTF8(filename8));
		pair = anchor.release();
	}
	else
	{
		NameValueString *pair_string = OP_NEW(NameValueString, ());
		RETURN_OOM_IF_NULL(pair_string);
		OpAutoPtr<NameValueString> anchor(pair_string);

		RETURN_IF_ERROR(pair_string->name.SetFromUTF8(name8));
		RETURN_IF_ERROR(pair_string->string.SetFromUTF8(value8));
		pair = anchor.release();
	}

	pair->Into(&value_pairs);
	return OpStatus::OK;
}


#ifdef SELFTEST
/* NOTE: this host method provides an expedient way to get a
   handle on what XHR.send(FormData) will output as a multipart/form-data.
   Its use is _only_ intended for selftests which can then check
   for well-formedness of such output. Its implementation does
   not lend itself to any productive uses beyond selftests, and
   should not be used. */

/* static */ int
DOM_FormData::toUploadString(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(formdata, DOM_TYPE_FORMDATA, DOM_FormData);

	Upload_Multipart *multipart;
	CALL_FAILED_IF_ERROR(formdata->Upload(multipart, "multipart/form-data", TRUE));
	OpAutoPtr<Upload_Multipart> anchor_multipart(multipart);

	TempBuffer *buf = GetEmptyTempBuf();
	BOOL done = FALSE;
	while (!done)
	{
		unsigned char output[4096]; // ARRAY OK sof 2011-10-17
		unsigned written = multipart->GetOutputData(output, sizeof(output), done);
		OpString str;
		CALL_FAILED_IF_ERROR(str.SetFromUTF8(reinterpret_cast<char *>(output), written));
		CALL_FAILED_IF_ERROR(buf->Append(str.CStr()));
	}
	DOMSetString(return_value, buf);
	return ES_VALUE;
}
#endif // SELFTEST

OP_STATUS
DOM_FormData::Upload(Upload_Multipart *&result, const char *mime_type, BOOL as_standalone)
{
	TRAPD(status, UploadL(result, mime_type, as_standalone));
	return status;
}

/* private */ void
DOM_FormData::UploadL(Upload_Multipart *&result, const char *mime_type, BOOL as_standalone)
{
	Upload_Multipart *multipart = OP_NEW_L(Upload_Multipart, ());
	OpStackAutoPtr<Upload_Multipart> anchor_multipart(multipart);

	if (!mime_type)
		mime_type = "multipart/form-data";
	multipart->InitL(mime_type);
	UploadFormL(multipart);

	Boundary_List boundaries;
	boundaries.InitL();
	multipart->PrepareL(boundaries, UPLOAD_BINARY_NO_CONVERSION);
	if (as_standalone)
		multipart->ResetL();
	result = anchor_multipart.release();
}

/* private */ void
DOM_FormData::UploadFormL(Upload_Multipart *multipart)
{
	for (NameValue *pair = value_pairs.First(); pair; pair = pair->Suc())
	{
		if (pair->is_string_value)
		{
			NameValueString *pair_string = static_cast<NameValueString *>(pair);
			OpStackAutoPtr<Upload_OpString8> element(OP_NEW_L(Upload_OpString8, ()));
			OpString8 string8;
			LEAVE_IF_ERROR(string8.SetUTF8FromUTF16(pair_string->string));
			element->InitL(string8, NULL, NULL);
			element->SetContentDispositionL("form-data");
			LEAVE_IF_ERROR(string8.SetUTF8FromUTF16(pair_string->name));
			element->SetContentDispositionParameterL("name", string8, TRUE);

			multipart->AddElement(element.release());
		}
		else
		{
			NameValueBlob *pair_blob = static_cast<NameValueBlob *>(pair);
			Upload_Handler *element = NULL;
			if (pair_blob->blob->IsA(DOM_TYPE_FILE) && !pair_blob->blob->IsSlice())
			{
				DOM_File *file = static_cast<DOM_File *>(pair_blob->blob);
				file->UploadL(element, pair_blob->name.CStr(), pair_blob->filename.CStr(), NULL, FALSE);
				if (element)
					multipart->AddElement(element);
			}
			else
			{
				Upload_BinaryBuffer *upload = OP_NEW_L(Upload_BinaryBuffer, ());
				OpStackAutoPtr<Upload_BinaryBuffer> upload_anchor(upload);

				upload->SetContentDispositionL("form-data");
				OpString8 string8;
				LEAVE_IF_ERROR(string8.SetUTF8FromUTF16(pair_blob->name));
				upload->SetContentDispositionParameterL("name", string8, TRUE);
				LEAVE_IF_ERROR(string8.SetUTF8FromUTF16(pair_blob->filename));
				upload->SetContentDispositionParameterL("filename", string8, TRUE);
				upload->InitL(pair_blob->byte_data, pair_blob->byte_length, UPLOAD_COPY_BUFFER, "application/octet-stream", 0, ENCODING_NONE);
				upload->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION);

				multipart->AddElement(upload_anchor.release());
			}
		}
	}
}

class FormDataTraversalContext
	: public FormTraversalContext
{
public:
	FormDataTraversalContext(FramesDocument *frames_doc, DOM_FormData *formdata, DOM_Runtime *runtime)
		: FormTraversalContext(frames_doc, NULL, NULL)
		, formdata(formdata)
		, runtime(runtime)
	{
	}

	DOM_FormData *formdata;
	DOM_Runtime *runtime;
};

class FormDataTraverser
	: public FormTraverser
{
public:
	virtual void ToFieldValueL(OpString8 &name, const uni_char *name_str)
	{
		name.SetUTF8FromUTF16L(name_str);
	}

	virtual void AppendNameValuePairL(FormTraversalContext &context, HTML_Element *he, const char *name8, const char *value8, BOOL verbatim = FALSE)
	{
		FormDataTraversalContext &formdata_context = static_cast<FormDataTraversalContext &>(context);
		LEAVE_IF_ERROR(formdata_context.formdata->Append(name8, value8 ? value8 : "", NULL, NULL));
	}

	virtual void AppendUploadFilenameL(FormTraversalContext &context, HTML_Element *he, const char *name8, const uni_char *filename, const char *encoded_filename8)
	{
		FormDataTraversalContext &formdata_context = static_cast<FormDataTraversalContext &>(context);

		DOM_File *file;
		LEAVE_IF_ERROR(DOM_File::Make(file, filename ? filename : UNI_L(""), FALSE, FALSE, formdata_context.runtime));
		LEAVE_IF_ERROR(formdata_context.formdata->Append(name8, NULL, file, encoded_filename8));
	}
};

static OP_STATUS
TraverseForm(FormDataTraversalContext &context, HTML_Element *form)
{
	FormDataTraverser traverser;
	TRAPD(status, FormTraversal::TraverseL(context, &traverser, form));
	return status;
}

OP_STATUS
DOM_FormData::PopulateFormData(DOM_HTMLFormElement *arg, DOM_Runtime *runtime)
{
	if (FramesDocument *frames_doc = arg->GetFramesDocument())
	{
		FormDataTraversalContext context(frames_doc, this, runtime);
		RETURN_IF_ERROR(TraverseForm(context, arg->GetThisElement()));
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_FormData::Clone(DOM_FormData *source_object, ES_Runtime *target_runtime, DOM_FormData *&target_object)
{
	RETURN_IF_ERROR(DOM_FormData::Make(target_object, NULL, static_cast<DOM_Runtime *>(target_runtime)));
	for (NameValue *entry = source_object->value_pairs.First(); entry; entry = entry->Suc())
	{
		NameValue *copy = NULL;
		if (entry->is_string_value)
			RETURN_IF_ERROR(NameValueString::Clone(entry, copy));
		else
			RETURN_IF_ERROR(NameValueBlob::Clone(entry, target_runtime, copy));
		copy->Into(&target_object->value_pairs);
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_FormData::NameValueString::Clone(NameValue *source_object, NameValue *&target_object)
{
	NameValueString *source = static_cast<NameValueString *>(source_object);
	NameValueString *o = OP_NEW(NameValueString, ());
	RETURN_OOM_IF_NULL(o);
	OpAutoPtr<NameValueString> anchor_copy(o);
	RETURN_IF_ERROR(o->name.Set(source->name.CStr()));
	RETURN_IF_ERROR(o->string.Set(source->string.CStr()));
	target_object = anchor_copy.release();

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_FormData::NameValueBlob::Clone(NameValue *source_object, ES_Runtime *target_runtime, NameValue *&target_object)
{
	NameValueBlob *source = static_cast<NameValueBlob *>(source_object);
	NameValueBlob *o = OP_NEW(NameValueBlob, ());

	RETURN_OOM_IF_NULL(o);
	OpAutoPtr<NameValueBlob> anchor_copy(o);
	RETURN_IF_ERROR(o->name.Set(source->name.CStr()));
	RETURN_IF_ERROR(o->filename.Set(source->filename.CStr()));
	DOM_Blob *target_blob = NULL;
	RETURN_IF_ERROR(DOM_Blob::Clone(source->blob, target_runtime, target_blob));
	o->blob = target_blob;
	if (source->byte_length > 0)
	{
		o->byte_data = OP_NEWA(unsigned char, source->byte_length);
		RETURN_OOM_IF_NULL(o->byte_data);
		op_memcpy(o->byte_data, source->byte_data, source->byte_length);
		o->byte_length = source->byte_length;
	}
	target_object = anchor_copy.release();

	return OpStatus::OK;
}

#ifdef ES_PERSISTENT_SUPPORT
class NameValueBlobPersistent
	: public DOM_FormData::NameValue
{
public:
	NameValueBlobPersistent()
		: DOM_FormData::NameValue(FALSE),
		  byte_data(NULL),
		  byte_length(0)
	{
	}

	virtual ~NameValueBlobPersistent()
	{
	    OP_DELETE(blob_persistent);
		OP_DELETEA(byte_data);
	}

	static OP_STATUS Clone(NameValue *source_object, ES_Runtime *runtime, NameValue *&target_object);

	virtual void GCTrace()
	{
	}

	ES_PersistentItem *blob_persistent;
	OpString filename;
	unsigned char *byte_data;
	unsigned byte_length;
};

/* static */ OP_STATUS
DOM_FormData::NameValueBlob::Clone(NameValue *source_object, NameValue *&target_object)
{
	NameValueBlob *source = static_cast<NameValueBlob *>(source_object);
	NameValueBlobPersistent *o = OP_NEW(NameValueBlobPersistent, ());

	RETURN_OOM_IF_NULL(o);
	OpAutoPtr<NameValueBlobPersistent> anchor_copy(o);
	RETURN_IF_ERROR(o->name.Set(source->name.CStr()));
	RETURN_IF_ERROR(o->filename.Set(source->filename.CStr()));
	ES_PersistentItem *target_item = NULL;
	RETURN_IF_ERROR(DOM_Blob::Clone(source->blob, target_item));
	o->blob_persistent = target_item;
	if (source->byte_length > 0)
	{
		o->byte_data = OP_NEWA(unsigned char, source->byte_length);
		RETURN_OOM_IF_NULL(o->byte_data);
		op_memcpy(o->byte_data, source->byte_data, source->byte_length);
		o->byte_length = source->byte_length;
	}

	target_object = anchor_copy.release();
	return OpStatus::OK;
}

class DOM_FormData_PersistentItem
	: public ES_PersistentItem
{
public:
	virtual ~DOM_FormData_PersistentItem()
	{
	}

	virtual BOOL IsA(int tag) { return tag == DOM_TYPE_FORMDATA; }

	List<DOM_FormData::NameValue> value_pairs;
};

/* static */ OP_STATUS
DOM_FormData::Clone(DOM_FormData *source_object, ES_PersistentItem *&target_item)
{
	DOM_FormData_PersistentItem *item = OP_NEW(DOM_FormData_PersistentItem, ());
	RETURN_OOM_IF_NULL(item);
	for (NameValue *entry = source_object->value_pairs.First(); entry; entry = entry->Suc())
	{
		NameValue *copy = NULL;
		if (entry->is_string_value)
			RETURN_IF_ERROR(NameValueString::Clone(entry, copy));
		else
			RETURN_IF_ERROR(NameValueBlob::Clone(entry, copy));
		copy->Into(&item->value_pairs);
	}

	target_item = item;
	return OpStatus::OK;
}

/* static */ OP_STATUS
NameValueBlobPersistent::Clone(NameValue *source_object, ES_Runtime *runtime, NameValue *&target_object)
{
	NameValueBlobPersistent *source = static_cast<NameValueBlobPersistent *>(source_object);
	DOM_FormData::NameValueBlob *o = OP_NEW(DOM_FormData::NameValueBlob, ());

	RETURN_OOM_IF_NULL(o);
	OpAutoPtr<DOM_FormData::NameValueBlob> anchor_copy(o);
	RETURN_IF_ERROR(o->name.Set(source->name.CStr()));
	RETURN_IF_ERROR(o->filename.Set(source->filename.CStr()));
	DOM_Blob *target_file = NULL;
	RETURN_IF_ERROR(DOM_Blob::Clone(source->blob_persistent, runtime, target_file));
	o->blob = target_file;
	if (source->byte_length > 0)
	{
		o->byte_data = OP_NEWA(unsigned char, source->byte_length);
		RETURN_OOM_IF_NULL(o->byte_data);
		op_memcpy(o->byte_data, source->byte_data, source->byte_length);
		o->byte_length = source->byte_length;
	}
	target_object = anchor_copy.release();

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_FormData::Clone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_FormData *&target_object)
{
	DOM_FormData_PersistentItem *source_item = static_cast<DOM_FormData_PersistentItem *>(source_item0);
	RETURN_IF_ERROR(DOM_FormData::Make(target_object, NULL, static_cast<DOM_Runtime *>(target_runtime)));
	for (NameValue *entry = source_item->value_pairs.First(); entry; entry = entry->Suc())
	{
		NameValue *copy = NULL;
		if (entry->is_string_value)
			RETURN_IF_ERROR(NameValueString::Clone(entry, copy));
		else
			RETURN_IF_ERROR(NameValueBlobPersistent::Clone(entry, target_runtime, copy));
		copy->Into(&target_object->value_pairs);
	}
	return OpStatus::OK;
}
#endif // ES_PERSISTENT_SUPPORT

/* virtual */ int
DOM_FormData_Constructor::Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_FormData *formdata;
	DOM_HTMLFormElement *arg = NULL;
	if (argc > 0)
	{
		if (argv[0].type != VALUE_OBJECT)
			return CallInternalException(DOM_Object::WRONG_ARGUMENTS_ERR, return_value);

		DOM_Object *form_object = DOM_HOSTOBJECT(argv[0].value.object, DOM_Object);
		if (!form_object || !form_object->IsA(DOM_TYPE_HTML_FORMELEMENT))
			return CallInternalException(DOM_Object::WRONG_ARGUMENTS_ERR, return_value);
		arg = static_cast<DOM_HTMLFormElement *>(form_object);
	}
	CALL_FAILED_IF_ERROR(DOM_FormData::Make(formdata, arg, static_cast<DOM_Runtime *>(origining_runtime)));
	DOMSetObject(return_value, formdata);
	return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_FormData)
	DOM_FUNCTIONS_FUNCTION(DOM_FormData, DOM_FormData::append, "append", "s-")
#ifdef SELFTEST
	DOM_FUNCTIONS_FUNCTION(DOM_FormData, DOM_FormData::toUploadString, "toUploadString", NULL)
#endif // SELFTEST
DOM_FUNCTIONS_END(DOM_FormData)

#endif // DOM_HTTP_SUPPORT
