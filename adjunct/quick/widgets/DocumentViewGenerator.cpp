/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Deepak Arora (deepaka)
 */
#include "core/pch.h"

#include "adjunct/quick/widgets/DocumentViewGenerator.h"
#include "adjunct/quick/ClassicApplication.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/managers/SpeedDialManager.h"

#include "modules/dochand/win.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/about/opgenerateddocument.h"

OpAutoVector<DocumentViewGenerator> DocumentViewGenerator::m_generator_pool;

OP_STATUS DocumentViewGenerator::QuickGenerate(URL &url, OpWindowCommander* commander)
{
	class GeneratedDocument
		: public OpGeneratedDocument
	{
	public:
		OpString m_title;

		GeneratedDocument(URL url)
			: OpGeneratedDocument(url)
		{
			OpStatus::Ignore(g_languageManager->GetString(Str::LocaleString(Str::SI_BLANK_PAGE_NAME), m_title));
		}

		virtual OP_STATUS GenerateData()
		{
			RETURN_IF_ERROR(OpenDocument(m_title));
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, NULL));
			return CloseDocument();
		}
	};

	DesktopWindow* doc_win = g_application->GetDesktopWindowCollection().GetDocumentWindowFromCommander(commander);
	if (!doc_win || doc_win->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT)
		doc_win = g_application->GetActiveDocumentDesktopWindow();

	GeneratedDocument gen_doc(url);

	OpString url_str;
	RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_Username_Password_Hidden, url_str));
	RETURN_IF_ERROR(CreateDocumentView(static_cast<DocumentDesktopWindow*>(doc_win), url_str.CStr(), gen_doc.m_title));
	return gen_doc.GenerateData();
}

OP_STATUS SpeedDialViewGenerator::CreateDocumentView(DocumentDesktopWindow* doc_win, const uni_char* url_str,  OpString& doc_title)
{
	if (g_speeddial_manager->GetState() == SpeedDialManager::Disabled || g_speeddial_manager->GetState() == SpeedDialManager::Folded)
		return OpStatus::OK; // This would at least generate blank page

	OpStatus::Ignore(g_languageManager->GetString(Str::S_SPEED_DIAL, doc_title));

	DocumentView::DocumentViewTypes type = DocumentView::DOCUMENT_TYPE_SPEED_DIAL;
	if (!doc_win->GetDocumentViewFromType(type))
		RETURN_IF_ERROR(doc_win->CreateDocumentView(type));

	return doc_win->SetActiveDocumentView(type);
}

#ifdef NEW_BOOKMARK_MANAGER
OP_STATUS CollectionViewGenerator::CreateDocumentView(DocumentDesktopWindow* doc_win, const uni_char* url_str, OpString& doc_title)
{
	OpStatus::Ignore(g_languageManager->GetString(Str::S_NAVIGATION_PANE_TITLE, doc_title));

	DocumentView::DocumentViewTypes type = DocumentView::DOCUMENT_TYPE_COLLECTION;
	if (!doc_win->GetDocumentViewFromType(type))
		RETURN_IF_ERROR(doc_win->CreateDocumentView(type));

	return doc_win->SetActiveDocumentView(type);
}
#endif

OP_STATUS DocumentViewGenerator::InitDocumentViewGenerator()
{
#ifdef NEW_BOOKMARK_MANAGER
	RETURN_IF_ERROR(RegisterDocumentGenerator(OP_NEW(CollectionViewGenerator, ()), "collectionview"));
#endif //NEW_BOOKMARK_MANAGER
	return RegisterDocumentGenerator(OP_NEW(SpeedDialViewGenerator, ()), "speeddial");
}

OP_STATUS DocumentViewGenerator::RegisterDocumentGenerator(DocumentViewGenerator* generator, const OpStringC8 &uri)
{
	OpAutoPtr<DocumentViewGenerator> holder;
	holder.reset(generator);
	RETURN_OOM_IF_NULL(holder.get());
	RETURN_IF_ERROR(holder->Construct(uri, FALSE));
	RETURN_IF_ERROR(m_generator_pool.Add(holder.get()));
	g_url_api->RegisterOperaURL(holder.get());
	holder.release();
	return OpStatus::OK;
}
