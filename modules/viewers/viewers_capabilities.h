/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Capability defines for the viewers module.
 *
 * Defines that can be tested for to adapt other code to deal with
 * different versions of the viewers module.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#ifndef VIEWERS_CAPABILITIES_H
#define VIEWERS_CAPABILITIES_H

/**
 * \brief The viewers module is present
 *
 * This macro is defined when the viewers module is present, and can be
 * used to fetch the include files from the right place, for instance.
 */
#define VIEWERS_CAP_MODULE

/**
 * \brief Enable the Viewer version 2 API elsewhere in the sourcecode
 *
 * This macro was not a capability when the viewers code was in
 * softcore, but has become a capability when the viewers module was
 * introduced: The viewers module does only support the version 2 API.
 */
#define SOFTCORE_API_VIEWER2

#endif // VIEWERS_CAPABILITIES_H
