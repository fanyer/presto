/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_UTIL_UTIL_CAPABILITIES_H
#define MODULES_UTIL_UTIL_CAPABILITIES_H

/** Has OpTreeModel
 *
 * Still used (2011-01-24):
 * - linuxsdk/work: adjunct/desktop_util/
 */
#define UTIL_CAP_HAS_OPTREEMODEL

/** MyUniStrTok() uses int instead of short
 *
 * Still used (2011-01-24):
 * - mini_server/work: platforms/mini/
 */
#define UTIL_CAP_MYUNISTRTOK_INT

/** Has OpFile::GetLocalizedPath()
 *
 * Still used (2011-01-24):
 * - bream/opera: platforms/symbian/
 */
#define UTIL_CAP_LOCALIZED_PATH

/** OpZipFolder implements OpLowLevelFile::MakeDirectory()
 *
 * Still used (2011-01-24):
 * - bream/opera: platforms/symbian/
 */
#define UTIL_CAP_LLF_MAKEDIR

/** Has template classes {Counted}{AutoDelete}List{Element}\<E>
 *
 * Still used (2011-01-24):
 * - bream/opera: bream/vm/
 */
#define UTIL_CAP_LIST_TEMPLATES

/** Has OPFILE_INI_CUSTOM_FOLDER
 *
 * Still used (2011-01-24):
 * - desktop/work: adjunct/quick/
 */
#define UTIL_CAP_OPFILE_INI_CUSTOM_FOLDER

#endif // !MODULES_UTIL_UTIL_CAPABILITIES_H
