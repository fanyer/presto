/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOWSOPDRAGOBJECT_H
#define WINDOWSOPDRAGOBJECT_H

#include "adjunct/desktop_pi/DesktopDragObject.h"
#include "platforms/windows/utils/sync_primitives.h"

extern CLIPFORMAT CF_HTML;
extern CLIPFORMAT CF_INETURL;
extern CLIPFORMAT CF_MOZURL;
extern CLIPFORMAT CF_INETURLW;
extern CLIPFORMAT CF_INETURLA;
extern CLIPFORMAT CF_FILECONTENTS;
extern CLIPFORMAT CF_FILEDESCRIPTOR;
extern CLIPFORMAT CF_URILIST;

/**
 * @brief Buffer with content type information.
 *
 */
struct WindowsTypedBuffer
{
	WindowsTypedBuffer() : data(NULL), length(0), content_type(NULL) {}
	~WindowsTypedBuffer()
	{
		// all are created with op_strdup
		op_free(const_cast<char*>(data));
		op_free(const_cast<char*>(content_type));
	}
	/** Content. */
	const char* data;

	/** Content length in bytes. */
	unsigned long length;

	/** Content (MIME) type of the \c data. Examples:
	 * - "text/plain" if \c data is a UTF-8 encoded string.
	 * - "text/x-opera-files" if \c data is a UTF-8 encoded string, which
	 *   contains a "\\r\\n"-separated list of file-names.
	 * - "text/uri-list" if \c data is a UTF-8 encoded string, which contains
	 *   a "\\r\\n"-separated list of URIs (see also section 5 of RFC 2483).
	 */
	const char* content_type;
};

/**
 * A class describing the drag'n'drop operation data and what is allowed
 * on drop. When dragging outside of Opera the data should be copied to the OS specific
 * drag'n'drop data structures so it may be passed to other applications.
 * When dragging from some external application the OS specific drag'n'drop data must be
 * copied to an instance of WindowsDNDData before passing to Opera.
 */
class WindowsDNDData
{
public:
	WindowsDNDData() : m_effects_allowed(DROP_UNINITIALIZED) {}
	~WindowsDNDData() { Clear(); }

	void Clear() 
	{
		WinAutoLock lock(&m_cs);

		m_data_array.DeleteAll();
	}
	OP_STATUS InitializeFromDragObject(OpDragObject *drag_object);
	OP_STATUS FillDragObjectFromData(OpDragObject *drag_object);

	/** An array of data.
	 *
	 * @note Only '\0'-terminated strings are supported as the drag'n'drop data.
	 *
	 * @see WindowsTypedBuffer
	 */
	OpAutoVector<WindowsTypedBuffer> m_data_array;

	/** Allowed drop effects as a value consisting of ORed GogiDNDDropType values.
	 * It drives what may be done on drop. E.g. if it's DROP_COPY the target will copy the data if possible.
	 * If it's DROP_MOVE | DROP_COPY the target may decide if data will be either copied or moved if possible.
	 *
	 * @see DropType
	 */
	unsigned			m_effects_allowed;

	// We access this data from multiple threads, synchronize using this
	OpCriticalSection	m_cs;
};

class WindowsOpDragObject : public DesktopDragObject
{
private:
	WindowsOpDragObject();

public:
	WindowsOpDragObject(OpTypedObject::Type type);
	~WindowsOpDragObject();

	virtual DropType	GetSuggestedDropType() const;

private:

};

/**
 * Copies strings from and to clipboard format.
 **/
class WindowsClipboardUtil
{
public:
	static char* HtmlStringToClipboardFormat(const char* html_string, const char* page_url, unsigned int len);
	//< Wrapps html_string into platform's CF_HTML data type.

	static char* GetHtmlString(HGLOBAL in);
	//< Gets html string from CF_HTML data type.
};

/**
 * Data from external application can be dropped on Opera as target
 * application.
 **/
class WindowsDropTarget : public IDropTarget
{
public:
	WindowsDropTarget(HWND target_hwnd);
	virtual ~WindowsDropTarget();
	void SetExternal(BOOL external) { m_drag_from_external_application = external; }

	// Implements platform's IDropTarget.
	BOOL QueryDrop(DWORD keyboard_state, DWORD* effect);
	HRESULT WINAPI QueryInterface(const IID& riid, void** void_object);
	ULONG WINAPI AddRef(void);
	ULONG WINAPI Release(void);
	HRESULT WINAPI DragEnter(IDataObject* drag_data, DWORD keyboard_state, POINTL lpt, DWORD* effect);
	HRESULT WINAPI DragOver(DWORD keyboard_state, POINTL pt, DWORD* effect);
	HRESULT WINAPI DragLeave();
	HRESULT WINAPI Drop(IDataObject* drag_data, DWORD keyboard_state, POINTL pt, DWORD* effect);

	static void DispatchDragOperation(HWND target_window, UINT message);

private:
	OP_STATUS AddSupportedFormat(CLIPFORMAT cf_format, /*DVTARGETDEVICE *ptd,*/ DWORD dw_aspect, /*LONG lindex,*/ DWORD tymed);
	OP_STATUS GetWindowsDNDData(IDataObject* drag_data, STGMEDIUM& medium, WindowsDNDData& data);

	/**
	 * Used to omit starting d'n'd when hovering non-client area and to omit
	 * calling DragEnter multiple times.
	 */
	void CallDragEnter();
	OP_STATUS FillWindowsDragDataFromDataObject(IDataObject* drag_data, STGMEDIUM& medium);

	int m_ref_count;
	FORMATETC* m_supported_format;
	BOOL m_allow_drop;
	BOOL m_drag_from_external_application;
	BOOL m_call_drag_enter;
	BOOL m_has_files;
	OpVector<FORMATETC> m_formatetc;
	interface IDropTargetHelper *m_drop_target_helper;
	HWND m_target_native_window;
};

