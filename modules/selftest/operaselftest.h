/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Anders Oredsson
 */
#if !defined MODULES_SELFTEST_OPERASELFTEST_H && defined SELFTEST
#define MODULES_SELFTEST_OPERASELFTEST_H

#include "modules/about/opgenerateddocument.h"

/**
 * Generator for the opera:selftest document.
 * The opera:selftest page lets you run selftests from opera.
 *
 * The page calls the operaselftestdispatcher trough registered
 * ecma callbacks in the opera.* DOM object, and the dispatcher
 * will run the tests / keep test-output, and return these upon request.
 *
 * opera:selftest uses stylesheet selftest.css and script selftest.js
 * from the about module.
 *
 * IMPORTANT: to set up new default values for directory/publish-server,
 * simply change the OnLoad() function in selftest.js
 */
class OperaSelftest : public OpGeneratedDocument
{
public:
	/**
	 * Constructor for the selftest page generator.
	 *
	 * @param url URL to write to.
	 */
	OperaSelftest(URL &url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		{}

	/**
	 * Generate the opera:selftest document to the specified internal URL.
	 *
	 * The page contains:
	 * - Head (link to selftest.css and selftest.js)
	 * - BodyHeadForm (the form at the top of the page)
	 * - BodyTables (the module list and the output)
	 * - BodyFooterForm (the publish for at the bottom of the page)
	 *
	 * important element names / hirarchy (for css and js integration):
	 * div id=header
	 *		div id=toolbar
	 *			form f
	 *				input a //argument
	 *				input r //run button
	 *				input s //scroll checkbox
	 *				input outputfile //file output
	 *				input runmanual //run manual tests checkbox
	 *				input testdata //custom testdatasource input
	 *				input override //override testdatasource checkbox
	 * pre id=output //main text output
	 * div id=testselector //module list
	 *		input runalltests //checkbox for running all test
	 *		input [modulename] //checkbox for selecting module name
	 * div id=footer
	 *		form publishToServer
	 *			input publishToHost	//what host do we publish to?
	 *			input publishOutputAsField //what field should the output be on the POSTed form?
	 *			input submit //submit button
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

protected:
	OP_STATUS WriteHead();
	OP_STATUS WriteJavascript();
	OP_STATUS WriteBody();
	OP_STATUS WriteBodyHeaderForm();
	OP_STATUS WriteBodyTables();
	OP_STATUS WriteBodyFooterForm();
	OP_STATUS WriteFinal();
};

#endif
