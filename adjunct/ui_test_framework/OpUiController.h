/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#ifdef FEATURE_UI_TEST

class OpAccessibilityExtension;

/**
 * @brief A UI Controller
 * @author Patricia Aas
 */

class OpUiController
{
public:
    virtual ~OpUiController() {}

	virtual OpAccessibilityExtension* GetRootNode() = 0;
};

#endif // FEATURE_UI_TEST

#endif // UI_CONTROLLER_H
