/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#ifndef OPPREFSLISTENER_H
#define OPPREFSLISTENER_H

#include "modules/prefs/prefsmanager/opprefscollection.h"

/**
  * Interface implemented by all classes that wish to listen to
  * changes in an OpPrefsCollection.
  *
  * To do that, make your class inherit this one, and call
  * OpPrefsCollection::RegisterListenerL(this) on the collection(s) you
  * wish to listen to.
  */
class OpPrefsListener
{
public:
	/** Standard destructor. Needs to be virtual due to inheritance. */
	virtual ~OpPrefsListener() {}

	/** Signals a change in an integer preference.
	  *
	  * @param id       Identity of the collection that has changed the value.
	  * @param pref     Identity of the preference. This is the integer
	  *                 representation of the enumeration value used by the
	  *                 associated OpPrefsCollection.
	  * @param newvalue The new value of the preference.
	  */
	virtual void PrefChanged(OpPrefsCollection::Collections /* id */, int /* pref */,
	                         int /* newvalue */) {}

	/** Signals a change in a string preference.
	  *
	  * @param id       Identity of the collection that has changed the value.
	  * @param pref     Identity of the preference. This is the integer
	  *                 representation of the enumeration value used by the
	  *                 associated OpPrefsCollection.
	  * @param newvalue The new value of the preference.
	  */
	virtual void PrefChanged(OpPrefsCollection::Collections /* id */, int /* pref */,
	                         const OpStringC & /* newvalue */) {}

#ifdef PREFS_HOSTOVERRIDE
	/** Signals the addition, change or removal of a host override.
	  *
	  * @param id       Identity of the collection that has changed the value.
	  * @param hostname The affected host name.
	  */
	virtual void HostOverrideChanged(OpPrefsCollection::Collections /* id */,
	                                 const uni_char * /* hostname */) {}
#endif
};

#endif // OPPREFSLISTENER_H
