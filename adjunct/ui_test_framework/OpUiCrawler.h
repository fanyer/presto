/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef UI_CRAWLER_H
#define UI_CRAWLER_H

#ifdef FEATURE_UI_TEST

#include "modules/pi/OpAccessibilityExtension.h"

class OpUiController;
class OpAccessibilityExtension;
class XMLFragment;


/**
 * @brief A UI Crawler
 * @author Patricia Aas
 */

class OpUiCrawler
{
public:
	OpUiCrawler(OpUiController* controller);

	~OpUiCrawler();

	OP_STATUS PassiveExplore();

	OP_STATUS ActiveExplore();

private:

	OP_STATUS ExploreNode(OpAccessibilityExtension* node, XMLFragment& fragment);
	OP_STATUS StartExport(XMLFragment& fragment);
	OP_STATUS StopExport(XMLFragment& fragment);

	OpUiController* m_controller;
};

#endif // FEATURE_UI_TEST

#endif // UI_CRAWLER_H
