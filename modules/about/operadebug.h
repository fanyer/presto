/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MODULES_ABOUT_OPERADEBUG_H 
#define MODULES_ABOUT_OPERADEBUG_H

#ifdef ABOUT_OPERA_DEBUG

#include "modules/hardcore/mh/messobj.h"

#include "modules/about/opgenerateddocument.h"

class ES_Object;
class ES_AsyncInterface;

/**
 * Generator for the opera:debug document.
 */
class OperaDebug : public OpGeneratedDocument
{
public:
    /**
     * Constructor for the "empty" page generator.
     *
     * @param url URL to write to.
     */
    OperaDebug(URL &url)
	: OpGeneratedDocument(url, OpGeneratedDocument::HTML5, TRUE)
	{}
    
    /**
     * Generate the opera:debug document to the specified internal URL.
     *
     * @return OK on success, or any error reported by URL or string code.
     */
    virtual OP_STATUS GenerateData();
};

class DOM_Object;
class ES_AsyncInterface;

class OperaDebugProxy : public MessageObject, public Link
{
public:
	OperaDebugProxy(ES_AsyncInterface *ai, ES_Runtime *runtime, DOM_Object* opera_object, ES_Object *callback);
	~OperaDebugProxy();
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	ES_Object *GetCallback() { return m_callback; }
private:
	ES_AsyncInterface *m_ai;
	ES_Runtime *m_runtime;
	ES_Object *m_callback;
	DOM_Object *m_opera_object;
};

#endif // ABOUT_OPERA_DEBUG
#endif // MODULES_ABOUT_OPERADEBUG_H
