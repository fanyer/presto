/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_INDEX_THEME_FILE_H__
#define __FILEHANDLER_INDEX_THEME_FILE_H__

#include "platforms/viewix/src/input_files/InputFile.h"

class OpFile;

class IndexThemeFile : public InputFile
{
 public:

    IndexThemeFile(ThemeNode * node) : m_node(node) {}

 protected:

    OP_STATUS ParseInternal(OpFile & file);

 private:

    ThemeNode * m_node;
};

#endif //__FILEHANDLER_INDEX_THEME_FILE_H__
