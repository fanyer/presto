/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_FORMDATA_H
#define DOM_FORMDATA_H

#ifdef DOM_HTTP_SUPPORT
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domfile/domblob.h"

class Upload_Multipart;
class DOM_HTMLFormElement;

/** The FormData interface, programmatic construction of form-submitted data.

    Such data sent onwards using XMLHttpRequest2's send(FormData), which is also
    the specification that covers the FormData interface. */
class DOM_FormData
    : public DOM_Object
{
public:
	static OP_STATUS Make(DOM_FormData *&formdata, DOM_HTMLFormElement *arg, DOM_Runtime *runtime);

	virtual ~DOM_FormData();

	static OP_STATUS Clone(DOM_FormData *source_object, ES_Runtime *target_runtime, DOM_FormData *&target_object);
#ifdef ES_PERSISTENT_SUPPORT
	static OP_STATUS Clone(DOM_FormData *source_object, ES_PersistentItem *&target_item);
	static OP_STATUS Clone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_FormData *&target_object);
#endif // ES_PERSISTENT_SUPPORT

	virtual BOOL IsA(int type) { return type == DOM_TYPE_FORMDATA || DOM_Object::IsA(type); }
	virtual void GCTrace();

	OP_STATUS Upload(Upload_Multipart *&result, const char *mime_type, BOOL as_standalone);
	/**< Construct the upload data for this FormData object, as a multipart. The
	     FormData entries are added in the order of insertion, either as (name,value)
	     pairs or named blobs along with their binary data.

	     @param [out]result On success, the constructed multipart upload object.
	     @param mime_type The content type to use. If NULL, defaults to
	            multipart/form-data.
	     @param as_standalone TRUE if to be prepared for standalone use.
	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	void UploadL(Upload_Multipart *&result, const char *mime_type, BOOL as_standalone);
	/**< Construct the upload data for this FormData object, as a multipart. The
	     FormData entries are added in the order of insertion, either as (name,value)
	     pairs or named blobs along with their binary data.

	     @param [out]result The upload object result.
	     @param mime_type The content type to use, cannot be NULL.
	     @param as_standalone TRUE if to be prepared for standalone use.
	     @return Leaves with OpStatus::ERR_NO_MEMORY on OOM. */

    DOM_DECLARE_FUNCTION(append);
#ifdef SELFTEST
    DOM_DECLARE_FUNCTION(toUploadString);
	/**< Serializing the upload data as a string. Not to be
	     used outside selftests (see comment next to its definition.) */
#endif // SELFTEST

    enum {
		FUNCTIONS_append = 1,
#ifdef SELFTEST
		FUNCTIONS_toUploadString,
#endif // SELFTEST
		FUNCTIONS_ARRAY_SIZE
	};

	/** FormData objects consist of (name, value) pairs, represented internally
	    as NameValue lists. The NameValue value being either a string or an external
	    value in the form of a Blob (with an optional filename.) */
	class NameValue
		: public ListElement<NameValue>
	{
	public:
		virtual ~NameValue()
		{
		}

		virtual void GCTrace()
		{
		}

		OpString name;
		BOOL is_string_value;

	protected:
		NameValue(BOOL is_string_value)
			: is_string_value(is_string_value)
		{
		}
	};

	class NameValueString
		: public NameValue
	{
	public:
		NameValueString()
			: NameValue(TRUE)
		{
		}

		virtual ~NameValueString()
		{
		}

		static OP_STATUS Clone(NameValue *source_object, NameValue *&target_object);

		OpString string;
	};

	class NameValueBlob
		: public NameValue
	{
	public:
		NameValueBlob()
			: NameValue(FALSE),
			  byte_data(NULL),
			  byte_length(0)
		{
		}

		virtual ~NameValueBlob()
		{
			OP_DELETEA(byte_data);
		}

		static OP_STATUS Clone(NameValue *source_object, ES_Runtime *target_runtime, NameValue *&target_object);
		static OP_STATUS Clone(NameValue *source_object, NameValue *&target_object);

		virtual void GCTrace()
		{
			GCMark(blob);
		}

		DOM_Blob *blob;
		OpString filename;
		unsigned char *byte_data;
		unsigned byte_length;
	};

private:
	DOM_FormData()
	{
	}

	OP_STATUS Append(const uni_char *name, const uni_char *value, DOM_Blob *blob, const uni_char *filename, const unsigned char *byte_data, unsigned byte_length);
	/**< Internal function for extending the FormData list.

	     @param name The name to give this entry.
	     @param value If a string-valued entry, the value. If not, NULL.
	     @param blob If a blob-valued entry, the blob to include. If not, NULL.
	     @param filename The filename to associate with the blob; NULL if not a blob entry.
	     @param byte_data If non-NULL, the in-memory contents of the Blob. Owned by caller.
	     @param byte_length The length of byte_data.
	     @return OpStatus::OK on successful append; OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS Append(const char *name, const char *value, DOM_Blob *blob, const char *filename);
	/**< Internal function for extending the FormData list from
	     UTF-8 encoded names and values.

	     @param name The name to give this entry. Encoded as UTF-8.
	     @param value If a string-valued entry, the UTF-8 encoded value.
	            If not, NULL.
	     @param blob If a blob-valued entry, the blob to include.
	            If not, NULL.
	     @param filename The filename to associate with the blob;
	            NULL if not a blob entry.
	     @return OpStatus::OK on successful append;
	             OpStatus::ERR_NO_MEMORY on OOM. */

	void UploadFormL(Upload_Multipart *multipart);
	/**< Helper function for UploadL(), see it for documentation. */

	OP_STATUS PopulateFormData(DOM_HTMLFormElement *form, DOM_Runtime *runtime);
	/**< Fill FormData object with the contents of the given form element.
	     Following the rules set out by 4.10.22.4 "Constructing the form data set".

	     @param form The form element to traverse.
	     @return OpStatus::OK on having added form elements successfully.
	             OpStatus::ERR_NO_MEMORY on OOM. */

	List<NameValue> value_pairs;

	friend class FormDataTraverser;
};

class DOM_FormData_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_FormData_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::FORMDATA_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

#endif // DOM_HTTP_SUPPORT
#endif // DOM_FORMDATA_H
