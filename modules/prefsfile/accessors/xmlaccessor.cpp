/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef PREFS_HAS_XML

#include "modules/prefsfile/accessors/xmlaccessor.h"
#include "modules/prefsfile/impl/backend/prefsmap.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opsafefile.h"
#include "modules/util/tempbuf.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlfragment.h"

OP_STATUS XmlAccessor::LoadL(OpFileDescriptor *file, PrefsMap *map)
{
	// Load the file
	OpStackAutoPtr<XMLFragment> xml(OP_NEW_L(XMLFragment, ()));
	xml->SetDefaultWhitespaceHandling(XMLWHITESPACEHANDLING_PRESERVE);
	OP_STATUS rc = file->Open(OPFILE_READ | OPFILE_TEXT);
	if (OpStatus::IsSuccess(rc))
	{
		rc = xml->Parse(file, "utf-8");
		LEAVE_IF_ERROR(file->Close());
		if (OpStatus::IsFatal(rc))
		{
			LEAVE(rc);
		}
	}
	if (OpStatus::IsError(rc))
	{
		return rc;
	}

	if (xml->EnterElement(UNI_L("preferences")))
	{
		// Iterate over all elements in <preferences>.
		while (xml->HasMoreElements())
		{
			if (xml->EnterAnyElement())
			{
				// Read any <section> element.
				if (xml->GetElementName() == UNI_L("section"))
				{
					LoadSectionL(xml.get(), map);
				}

				xml->LeaveElement();
			}
		}
	}

	return OpStatus::OK;
}

void XmlAccessor::LoadSectionL(XMLFragment *fragment, PrefsMap *map)
{
	// The section name is stored in the "id" attribute
	const uni_char *section = fragment->GetAttribute(UNI_L("id"));
	if (!section || !*section)
		return;

	// Iterate over all elements in <section>
	while (fragment->EnterAnyElement())
	{
		if (fragment->GetElementName() == UNI_L("value"))
		{
			// The key is stored in the "id" attribute.
			const uni_char *key = fragment->GetAttribute(UNI_L("id"));

			// The value is stored as text data.
			TempBuffer buffer; ANCHOR(TempBuffer, buffer);
			LEAVE_IF_ERROR(fragment->GetAllText(buffer));

			const uni_char *data = buffer.GetStorage();
			if (!data)
			{
				// Distinguish between null data and empty strings
				// with an attribute
				const uni_char *isnull = fragment->GetAttribute(UNI_L("null"));
				if (!isnull || !uni_str_eq(isnull, UNI_L("yes")))
				{
					data = UNI_L("");
				}
			}

			if (key && *key)
			{
				map->SetL(section, key, data);
			}
		}
		fragment->LeaveElement();
	}
}

#ifdef PREFSFILE_WRITE
OP_STATUS XmlAccessor::StoreL(OpFileDescriptor *file, const PrefsMap *map)
{
	if (!file->IsWritable())
	{
		return OpStatus::ERR_NO_ACCESS;
	}

	OpStackAutoPtr<XMLFragment> xml(OP_NEW_L(XMLFragment, ()));
	xml->SetDefaultWhitespaceHandling(XMLWHITESPACEHANDLING_PRESERVE);

	// Create the XML code.
	LEAVE_IF_ERROR(xml->OpenElement(UNI_L("preferences")));

	const PrefsSection *section = map->Sections();
	while (section != NULL)
	{
		// Create the <section> element
		LEAVE_IF_ERROR(xml->OpenElement(UNI_L("section")));
		LEAVE_IF_ERROR(xml->SetAttribute(UNI_L("id"), section->Name()));

		const PrefsEntry *entry = section->Entries();
		while (entry != NULL)
		{
			// Create the <value> element
			LEAVE_IF_ERROR(xml->OpenElement(UNI_L("value")));
			LEAVE_IF_ERROR(xml->SetAttribute(UNI_L("id"), entry->Key()));
			if (entry->Value())
			{
				LEAVE_IF_ERROR(xml->AddText(entry->Value(),
					uni_strlen(entry->Value()),
					XMLWHITESPACEHANDLING_PRESERVE));
			}
			else
			{
				LEAVE_IF_ERROR(xml->SetAttribute(UNI_L("null"), UNI_L("yes")));
			}
			xml->CloseElement();

			entry = entry->Suc();
		}

		// Iterate over the sections.
		section = section->Suc();
		xml->CloseElement();
	}

	xml->CloseElement();

	// Now dump the XML document to file.
	ByteBuffer buffer; ANCHOR(ByteBuffer, buffer);
	LEAVE_IF_ERROR(xml->GetEncodedXML(buffer, TRUE, "utf-8", TRUE));

	// If this is a disk file, we need to use a OpSafeFile to rewrite
	// it, to ensure we don't lose data if writing fails.
	OpStackAutoPtr<OpSafeFile> safefile(NULL);
	if (file->Type() == OPFILE)
	{
		safefile.reset(OP_NEW_L(OpSafeFile, ()));
		LEAVE_IF_ERROR(safefile->Construct(static_cast<OpFile *>(file)));
		file = safefile.get();
	}

	OP_STATUS rc = file->Open(OPFILE_WRITE | OPFILE_TEXT);
	if (OpStatus::IsSuccess(rc))
	{
		unsigned int n = buffer.GetChunkCount();
		for (unsigned int i = 0; i < n; ++ i)
		{
			unsigned int len;
			const char *data = buffer.GetChunk(i, &len);
			LEAVE_IF_ERROR(file->Write(data, len));
		}
		rc = file->Close();
	}

	if (safefile.get())
	{
		LEAVE_IF_ERROR(safefile->SafeClose());
	}

	return rc;
}
#endif // PREFSFILE_WRITE

#endif // PREFS_HAS_XML
