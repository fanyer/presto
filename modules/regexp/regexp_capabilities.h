/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2002-2006
 */

#ifndef REGEXP_CAPABILITIES_H
#define REGEXP_CAPABILITIES_H

#define REGEXP_CAP_PRESENT
	/**< Separate regexp module exists */

#define REGEXP_CAP_PREEMPTION
	/**< Supports preemption API on RegExp::Exec()  */

#define REGEXP_CAP_PREEMPTION_ACTUALLY_WORKS
	/**< Preemption actually works now */

#define REGEXP_CAP_PACKAGED_FLAGS
	/**< The RegExpFlags and OpREFlags structures exist, as do APIs that accept them  */

#define REGEXP_CAP_ECMASCRIPT4_API
	/**< RegExp::Exec has the ability to suspend and take the 'searching' flag */

#define JL_REGEXP
	/**< Use experimental engine instead. */

#endif // REGEXP_CAPABILITIES_H
