/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _JS11_LOCATION_
#define _JS11_LOCATION_

class FramesDocument;
class ES_Thread;

#include "modules/ecmascript/ecmascript.h"
#include "modules/dom/src/domobj.h"
#include "modules/url/url2.h"

/**
 * Helper function for creating a resolved url based on an url string taking different documents into account.
 *
 * @param origining_frames_doc The source of the string. Used to resolve relative urls.
 * @param target_frames_doc The target document (if known). Used to make sure the generated url has the
 * right context.
 * @param url The url string. Can be an absolute or relative url.
 * @param override_base_url An overriding base URL. If non-NULL, the base URL
 *        to use rather than what the origining_frames_doc has set. Used in a
 *        selftest context only.
 */
URL GetEncodedURL( FramesDocument *origining_frames_doc
                 , FramesDocument* target_frames_doc
                 , const uni_char *url
#ifdef SELFTEST
                 , URL *override_base_url = NULL
#endif // SELFTEST
                 );

class JS_FakeWindow;

class JS_Location
	: public DOM_Object
{
protected:
	ES_PutState SetTheURL(FramesDocument* frames_document, DocumentReferrer& ref_url, URL& url, ES_Thread* origin_thread, BOOL only_hash);
	DocumentReferrer GetStandardRefURL(FramesDocument* frames_doc, ES_Runtime* origining_runtime);

	BOOL do_reload;
	BOOL do_replace;
	JS_FakeWindow *fakewindow;

#ifdef SELFTEST
	/* For selftest purposes the Location object can be turned off and
	   disallow navigation on updates of the location properties. */
	BOOL do_navigation;
	URL current_url;
#endif // SELFTEST

public:
	JS_Location(JS_FakeWindow *fakewindow = NULL)
		: do_reload(FALSE)
		, do_replace(FALSE)
		, fakewindow(fakewindow)
#ifdef SELFTEST
		, do_navigation(TRUE)
#endif // SELFTEST
	{
	}

	virtual BOOL SecurityCheck(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState	PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState	PutName(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL AllowOperationOnProperty(const uni_char *property_name, EcmaScript_Object::PropertyOperation op);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_LOCATION || DOM_Object::IsA(type); }
	virtual void GCTrace();

	BOOL				OpenURL(URL &url, DocumentReferrer ref_url, BOOL is_reload, BOOL is_replace, ES_Thread* origin_thread);
	void				DoReload();
	void				DoReplace();

	static void PreparePrototype(ES_Object *object, DOM_Runtime *runtime);

#ifdef SELFTEST
	/* Disable navigation on Location property updates. */
	void DisableNavigation();
#endif // SELFTEST

	URL GetEncodedURL(FramesDocument *origining_frames_doc, const uni_char *url);
	/**< Method that wrap a call to the global GetEncodedURL function. */

	DOM_DECLARE_FUNCTION(reload);
	DOM_DECLARE_FUNCTION(toString);
	enum { FUNCTIONS_ARRAY_SIZE = 4 };

	DOM_DECLARE_FUNCTION_WITH_DATA(replaceOrAssign); // replace and assign
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 3 };
};

#ifdef DOM_WEBWORKERS_SUPPORT
class JS_Location_Worker
	: public JS_Location
{
public:
	JS_Location_Worker(JS_FakeWindow *fakewindow = NULL) : JS_Location(fakewindow) {}

	virtual ES_PutState	PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	enum { FUNCTIONS_ARRAY_SIZE = 3 };
};
#endif


#endif /* _JS11_LOCATION_ */


