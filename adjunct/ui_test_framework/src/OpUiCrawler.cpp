/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#ifdef FEATURE_UI_TEST

#include "adjunct/ui_test_framework/OpUiCrawler.h"
#include "adjunct/ui_test_framework/OpUiController.h"
#include "modules/pi/OpAccessibilityExtension.h"
#include "modules/accessibility/OpAccessibilityExtensionListener.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/adt/bytebuffer.h"
#include "adjunct/ui_test_framework/OpUiTree.h"

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpUiCrawler::OpUiCrawler(OpUiController* controller) :
	m_controller(controller)
{
	OP_ASSERT(m_controller);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpUiCrawler::~OpUiCrawler()
{

}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiCrawler::PassiveExplore()
{
	// Create the tree
	OpUiTree tree;
	RETURN_IF_ERROR(tree.BuildTree(m_controller->GetRootNode()));

	// Output the tree
	XMLFragment fragment;
	RETURN_IF_ERROR(StartExport(fragment));
	RETURN_IF_ERROR(tree.Export(fragment));
	RETURN_IF_ERROR(StopExport(fragment));
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiCrawler::ActiveExplore()
{
	// Create the tree
	OpUiTree tree;
	RETURN_IF_ERROR(tree.BuildTree(m_controller->GetRootNode()));

	tree.ClickAll();

	// Output the tree
	XMLFragment fragment;
	RETURN_IF_ERROR(StartExport(fragment));
	RETURN_IF_ERROR(tree.Export(fragment));
	RETURN_IF_ERROR(StopExport(fragment));
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiCrawler::StartExport(XMLFragment& fragment)
{
	// Any global data
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiCrawler::StopExport(XMLFragment& fragment)
{
	// -----------------
	// Get the xml into a ByteBuffer
	// -----------------

	ByteBuffer buffer;
	RETURN_IF_ERROR(fragment.GetEncodedXML(buffer, TRUE, "utf-8", TRUE));

	// -----------------
	// Open the file
	// -----------------
	OP_STATUS status;
	OpFile file;
	TRAP(status, g_pcfiles->GetFileL(PrefsCollectionFiles::UiCrawlerFile, file));
	RETURN_IF_ERROR(status);
	RETURN_IF_ERROR(file.Open(OPFILE_WRITE));

	// -----------------
	// Write the ByteBuffer to the file, one chunk at a time
	// -----------------

	for(UINT32 i = 0; i < buffer.GetChunkCount(); i++)
	{
		unsigned int bytes = 0;
		char* chunk_ptr = buffer.GetChunk(i, &bytes);

		if(chunk_ptr)
			RETURN_IF_ERROR(file.Write(chunk_ptr, bytes));
	}

	// -----------------
	// Close the file
	// -----------------
	file.Close();

	return OpStatus::OK;
}

#endif // FEATURE_UI_TEST
