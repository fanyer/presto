/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#if !defined ACCESSORS_XML_H && defined PREFS_HAS_XML
#define ACCESSORS_XML_H

#include "modules/prefsfile/accessors/prefsaccessor.h"

class PrefsMap;
class OpFileDescriptor;
class XMLFragment;

/**
 * XML file parser. A class able to parse files containing preferences
 * stored as a simple XML document. The XML format is specified by Opera
 * and looks as follows:
 *
 * \code
 * <preferences>
 *  <section id="food">
 *   <value id="fruit">banana</value>
 *   <value id="fruit">beef</value>
 *  </section>
 * </preferences>
 * \endcode
 */
class XmlAccessor : public PrefsAccessor
{
public:

	XmlAccessor()
		{}

	/**
	 * Loads and parses the content in the file and
	 * put the result in the map.
	 * @param file The file to load.
	 * @param map The map to put the result in.
	 */
	virtual OP_STATUS LoadL(OpFileDescriptor *file, PrefsMap *map);

#ifdef PREFSFILE_WRITE
	/**
	 * Saves the content in the map to the file.
	 * @param file The file to save to.
	 * @param map The map to save.
	 */
	virtual OP_STATUS StoreL(OpFileDescriptor *file, const PrefsMap *map);
#endif // PREFSFILE_WRITE

private:
	/**
	 * Load the contents of a section. The <section> element is the
	 * current element in the XML fragment supplied to the method.
	 * LoadSectionL() will read the section, but will not leave the
	 * section element.
	 *
	 * @param fragment XMLFragment positioned at the start of the section
	 *                 to be read.
	 * @param map      Output map.
	 */
	void LoadSectionL(XMLFragment *fragment, PrefsMap *map);
};

#endif // !ACCESSORS_XML_H && PREFS_HAS_XML
