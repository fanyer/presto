/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Lasse Magnussen lasse@opera.com
*/

#ifndef MODULES_DOM_DOMIO_H
#define MODULES_DOM_DOMIO_H

#if defined(DOM_GADGET_FILE_API_SUPPORT) || defined(WEBSERVER_SUPPORT)

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/util/adt/opvector.h"
#include "modules/webserver/webserver_callbacks.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/windowcommander/OpWindowCommander.h"

class DOM_GadgetFile;
class DOM_FileList;
class DOM_WebServer;
class WebserverFileSandbox;
class WebserverBodyObject_Base;
class WebserverFolderSelector;
class OpPersistentStorageListener;
class WebserverResourceDescriptor_Base;

/************************************************************************/
/* class DOM_IO                                                         */
/************************************************************************/

class DOM_IO
	: public DOM_Object
#ifdef SELFTEST
	,public OpPersistentStorageListener
#endif // SELFTESTS
{
public:
	static OP_STATUS Make(DOM_IO *&new_obj, DOM_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_IO || DOM_Object::IsA(type); }

#ifdef SELFTEST
	// from OpPersistentStorageListener
	OP_STATUS		SetPersistentData(const uni_char* section, const uni_char* key, const uni_char* value) { return OpStatus::OK; }
	const uni_char*	GetPersistentData(const uni_char* section, const uni_char* key) { return NULL; }
	OP_STATUS		DeletePersistentData(const uni_char* section, const uni_char* key) { return OpStatus::OK; }
	OP_STATUS		GetPersistentDataItem(UINT32 idx, OpString& key, OpString& data) { return OpStatus::ERR; }
	OP_STATUS		GetStoragePath(OpString& storage_path);
#endif // SELFTEST

private:
	DOM_IO() : m_persistentStorageListener(NULL) {}
	OP_STATUS Initialize();

	void InstallFileObjectsL();
#ifdef WEBSERVER_SUPPORT
	OP_STATUS InstallWebserverObjects();
#endif // WEBSERVER_SUPPORT

	OpPersistentStorageListener *m_persistentStorageListener;

public:
	static BOOL AllowIOAPI(FramesDocument* frm_doc);
	/**< Returns TRUE if the opera.io object should be present for the
		 specified FramesDocument's runtime. */

	static BOOL AllowFileAPI(FramesDocument* frm_doc);
};

#endif //defined(DOM_GADGET_FILE_API_SUPPORT) || defined(WEBSERVER_SUPPORT)
#endif // !MODULES_DOM_DOMIO_H
