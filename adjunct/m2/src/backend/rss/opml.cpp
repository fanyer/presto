// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// $Id$
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#include "opml.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/backend/rss/opml_importer.h"
#include "adjunct/m2/src/backend/rss/opml_exporter.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/util/xmlparser_tracker.h"
#include "adjunct/quick/Application.h"
#include "modules/util/opfile/opfile.h"

using namespace M2XMLUtils;

// ***************************************************************************
//
//	OPMLParser
//
// ***************************************************************************

class OPMLParser
:	public XMLParserTracker
{
public:
	// Construction.
	OPMLParser(OPMLImportHandler& import_handler);
	OP_STATUS Init();

private:
	// XMLParserTracker.
	virtual OP_STATUS OnStartElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes);
	virtual OP_STATUS OnEndElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes);

	// Members.
	OPMLImportHandler& m_import_handler;
	BOOL m_inside_opml;
};


OPMLParser::OPMLParser(OPMLImportHandler& import_handler)
:	m_import_handler(import_handler),
	m_inside_opml(FALSE)
{
}


OP_STATUS OPMLParser::Init()
{
	RETURN_IF_ERROR(XMLParserTracker::Init());
	return OpStatus::OK;
}


OP_STATUS OPMLParser::OnStartElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	OpAutoStringHashTable<XMLAttributeQN> &attributes)
{
	if (m_inside_opml)
	{
		if (local_name.CompareI("outline") == 0 &&
			IsDescendantOf(UNI_L("body")))
		{
			// Fetch the attribute values we care about.
			XMLAttributeQN* text_attribute = 0;
			OpStatus::Ignore(attributes.GetData(UNI_L("text"), &text_attribute));

			XMLAttributeQN* xml_url_attribute = 0;
			OpStatus::Ignore(attributes.GetData(UNI_L("xmlUrl"), &xml_url_attribute));

			XMLAttributeQN* title_attribute = 0;
			OpStatus::Ignore(attributes.GetData(UNI_L("title"), &title_attribute));

			XMLAttributeQN* description_attribute = 0;
			OpStatus::Ignore(attributes.GetData(UNI_L("description"), &description_attribute));

			// Notify import handler.
			OpStatus::Ignore(m_import_handler.OnOutlineBegin(
				text_attribute != 0 ? text_attribute->Value() : OpStringC(),
				xml_url_attribute != 0 ? xml_url_attribute->Value() : OpStringC(),
				title_attribute != 0 ? title_attribute->Value() : OpStringC(),
				description_attribute != 0 ? description_attribute->Value() : OpStringC()));
		}
	}
	else if (local_name.CompareI("opml") == 0)
	{
		m_inside_opml = TRUE;

		// Notify import handler.
		OpStatus::Ignore(m_import_handler.OnOPMLBegin());
	}

	return OpStatus::OK;
}



OP_STATUS OPMLParser::OnEndElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes)
{
	if (m_inside_opml)
	{
		if (local_name.CompareI("title") == 0 &&
			IsDescendantOf(UNI_L("head")))
		{
			// Notify import handler.
			const OpStringC& title = ElementContent();
			OpStatus::Ignore(m_import_handler.OnHeadTitle(title));
		}
		else if (local_name.CompareI("outline") == 0 &&
			IsDescendantOf(UNI_L("body")))
		{
			// Notify import handler.
			OpStatus::Ignore(m_import_handler.OnOutlineEnd(!attributes.Contains(UNI_L("xmlUrl"))));
		}
		else if (local_name.CompareI("opml") == 0)
		{
			m_inside_opml = FALSE;

			// Notify import handler.
			OpStatus::Ignore(m_import_handler.OnOPMLEnd());
		}
	}

	return OpStatus::OK;
}

// ***************************************************************************
//
//	OPML
//
// ***************************************************************************

OP_STATUS OPML::Import(const OpStringC& file_path, OPMLImportHandler& import_handler)
{
	// Open the file.
	OpFile file;
	RETURN_IF_ERROR(file.Construct(file_path.CStr()));
	RETURN_IF_ERROR(file.Open(OPFILE_READ | OPFILE_TEXT));

	// Read the content and parse it.
	OPMLParser opml_parser(import_handler);
	RETURN_IF_ERROR(opml_parser.Init());

	char buffer[4096];
	OpFileLength bytes_read = 0;

	while (!file.Eof())
	{
		OpStatus::Ignore(file.Read(&buffer, 4096, &bytes_read));
		RETURN_IF_ERROR(opml_parser.Parse(buffer, UINT(bytes_read), file.Eof()));
	}

	return OpStatus::OK;
}


OP_STATUS OPML::Export(const OpStringC& file_path)
{
	// Open the file for output.
	OpFile file;
	RETURN_IF_ERROR(file.Construct(file_path.CStr()));
	RETURN_IF_ERROR(file.Open(OPFILE_WRITE | OPFILE_TEXT));

	// Write out the content.
	OPMLExporter opml_exporter;
	RETURN_IF_ERROR(opml_exporter.Export(file));

	return OpStatus::OK;
}
