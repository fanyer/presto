/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Deepak Arora (deepaka)
 */
#ifndef DOCUMENT_VIEW_GENERATOR
#define DOCUMENT_VIEW_GENERATOR

#include "adjunct/quick/widgets/DocumentView.h"

class DocumentDesktopWindow;

class DocumentViewGenerator
	: public OperaURL_Generator
{
public:
	static OP_STATUS InitDocumentViewGenerator();

protected:
	// OperaURL_Generator
	virtual OperaURL_Generator::GeneratorMode GetMode() const { return OperaURL_Generator::KQuickGenerate; }
	virtual OP_STATUS QuickGenerate(URL &url, OpWindowCommander* commader);

	virtual OP_STATUS CreateDocumentView(DocumentDesktopWindow* doc_win, const uni_char* url_str, OpString& doc_title) = 0;

private:
	/** Registers given documentview generator with core.
	 *
	 *  @param generator identifies documentview generator.
	 *  @param p_name The name of the opera: URL path to be matched.
	 *  The name must contain any slashes that are part of the name.
	 */
	static OP_STATUS RegisterDocumentGenerator(DocumentViewGenerator* generator, const OpStringC8 &uri);
	static OpAutoVector<DocumentViewGenerator> m_generator_pool;
};

class SpeedDialViewGenerator
	 : public DocumentViewGenerator
{
	// DocumentViewGenerator
	virtual OP_STATUS CreateDocumentView(DocumentDesktopWindow* doc_win, const uni_char* url_str, OpString& doc_title);
};

#ifdef NEW_BOOKMARK_MANAGER
class CollectionViewGenerator
	: public DocumentViewGenerator
{
	// DocumentViewGenerator
	virtual OP_STATUS CreateDocumentView(DocumentDesktopWindow* doc_win, const uni_char* url_str, OpString& doc_title);
};
#endif
#endif // DOCUMENT_VIEW_GENERATOR
