/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/about/operagpu.h"

#ifdef VEGA_OPPAINTER_SUPPORT

# include "modules/libvega/vegarenderer.h"
# include "modules/locale/oplanguagemanager.h"
# include "modules/locale/locale-enum.h"

// virtual
OP_STATUS OperaGPU::GenerateData()
{
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_IDABOUT_GPU, PrefsCollectionFiles::StyleGPUFile));
#else // _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_IDABOUT_GPU));
#endif // _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenBody());

	RETURN_IF_ERROR(GenerateBackendInfo());

	return CloseDocument();
}

// NOTE: should only be used directly when str doesn't need translation
OP_STATUS OperaGPU::Paragraph(const OpStringC &str)
{
	OpString line;
	RETURN_IF_ERROR(line.AppendFormat("<p>%s</p>\n", str.CStr()));
	return m_url.WriteDocumentData(URL::KNormal, line.CStr());
}

// NOTE: should only be used directly when str doesn't need translation
OP_STATUS OperaGPU::Heading(const OpStringC &str, unsigned level/* = 1*/)
{
	OpString line;
	RETURN_IF_ERROR(line.AppendFormat("<h%d>%s</h%d>\n", level, str.CStr(), level));
	return m_url.WriteDocumentData(URL::KNormal, line.CStr());
}

OP_STATUS OperaGPU::ListEntry(const OpStringC &term, const OpStringC &def, const OpStringC& details)
{
	OpString line;
	line.Reserve(256);
	RETURN_IF_ERROR(line.Set("  <dt>"));
	RETURN_IF_ERROR(line.Append(term));
	RETURN_IF_ERROR(line.Append("</dt><dd>"));
	const uni_char *p = def.CStr();
	if (p)
	{
		RETURN_IF_ERROR(AppendHTMLified(&line, p, static_cast<unsigned>(uni_strlen(p))));
	}
	const uni_char *d = details.CStr();
	if (d)
	{
		RETURN_IF_ERROR(line.Append(p ? UNI_L(": <i>") : UNI_L("<i>")));
		RETURN_IF_ERROR(AppendHTMLified(&line, d, static_cast<unsigned>(uni_strlen(d))));
		RETURN_IF_ERROR(line.Append(UNI_L("</i>")));
	}
	RETURN_IF_ERROR(line.Append("</dd>\n"));
	return m_url.WriteDocumentData(URL::KNormal, line);
}

OP_STATUS OperaGPU::Link(const OpStringC &title, const OpStringC &url)
{
	OpString line;
	line.Reserve(256);
	RETURN_IF_ERROR(line.AppendFormat("<a href=\"%s\">%s</a>\n", url.CStr(), title.CStr()));
	return m_url.WriteDocumentData(URL::KNormal, line);
}

OP_STATUS OperaGPU::OpenDefinitionList()
{
	return m_url.WriteDocumentData(URL::KNormal, UNI_L("<dl>\n"));
}

OP_STATUS OperaGPU::CloseDefinitionList()
{
	return m_url.WriteDocumentData(URL::KNormal, UNI_L("</dl>\n"));
}

OP_STATUS OperaGPU::GenerateBackendInfo()
{
	return VEGARenderer::GenerateBackendInfo(this);
}

#endif // VEGA_OPPAINTER_SUPPORT
