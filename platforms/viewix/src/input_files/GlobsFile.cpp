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

#include "platforms/viewix/FileHandlerManager.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"
#include "platforms/viewix/src/input_files/GlobsFile.h"

#include "modules/util/opfile/opfile.h"

/***********************************************************************************
 ** ParseGlobsFile
 **
 ** "The glob files
 **
 **  This is a simple list of lines containing a MIME type and pattern, separated by
 **  a colon. For example:
 **
 **  # This file was automatically generated by the
 **  # update-mime-database command. DO NOT EDIT!
 **  ...
 **  text/x-diff:*.diff
 **  text/x-diff:*.patch
 **  ...
 **
 **  Applications MUST first try a case-sensitive match, then try again with the filename
 **  converted to lower-case if that fails. This is so that main.C will be seen as a C++
 **  file, but IMAGE.GIF will still use the *.gif pattern.
 **
 **  If several patterns match then the longest pattern SHOULD be used. In particular,
 **  files with multiple extensions (such as Data.tar.gz) MUST match the longest sequence
 **  of extensions (eg '*.tar.gz' in preference to '*.gz'). Literal patterns (eg, 'Makefile')
 **  must be matched before all others. It is suggested that patterns beginning with `*.'
 **  and containing no other special uni_characters (`*?[') should be placed in a hash table
 **  for efficient lookup, since this covers the majority of the patterns. Thus, patterns of
 **  this form should be matched before other wildcarded patterns.
 **
 **  There may be several rules mapping to the same type. They should all be merged. If the
 **  same pattern is defined twice, then they MUST be ordered by the directory the rule came
 **  from, as described above.
 **
 **  Lines beginning with `#' are comments and should be ignored. Everything from the `:'
 **  uni_character to the newline is part of the pattern; spaces should not be stripped. The
 **  file is in the UTF-8 encoding. The format of the glob pattern is as for fnmatch(3). The
 **  format does not allow a pattern to contain a literal newline uni_character, but this is
 **  not expected to be a problem.
 **
 **  Common types (such as MS Word Documents) will be provided in the X Desktop Group's
 **  package, which MUST be required by all applications using this specification. Since each
 **  application will then only be providing information about its own types, conflicts should
 **  be rare."
 **
 **  Ref: http://standards.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-0.13.html#id2506119
 ***********************************************************************************/
OP_STATUS GlobsFile::ParseInternal(OpFile & file)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //File pointer cannot be null
    OP_ASSERT(file.IsOpen());
    if(!file.IsOpen())
	return OpStatus::ERR;
    //-----------------------------------------------------

    //Split file into lines - only if smaller than 10MB
    UINT32 max_size = 10000000;
    OpAutoVector<OpString> list;
    FileHandlerManagerUtilities::FileToLineVector(&file, list, max_size);

    for(UINT32 i = 1; i < list.GetCount(); i++)
    {
		OpString* line = list.Get(i);
		
		if(line->FindFirstOf('#') == 0)
			continue;
		
		OpAutoVector<OpString> items;
		FileHandlerManagerUtilities::SplitString(items, *line, ':');
		
		if(items.GetCount() == 2)
		{
			OpString * type = items.Get(0);
			OpString * g    = items.Get(1);
			
			const uni_char * mime_type = type->CStr();
			const uni_char * glob      = g->CStr();
			
			if (!mime_type || !glob)
				continue;
			
			if (FileHandlerManagerUtilities::LiteralGlob(glob))
			{
				FileHandlerManager::GetManager()->MakeGlobNode(glob, mime_type, GLOB_TYPE_LITERAL);
			}
			else if (FileHandlerManagerUtilities::SimpleGlob(glob))
			{
				FileHandlerManager::GetManager()->MakeGlobNode(glob, mime_type, GLOB_TYPE_SIMPLE);
			}
			else
			{
				FileHandlerManager::GetManager()->MakeGlobNode(glob, mime_type, GLOB_TYPE_COMPLEX);
			}
		}
		else
		{
            //Error in reading line - should have two elements
			OP_ASSERT(FALSE);
		}
    }
	
    return OpStatus::OK;
}