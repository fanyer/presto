/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Markus Johansson
*/

#ifdef SESSION_SUPPORT

#ifndef OP_SESSION_H
#define OP_SESSION_H

#include "modules/prefsfile/prefsfile.h"
#include "modules/util/adt/opvector.h"
#include "modules/skin/OpWidgetImage.h"
#include "modules/prefsfile/accessors/ini.h"

class OpSessionWindow;
class OpFileDescriptor;


class OpSessionReader
{
public:

	OpSessionReader();
	virtual	~OpSessionReader() {}

	void InitL(const OpStringC8& section, PrefsFile* prefsfile);

	bool HasValue(const OpStringC8& name) const;
	INT32 GetValueL(const OpStringC8& name) const;
	void SetValueL(const OpStringC8& name, INT32 value);

	const OpStringC	GetStringL(const OpStringC8& name) const;
	void SetStringL(const OpStringC8& name, const OpStringC& string);

	void GetStringArrayL(const OpStringC8& name, OpVector<OpString>& array) const;
	void SetStringArrayL(const OpStringC8& name, const OpVector<OpString>& array);
	void ClearStringArrayL();

	void GetValueArrayL(const OpStringC8& name, OpVector<UINT32>& array) const;
	void SetValueArrayL(const OpStringC8& name, const OpVector<UINT32>& array);

	/** Accessor for the configuration file */
	class SessionFileAccessor : public IniAccessor
	{
	public:
		SessionFileAccessor() : IniAccessor(FALSE) {}

		/**
		* Store the contents in the map to a file.
		*
		* @param file The file to store stuff in
		* @param map The map to get stuff from
		* @return Status of the operation
		*/
		virtual OP_STATUS StoreL(OpFileDescriptor *file, const PrefsMap *map);
	};

protected:

	OpString8	m_section;
	PrefsFile*	m_prefsfile;
};


class OpSession
	: public OpSessionReader
{
public:

	~OpSession();

	void InitL(PrefsFile* prefsfile, BOOL in_memory_only = FALSE);

	void LoadL();

	UINT32 GetVersionL() const;
	void SetVersionL(UINT32 version);

	UINT32 GetCreationDateL() const;
	void SetCreationDateL(UINT32 creation_date);

	OpStringC GetCommentL() const;
	void SetCommentL(const OpStringC& comment);

	UINT32 GetWindowCountL() const;

	OpSessionWindow* GetWindowL(UINT32 index);
	OpSessionWindow* AddWindowL();
	void DeleteWindowL(UINT32 index);
	void DeleteWindowL(OpSessionWindow* window);

	void WriteToFileL(BOOL reset_after_write);

	BOOL DeleteL();

	BOOL operator==(const OpSession* filetocompare);

	/**
	 * Initialize session for writing it - use to turn off cascading
	 */
	OP_STATUS InitForWrite();

	/**
	 * Abort storing the session - makes sure it's not stored in the destructor
	 */
	void Cancel();

private:

	OpAutoVector<OpSessionWindow>	m_windows;
	BOOL							m_in_memory_only;
};


class OpSessionWindow
	: public OpSessionReader
{
public:

	enum Type { BROWSER_WINDOW,
		        DOCUMENT_WINDOW,
				MAIL_WINDOW,
				MAIL_COMPOSE_WINDOW,
				TRANSFER_WINDOW,
				HOTLIST_WINDOW,
				CHAT_WINDOW,
				PANEL_WINDOW,
				GADGET_WINDOW
#ifdef DEVTOOLS_INTEGRATED_WINDOW
				, DEVTOOLS_WINDOW
#endif // DEVTOOLS_INTEGRATED_WINDOW
				};

	void InitL(PrefsFile* prefsfile, UINT32 id);

	Type GetTypeL() const;
	void SetTypeL(Type type);

	/**
	 * Should only be used from OpSession.
	 */
	void DeleteL();

	/**
	 * I need this for in-memory-only session windows used for Undo'ing of closed pages because
	 * it is neat to keep the title image around to be displayed in the "Closed pages" menu
	 */
	OpWidgetImage* GetWidgetImage() {return &m_image;}

private:
	OpWidgetImage				m_image;
};

#endif // OP_SESSION_H

#endif // SESSION_SUPPORT
