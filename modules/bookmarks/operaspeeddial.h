/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BOOKMARKS_OPERASPEEDDIAL_H
#define BOOKMARKS_OPERASPEEDDIAL_H

#ifdef OPERASPEEDDIAL_URL

#include "modules/about/opgenerateddocument.h"
#include "modules/bookmarks/speeddial_listener.h"
#include "modules/url/url_lop_api.h"

/** URL hook */
class OperaSpeedDialURLGenerator 
	: public OperaURL_Generator
{
public:
	virtual OperaURL_Generator::GeneratorMode GetMode() const { return OperaURL_Generator::KQuickGenerate; }
	virtual OP_STATUS QuickGenerate(URL &url, OpWindowCommander*);
};

/** opera:speeddial generator */
class OperaSpeedDial
	: public OpGeneratedDocument
{
public:
	OperaSpeedDial(URL url);

	virtual ~OperaSpeedDial();
	virtual OP_STATUS GenerateData();
};


class ES_AsyncInterface;
class ES_Object;

/**
 * Keeps track of the callback to one opera:speeddial page.
 */
class OperaSpeedDialCallback
	: public OpSpeedDialListener
{
public:
	OperaSpeedDialCallback(ES_AsyncInterface *ai, ES_Object *callback) 
		: m_ai(ai), m_callback(callback)
	{ }

	/** Destructor the also removes the object from the callback list */
	~OperaSpeedDialCallback();

	/** Call during GCTrace. Will prevent the callback from being GC'ed. */
	void GCTrace(ES_Runtime *runtime);

	// From OpSpeedDialListener
	OP_STATUS OnSpeedDialChanged(SpeedDial* speed_dial);
	OP_STATUS OnSpeedDialRemoved(SpeedDial* speed_dial);

private:
	OP_STATUS Call(int index, const uni_char* title, const uni_char* url, const uni_char* thumbnail_url, BOOL is_loading);
	
	ES_AsyncInterface *m_ai;
	ES_Object *m_callback;
};

#endif // OPERASPEEDDIAL_URL
#endif // BOOKMARKS_OPERASPEEDDIAL_H
