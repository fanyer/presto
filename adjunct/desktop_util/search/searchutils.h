/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SEARCHUTILS_H
#define SEARCHUTILS_H

#include "modules/util/opfile/opfile.h"

#ifndef NO_SEARCH_ENGINES
/**
 * Copies the source file to a backup file with the extension ".bak"
 * If the backup file already exists a new extention is added ranging
 * from ".000" to ".999". The last extension will always be used even
 * if the file already exists.
 *
 * @param src The source file
 * @param error The error code is returned here if non NULL
 *
 * @return TRUE on success, otherwise FALSE
 */
BOOL CreateFileBackup( OpFile& src, INT32 *error );

/**
 * Updates the personal resource file using the shared resource file if the latter
 * has a higher version number. The location of both files are modified if
 * 'test_language' is TRUE. If there is no language version of the file, then
 * the regular files will be used (as if test_language was FALSE)
 *
 * @return The personal resource file.
 */
OpFile* UpdateResourceFileL(const uni_char* filename, BOOL test_language);

#endif
#endif
