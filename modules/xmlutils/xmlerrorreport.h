/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLERRORREPORT_H
#define XMLERRORREPORT_H

#ifdef XML_ERRORS

/**

  Class representing an XML error report.  If requested via
  XMLParser::Configuration::generate_error_report, an error report is
  generated when the parser detects a parse error.  The error report
  consists of a number of items, that together make up a number of
  contexts lines, and the culprit line(s) with tag items markin the
  start and end of the actual error.  For instance, when parsing the
  XML

    <!-- the pre element is not part of the example! -->

  <pre>
    <root>
      <element>
        Some text.
      </wrong>
    </root>
  </pre>

  an error report containing the items

  <pre>
    TYPE_LINE_END, "<root>"
    TYPE_LINE_FRAGMENT, "  "
    TYPE_INFORMATION_START
    TYPE_LINE_FRAGMENT, "<element>"
    TYPE_INFORMATION_END
    TYPE_LINE_END, ""
    TYPE_LINE_END, "    Some text."
    TYPE_LINE_FRAGMENT, "  "
    TYPE_ERROR_START
    TYPE_LINE_FRAGMENT, "</wrong>"
    TYPE_ERROR_END
    TYPE_LINE_END, ""
    TYPE_LINE_END, "</root>"
  </pre>

  is generated, meaning the "</wrong>" fragment is the error, and the
  "<element>" fragment is of some interest (in this case because it is
  the start tag that "</wrong>" didn't match.)  Most errors have no
  TYPE_INFORMATION_START or TYPE_INFORMATION_END tags, only
  TYPE_ERROR_START and TYPE_ERROR_END tags.

*/
class XMLErrorReport
{
public:
	/** Error report item.  There are two types of items: tags and
	    strings.  Each item has line and column information.  For
	    strings this specifies the position of their first character
	    in the source.  For tags that signal the start of something
	    this specifies the position of the first character of the next
	    string item.  For tags that signal the end of something, this
	    specifies the position past the last character of the previous
	    string item.  Tag items are always zero characters long.
	    Items always start and end on the same line. */
	class Item
	{
	public:
		enum Type
		{
			TYPE_ERROR_START,
			/**< Tag that signals the start of the/an error location.  No
			     string value. */

			TYPE_ERROR_END,
			/**< Tag that signals the end of the/an error location.
			     No string value. */

			TYPE_INFORMATION_START,
			/**< Tag that signals the start of an information
			     location.  No string value. */

			TYPE_INFORMATION_END,
			/**< Tag that signals the end of an information location.
			     No string value. */

			TYPE_LINE_FRAGMENT,
			/**< Part of a line, but not the last part.  Never zero
			     length. */

			TYPE_LINE_END
			/**< Last part of a line.  Line break characters are not
			     included in the string.  Can be zero length, if a tag
			     item is at the end of the line. */
		};

		Type GetType() const { return type; }
		/**< Returns the type of this item.

		     @return A type. */

		unsigned GetLine() const { return line; }
		/**< Returns the line number of this item.  The first line is
		     numbered zero.

		     @return A line number. */

		unsigned GetColumn() const { return column; }
		/**< Returns the column number of this item.  The first column
		     is numbered zero.  If the type of this item is
		     TYPE_LINE_FRAGMENT or TYPE_LINE_END, the column is the
		     column of the first character.

		     @return A column number. */

		const uni_char *GetString() const { return string; }
		/**< Returns the string value.  Only valid for the types
		     TYPE_LINE_FRAGMENT and TYPE_LINE_END.

		     @return A string or NULL if not valid. */

	protected:
		friend class XMLErrorReport;

		~Item();

		Type type;
		unsigned line, column;
		uni_char *string;
	};

	XMLErrorReport();
	/**< Constructor. */

	~XMLErrorReport();
	/**< Destructor. */

	unsigned GetItemsCount() const { return items_count; }
	/**< Returns the number of items in this report.

	     @return The number of items in this report. */

	const Item *GetItem(unsigned index) const { return items[index]; }
	/**< Returns the index-th item in this report.

	     @return A pointer to an Item. */

	OP_STATUS AddItem(Item::Type type, unsigned line, unsigned column, const uni_char *string = 0, unsigned string_length = ~0u);
	/**< Add an item to this report.

	     @param type The type of the item.
	     @param line The line number of the item.
	     @param column The column number of (the start of) the item.
	     @param string The string value.  Will be copied by this
	                   function.  Should be NULL (and is ignored) for
	                   item types other than TYPE_LINE_FRAGMENT and
	                   TYPE_LINE_END.
	     @param string_length The length of 'string'.  Is calculated
	                          if not specified, in which case 'string'
	                          must be null-terminated.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

protected:
	unsigned items_count, items_allocated;
	Item **items;
};

#endif // XML_ERRORS
#endif // XMLERRORREPORT_H
