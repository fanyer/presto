/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLPARSERIMPL_INTERNAL_H
#define XMLPARSERIMPL_INTERNAL_H

#include "modules/xmlutils/src/xmlparserimpl.h"
#include "modules/xmlparser/xmldatasource.h"

class OpFile;
class UnicodeInputStream;
class XMLDataSourceImpl;

class XMLDataSourceHandlerImpl
	: public XMLDataSourceHandler
{
protected:
	XMLParserImpl *parser;
	XMLDataSourceImpl *base, *current;
	BOOL load_external_entities;

	OP_BOOLEAN Load(XMLDataSourceImpl *source, const URL &url);

public:
	XMLDataSourceHandlerImpl(XMLParserImpl *parser, BOOL load_external_entities);
	~XMLDataSourceHandlerImpl();

	virtual OP_STATUS CreateInternalDataSource(XMLDataSource *&source, const uni_char *data, unsigned data_length);
	virtual OP_STATUS CreateExternalDataSource(XMLDataSource *&source, const uni_char *pubid, const uni_char *system, URL baseurl);
	virtual OP_STATUS DestroyDataSource(XMLDataSource *source);

	OP_STATUS Construct(const URL &url);

	void SetLoadExternalEntities(BOOL value) { load_external_entities = value; }

	XMLParserImpl *GetParser() { return parser; }
	XMLDataSourceImpl *GetBaseDataSource() { return base; }
	XMLDataSourceImpl *GetCurrentDataSource() { return current; }
};

class XMLDataSourceImpl
	: public XMLDataSource,
	  public ExternalInlineListener,
	  public MessageObject
{
protected:
	XMLDataSourceHandlerImpl *handler;
	URL url;
	URL_DataDescriptor *urldd;

	OpFile *file;
	UnicodeInputStream *stream;

	class DataElement
	{
	public:
		DataElement()
			: data(NULL),
			  data_offset(0)
		{
		}

		~DataElement()
		{
			if (ownsdata)
			{
				uni_char *non_const = const_cast<uni_char *>(data);
				OP_DELETEA(non_const);
			}
		}

		const uni_char *data;
		unsigned data_length, data_offset;
		BOOL ownsdata;

		DataElement *next;
	};

	DataElement *first_data_element, *last_data_element;
	BOOL is_at_end, discarded, force_empty, is_loading, url_allowed, need_more_data;
	unsigned busy, default_consumed;

	/* From ExternalInlineListener. */
	virtual void LoadingProgress(const URL &url);
	virtual void LoadingStopped(const URL &url);
	virtual void LoadingRedirected(const URL &from, const URL &to);

	/* From MessageObject. */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	void LoadFromUrl();

	/* To unregister from an inline load or message delivery. */
	void Unregister();

public:
	XMLDataSourceImpl(XMLDataSourceHandlerImpl *handler);
	XMLDataSourceImpl(XMLDataSourceHandlerImpl *handler, const URL &url, BOOL url_allowed, OpFile *file = 0, UnicodeInputStream *stream = 0);
	~XMLDataSourceImpl();

	virtual OP_STATUS Initialize();
	virtual const uni_char *GetName();
	virtual URL GetURL();

	virtual const uni_char *GetData();
	virtual unsigned GetDataLength();

	virtual unsigned Consume(unsigned length);
	virtual OP_BOOLEAN Grow();
	virtual BOOL IsAtEnd();
	virtual BOOL IsAllSeen();

#ifdef XML_ERRORS
	virtual OP_BOOLEAN Restart();
#endif // XML_ERRORS

	OP_STATUS AddData(const uni_char *data, unsigned data_length, BOOL more, BOOL needcopy = TRUE);
	OP_STATUS BeforeParse();
	OP_STATUS AfterParse(BOOL is_oom);

	OP_STATUS LoadFromStream();
	OP_STATUS SetCallbacks();
	OP_STATUS CleanUp(BOOL is_oom, unsigned *consumed);

	void SetLoading() { is_loading = TRUE; }
	BOOL IsLoading() { return is_loading; }

	BOOL CheckExternalEntityAccess();

	BOOL IsDiscarded() { return discarded && !busy; }

	static void Discard(XMLDataSourceImpl *source);
};

#ifdef XML_CONFIGURABLE_DOCTYPES
# include "modules/util/simset.h"

class XMLConfiguredDoctypes
	: public Head
{
protected:
	BOOL defaultlocation;
	OpFile *file;
	time_t lastmodified;

	void UpdateL(BOOL &is_default);

public:
	XMLConfiguredDoctypes();
	~XMLConfiguredDoctypes();

	OP_STATUS Update(BOOL &is_default);
	BOOL Find(const uni_char *&filename, const uni_char *public_id, const uni_char *system_id, BOOL load_external_entities);
};

class XMLConfiguredDoctype
	: public Link
{
protected:
	OpString public_id, system_id, filename;
	BOOL always_use;

public:
	void ConstructL(PrefsFile &prefsfile, const uni_char *section);

	BOOL Match(const uni_char *public_id, const uni_char *system_id);
	BOOL AlwaysUse() { return always_use; }

	const uni_char *GetFilename();
};

#endif // XML_CONFIGURABLE_DOCTYPES
#endif // XMLPARSERIMPL_INTERNAL_H
