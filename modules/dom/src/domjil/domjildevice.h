/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILDEVICE_H
#define DOM_DOMJILDEVICE_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/dom/src/domasynccallback.h"
#include "modules/device_api/jil/JILFileFinder.h"

#ifdef USE_OP_CLIPBOARD
#include "modules/dragdrop/clipboard_manager.h"
#endif // USE_OP_CLIPBOARD

class DOM_JILAccount;
class DOM_JILRadioInfo;
class DOM_JILFile_Constructor;

class DOM_JILDevice : public DOM_JILObject
#ifdef USE_OP_CLIPBOARD
	, public ClipboardListener
#endif // USE_OP_CLIPBOARD
{
public:
	virtual ~DOM_JILDevice() {}
	static OP_STATUS Make(DOM_JILDevice*& device, DOM_Runtime* runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_DEVICE || DOM_Object::IsA(type); }
	virtual void GCTrace();

	enum { FUNCTIONS_ARRAY_SIZE = 14 };

	DOM_DECLARE_FUNCTION(getAvailableApplications);
	DOM_DECLARE_FUNCTION(getDirectoryFileNames);
	DOM_DECLARE_FUNCTION(getFile);
	DOM_DECLARE_FUNCTION(launchApplication);
	DOM_DECLARE_FUNCTION(copyFile);
	DOM_DECLARE_FUNCTION(moveFile);
	DOM_DECLARE_FUNCTION(deleteFile);
	DOM_DECLARE_FUNCTION(findFiles);
	DOM_DECLARE_FUNCTION(getFileSystemRoots);
	DOM_DECLARE_FUNCTION(getFileSystemSize);
	DOM_DECLARE_FUNCTION(setRingtone);
	DOM_DECLARE_FUNCTION(vibrate);

#ifdef USE_OP_CLIPBOARD
	void OnCut(OpClipboard* clipboard) {}
	void OnCopy(OpClipboard* clipboard);
	void OnPaste(OpClipboard* clipboard);
#endif // USE_OP_CLIPBOARD

protected:
	DOM_JILDevice() : m_on_files_found(NULL) {}
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);

private:
	class JILDeviceFilesFoundListenerAsync : public FilesFoundListener, public DOM_AsyncCallback
	{
	public:
		struct SearchParams
		{
			OpFileInfo *ref_info;
			uni_char *path;
			uni_char *pattern;
			unsigned int start_index;
			unsigned int end_index;
			SearchParams()
			: ref_info(NULL)
			, path(NULL)
			, pattern(NULL)
			, start_index(0)
			, end_index(-1)
			{}

			~SearchParams();
		};

		JILDeviceFilesFoundListenerAsync(DOM_Runtime* runtime, ES_Object* this_object, ES_Object* es_callback, OpAutoPtr<SearchParams> params);

		~JILDeviceFilesFoundListenerAsync()
		{
			OP_DELETE(m_finder);
		}

		OP_STATUS Construct();
		void Start();
		virtual void OnFilesFound(OpVector<OpString> *results);
		virtual void OnError(OP_STATUS error);
		virtual void OnFinished();

	private:
		DOM_AutoProtectPtr m_results_array_protector;
		OpAutoPtr<SearchParams> m_search_params;
		JILFileFinder *m_finder;
		DOM_Runtime *m_runtime;
	};

	friend class JILDeviceFilesFoundListenerAsync;

	/** Adds URIs for all installed gadgets to the array, JIL-style.
	 *
	 * The URIs are added in form of:
	 * widget://{wuid}?wname={widget_name}
	 *
	 * If a widget doesn't have a name, that part is empty, but the
	 * "?wname=" substring is still there (it's not specified by JIL).
	 *
	 * Note: This function won't cope well with very large arrays so
	 * make sure it doesn't get an array from the script.
	 *
	 * @param result_array An EcmaScript array object to which entries will be added.
	 * @param runtime The runtime that has been used to create result_array.
	 */
	static OP_STATUS AddInstalledWidgetIds(ES_Object* result_array, DOM_Runtime* runtime);

	static int HandleApplicationError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* origining_runtime);
	static int HandleFileOperationError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* origining_runtime);
	static int HandleDeleteFileOperationError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* origining_runtime);
	ES_Object *m_on_files_found;
#ifdef USE_OP_CLIPBOARD
	OpString m_clipboard_string;
#endif // USE_OP_CLIPBOARD
};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILDEVICE_H
