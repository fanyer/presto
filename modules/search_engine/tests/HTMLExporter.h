/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef HTMLEXPORTER_H
#define HTMLEXPORTER_H

#include "modules/util/opfile/opfile.h"
#include "modules/util/adt/opvector.h"


class HTMLExporter
{
public:
	OP_STATUS Open(const uni_char *file_name, int width, int height, const char *name = NULL)
	{
		int i;

		RETURN_IF_ERROR(html_file.Construct(file_name));
		RETURN_IF_ERROR(html_file.Open(OPFILE_WRITE));
		Printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\n"
				"<html>\n"
				"<head>\n");
		if (name != NULL)
			Printf("<title>%s</title>\n", name);
		else Printf("<title></title>\n");

		Printf("<style type=\"text/css\">\n"
				"  table { border-collapse: collapse; }\n");
		if (width != 0 && height != 0)
			Printf("  td { width: %ipx; height: %ipx; font-size: 1px; line-height: 1px; }\n", width, height);

		for (i = 0; i < (int)text_color.GetCount(); ++i)
			Printf("  .c_%s { background: %s; }\n", text_color.Get(i)->name, text_color.Get(i)->color);
		for (i = 0; i < (int)number_color.GetCount(); ++i)
			Printf("  .class_%u { background: %s; }\n", number_color.Get(i)->number, number_color.Get(i)->color);

		Printf("  .un_0 { background: white; }\n"
				"  .un_1 { background: black; }\n"
				"  .un_2 { background: red; }\n"
				"  .un_3 { background: green; }\n"
				"  .un_4 { background: blue; }\n"
				"  .un_5 { background: yellow; }\n"
				"</style>\n"
				"</head>\n"
				"<body>\n"
				"<table>\n"
				"<tr>\n");

		return OpStatus::OK;
	}

	void Close(void)
	{
		Printf("</tr>\n"
				"</table>\n"
				"</body>\n"
				"</html>\n");

		html_file.Close();
	}

	OP_STATUS SetClass(const char *name, const char *color)
	{
		TextColor *c;

		if ((c = OP_NEW(TextColor, ())) == NULL)
			return OpStatus::ERR_NO_MEMORY;

		op_strncpy(c->name, name, sizeof(c->name));
		op_strncpy(c->color, color, sizeof(c->color));
		c->color[sizeof(c->color) - 1] = 0;

		text_color.Add(c);

		return OpStatus::OK;
	}

	OP_STATUS SetClass(unsigned int number, const char *color)
	{
		NumberColor *c;

		if ((c = OP_NEW(NumberColor, ())) == NULL)
			return OpStatus::ERR_NO_MEMORY;

		c->number = number;
		op_strncpy(c->color, color, sizeof(c->color));
		c->color[sizeof(c->color) - 1] = 0;

		number_color.Add(c);

		return OpStatus::OK;
	}

	void Add(const char *name, const char *value = "")
	{
		int i;

		if (name == NULL)
		{
			Printf("\t<td></td>\n");
			return;
		}

		for (i = 0; i < (int)text_color.GetCount(); ++i)
			if (op_strncmp(name, text_color.Get(i)->name, sizeof(text_color.Get(i)->name)) == 0)
			{
				Printf("\t<td class=\"c_%s\">%s</td>\n", name, value);
				return;
			}

		Printf("\t<td class=\"un_%i\">%s</td>\n", name[0] % 6, value);
	}

	void Add(unsigned int number, const char *value = "")
	{
		int i;

		for (i = 0; i < (int)number_color.GetCount(); ++i)
			if (number_color.Get(i)->number == number)
			{
				Printf("\t<td class=\"class_%u\">%s</td>\n", number, value);
				return;
			}

		Printf("\t<td class=\"un_%i\">%s</td>\n", number % 6, value);
	}

	void NewLine(void)
	{
		Printf("</tr>\n</table>\n<table>\n<tr>\n");
	}

	void Label(const char *value)
	{
		Printf("\t<th>%s:&nbsp;</th>\n", value);
	}

	void Label(int value)
	{
		Printf("\t<th>%.03i:&nbsp;</th>\n", value);
	}

protected:
	// OpFile::Print was buggy
	void Printf(const char *format, ...)
	{
		va_list arglist;
		va_start(arglist, format);

		char buffer[256];  /* ARRAY OK 2010-09-24 roarl */
		int count = op_vsnprintf(buffer, 255, format, arglist);
		buffer[255] = 0;

		if (count > 0)
			html_file.Write(buffer, count);

		va_end(arglist);
	}

	struct TextColor
	{
		char name[32];  /* ARRAY OK 2010-09-24 roarl */
		char color[32];  /* ARRAY OK 2010-09-24 roarl */
	};
	struct NumberColor
	{
		unsigned int number;
		char color[32];  /* ARRAY OK 2010-09-24 roarl */
	};

	OpAutoVector<TextColor> text_color;
	OpAutoVector<NumberColor> number_color;

	OpFile html_file;
};

#endif  // HTMLEXPORTER_H
