/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_FILELIST_H
#define DOM_FILELIST_H

#include "modules/dom/src/domobj.h"

class HTML_Element;
class DOM_File;

/**
 * The FileList interface in the W3C File API.
 */
class DOM_FileList
	: public DOM_Object
{
public:
	static OP_BOOLEAN Construct(DOM_FileList *&filelist, HTML_Element *file_input_element, OpString &file_upload_values, ES_Value &previous_file_upload_value, ES_Value &previous_filelist, DOM_Runtime *origining_runtime);
	/**< Return the FileList object for the given type=file element. The constructor
	     checks if the previous FileList instance return for that element can be used
	     and will return that cached instance, if possible.

	     @param [out]filelist The FileList result.
	     @param file_input_element The file input element to return a list for.
	     @param [out]file_upload_values The element's list of files as a string.
	     @param previous_file_upload_value If of type VALUE_STRING, the previous
	            filelist string that a DOM_FileList instance was created for this
	            element. Checked against current contents of the element to determine
	            if creating a new DOM_FileList instance is required.
	     @param previous_file_upload_value If of type VALUE_OBJECT, the previous
	            DOM_FileList object that was created for this element.
	     @param origining_runtime A runtime in which the list should be created.
	     @return OpBoolean::IS_TRUE if successful and a new DOM_FileList object
	             was constructed, OpBoolean::IS_FALSE if the previous instance
	             could be reused. OpStatus::ERR_NO_MEMORY in case of OOM. */

	static OP_STATUS Make(DOM_FileList *&filelist, const uni_char *file_upload_string, DOM_Runtime *origining_runtime);
	/**< Create a new FileList and populate it from the given type=file form value string.

	     @param [out]filelist The FileList result.
	     @param file_upload_values The file input element's list of files as a string.
	     @param origining_runtime A runtime in which the list should be created.
	     @return OpStatus::ERR_NO_MEMORY in case of OOM. OpStatus::OK otherwise. */

	static OP_STATUS Make(DOM_FileList *&filelist, DOM_Runtime *origining_runtime);
	/**< Create an empty FileList.

	     @param [out]filelist The FileList result.
	     @param origining_runtime A runtime in which the list should be created.
	     @return OpStatus::ERR_NO_MEMORY in case of OOM. OpStatus::OK otherwise. */

	OP_STATUS Add(DOM_File *file);
	/**< Append the given DOM_File to this FileList.

	     @param file The File to add.
	     @return OpStatus::OK on success; OpStatus::ERR_NO_MEMORY on OOM. */

	void Clear();
	/**< Reset the FileList to an empty state.

	     @return No result or errors. */

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_FILELIST || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	DOM_DECLARE_FUNCTION(item);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };

private:
	DOM_FileList()
	{
	}

	static OP_STATUS GetFiles(OpVector<DOM_File> &files, const uni_char *file_upload_string, DOM_Runtime *runtime);
	/**< For each filename in the input type="file" element, create a DOM_File
	     and add it to the collection. (Used to populate a DOM_FileList as well
	     as generating the FormData entries when reifying a form element as a
	     FormData.)

	     @param files The resulting vector; files added "left to right."
	     @param file_upload_values The file input element's list of files as a string.
	     @param runtime The runtime to create the file objects in.
	     @return OpStatus::OK on success; OpStatus::ERR_NO_MEMORY on OOM. */

	unsigned GetFileListCount();

	OpVector<DOM_File> m_files;
};

#endif // DOM_FILELIST_H
