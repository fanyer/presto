/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "platforms/viewix/src/input_files/InputFile.h"

#include "modules/util/opfile/opfile.h"

OP_STATUS InputFile::Parse(const OpStringC & file_name)
{

    OpFile file;
    RETURN_IF_ERROR(OpenFile(file, file_name));
    return ParseInternal(file);
}

OP_STATUS InputFile::OpenFile(OpFile & file, const OpStringC & file_name)
{
    if(file_name.IsEmpty())
	return OpStatus::ERR;

    RETURN_IF_ERROR(file.Construct(file_name.CStr()));

    return file.Open(OPFILE_READ);
}