/**
 * Opera can be a source of dnd data for platform dnd.
 **/
class WindowsDropSource : public IDropSource
{
public:
	WindowsDropSource();
	virtual ~WindowsDropSource() {}

	// Implements IDropSource.
	HRESULT WINAPI QueryInterface(const IID& riid, void** void_object);
	ULONG WINAPI AddRef();
	ULONG WINAPI Release();
	HRESULT WINAPI QueryContinueDrag(BOOL escape_pressed, DWORD keyboard_state);
	HRESULT WINAPI GiveFeedback(DWORD);

private:
	int m_ref_count;
};

/**
 * Platform dnd gets dnd data from Opera using WindowsDataObject during
 * dragging on DragEnter() and on Drop().
 **/
class WindowsDataObject : public IDataObject
{
public:
	IDropSource* m_ids;
	WindowsDataObject(IDropSource* ids);
	virtual ~WindowsDataObject();
	void CopyMedium(STGMEDIUM* medium_destination, STGMEDIUM* medium_source, FORMATETC* fetc_source);

	// Implements IDataObject.
	HRESULT WINAPI QueryInterface(const IID& riid, void** void_object);
	ULONG WINAPI AddRef();
	ULONG WINAPI Release();
	HRESULT WINAPI GetData(FORMATETC* fetc, STGMEDIUM *medium);
	HRESULT WINAPI GetDataHere(FORMATETC* fetc,STGMEDIUM* medium);
	HRESULT WINAPI QueryGetData(FORMATETC* fetc);
	HRESULT WINAPI GetCanonicalFormatEtc(FORMATETC* fetc_in, FORMATETC* fetc_out);
	HRESULT WINAPI SetData(FORMATETC* fetc, STGMEDIUM* medium, BOOL release);
	HRESULT WINAPI EnumFormatEtc(DWORD direction, IEnumFORMATETC** enum_fetc);
	HRESULT WINAPI DAdvise(FORMATETC* , DWORD , IAdviseSink* , DWORD *);
	HRESULT WINAPI DUnadvise(DWORD);
	HRESULT WINAPI EnumDAdvise(IEnumSTATDATA **);

	/**
	 * Sets the passed in drag'n'drop data in OS specific structures.
	 *
	 * @param drag_object - active Opera drag object
	 * @param type - mime type of the data to set.
	 * @param contents - the data to be set.
	 * @param len - the data's length.
	 *
	 * @return TRUE if the data was set. FALSE otherwise.
	 */
	BOOL SetData(OpDragObject *drag_object, const char* type, const char* contents, unsigned int len);

	/**
	 * Sets the passed in drag'n'drop data directly as a supplied clipboard format
	 *
	 * @param cf_type - clipboard type of the data to set.
	 * @param contents - the data to be set.
	 * @param len - the data's length.
	 * @param convert_to_unicode - convert the data to unicode or keep it as-is
	 *
	 * @return TRUE if the data was set. FALSE otherwise.
	 */
	BOOL SetClipboardFormatData(CLIPFORMAT cf_type, const char* contents, unsigned int len, bool convert_to_unicode);

	/**
	 * Sets the passed in drag'n'drop data as a virtual file in memory, it'll be created as a
	 * real file when dropped on a supporting application (most likely Explorer)
	 *
	 * @param drag_object - The active Opera drag object
	 * @param contents - the data to be set.
	 * @param len - the data's length.
	 *
	 * @return TRUE if the data was set. FALSE otherwise.
	 */
	BOOL SetClipboardVirtualFileData(OpDragObject *drag_object, const char* contents, unsigned int len);

	/**
	 * Sets the passed Image as as a CF_DIB in the platform drop data.
	 *
	 * @param drag_object - The active Opera drag object
	 * @param image - the image to be set
	 *
	 * @return TRUE if the data was set. FALSE otherwise.
	 */
	BOOL SetClipboardDIB(OpDragObject *drag_object, Image image);

	/**
	 * The URL of the page the drag started on. 
	 */
	OP_STATUS SetOriginURL(OpStringC url) { return m_origin_url.SetUTF8FromUTF16(url); }

private:
	// Create CF_HDROP format of the files, if any
	OP_STATUS CommitFileData();

private:
	int m_ref_count;
	OpVector<FORMATETC> m_fetcs;
	OpVector<STGMEDIUM> m_mediums;
	OpString8			m_origin_url;			// if origin is a page, this is the url
	OpAutoVector<OpString>	m_drop_files;		// if a text/uri-list is actually a file, put it here and create the CF_HDROP object in CommitFileData()
};

/**
 * WindowsDataObject uses WindowsEnumFormatEtc when platform wants to check
 * what kind of data it is dragging.
 **/
class WindowsEnumFormatEtc : public IEnumFORMATETC
{
public:
	WindowsEnumFormatEtc(const OpVector<FORMATETC>& fetcs);

	// Implements IEnumFORMATETC.
	HRESULT WINAPI QueryInterface(REFIID refiid, void** void_object);
	ULONG WINAPI AddRef();
	ULONG WINAPI Release();
	HRESULT WINAPI Next(ULONG celt, FORMATETC* fetc, ULONG* celt_fetched);
	HRESULT WINAPI Skip(ULONG celt);
	HRESULT WINAPI Reset();
	HRESULT WINAPI Clone(IEnumFORMATETC** clone_enum_fetc);

private:
	ULONG m_ref_count;
	OpVector<FORMATETC>  m_fetcs;
	unsigned int m_current;
};

#endif // WINDOWSOPDRAGOBJECT_H
