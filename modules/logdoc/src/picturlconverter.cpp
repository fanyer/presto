/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#include "core/pch.h"

#ifdef PICTOGRAM_SUPPORT

#include "modules/logdoc/src/picturlconverter.h"

#include "modules/util/handy.h"
#include "modules/util/opfile/opfile.h"


void
PictUrlConverter::GetLocalUrlL(
	const uni_char *pict_url, int pict_url_len, OpString &out_url_str)
{
	out_url_str.Empty();

	if (pict_url_len <= 7 || !uni_strni_eq(pict_url, "PICT://", 7))
	{
		OP_ASSERT(!"Malformed pictogram URL");
		return;
	}

	const uni_char *url_end = pict_url + pict_url_len;
	const uni_char *serv_start = pict_url + 7;
	const uni_char *serv_end = serv_start;
	while (serv_end < url_end && *serv_end != '/')
		serv_end ++;

	if (serv_end == url_end)
		return;

	OpString server;
	ANCHOR(OpString, server);
	if (serv_end == serv_start)
		server.SetL("www.wapforum.org");
	else
	{
		server.SetL(serv_start, serv_end - serv_start);
		uni_strlwr(server.CStr());
	}

	OpString loc;
	ANCHOR(OpString, loc);
	loc.SetL(UNI_L("pictograms"));
	loc.AppendL(UNI_L(PATHSEP));
	loc.AppendL(server);
	loc.AppendL(UNI_L(PATHSEP));

	const uni_char *class_start = serv_end;
	const uni_char *class_end = class_start;
	while (class_start < url_end)
	{
		while (*class_start == '/')
		{
			if (++class_start == url_end)
				return; // No pictogram name
		}

		class_end = class_start;
		while (class_end < url_end && *class_end != '/')
			class_end++;

		if (class_end == url_end)
		{
			// The pictogram name is between class_start and class_end now
			break;
		}
		else if (class_end - class_start != 2 || *class_start != '.' && *(class_start + 1) != '.')
			// weed out the ..'s in the path
		{
			loc.AppendL(class_start, class_end - class_start);
			loc.AppendL(UNI_L(PATHSEP));
		}

		class_start = class_end;
	}

	loc.AppendL(class_start, class_end - class_start);

#ifdef ADS12 // Sucky linker can't handle array-of-string initializers :-(
	const char* exts[4]; // ARRAY OK 2010-07-09 eddy
	exts[0] = ".gif"; exts[1] = ".png"; exts[2] = ".jpg"; exts[3] = ".wbmp";
	// Update array size before adding any new members !
#else // Make sure the above stays in sync with the following
	const char *const exts[] = { ".gif", ".png", ".jpg", ".wbmp" };
#endif
	for (int i=0; i<2; i++)
		for (size_t j=0; j<ARRAY_SIZE(exts); j++)
		{
			OpString filename;
			ANCHOR(OpString, filename);
			filename.SetL(loc);
			filename.AppendL(exts[j]);

			OpFile file;
			ANCHOR(OpFile, file);
			LEAVE_IF_ERROR(file.Construct(filename.CStr(), i == 0 ? OPFILE_HOME_FOLDER : OPFILE_RESOURCES_FOLDER));

			BOOL exists = FALSE;
			OP_STATUS status = file.Exists(exists);

			if(status != OpStatus::OK)
			{
				OP_ASSERT(!"Failed to discover whether pictogram file exists :-(");
			}

			if(exists)
			{
				const uni_char* full_path = file.GetFullPath();
				if (*full_path != '/')
					out_url_str.SetL("file://localhost/");
				else
					out_url_str.SetL("file://localhost");
				out_url_str.AppendL(full_path);
				return;
			}
		}
}

/*static*/ OP_STATUS
PictUrlConverter::GetLocalUrl(
	const uni_char *pict_url, int pict_url_len, OpString &out_url_str)
{
	TRAPD(ret_stat, GetLocalUrlL(pict_url, pict_url_len, out_url_str));
	return ret_stat;
}

#endif // PICTOGRAM_SUPPORT
