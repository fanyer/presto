/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#include "core/pch.h"

#if defined(SELFTEST) && defined (_FILE_UPLOAD_SUPPORT_)
# include "modules/forms/selftest/testhelpers.h"

uni_char* formtest_getcwd(uni_char* buffer, size_t maxlen)
{
	if (maxlen)
	{
		OpString tmp_storage;
		const uni_char* test_folder = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage);
		if (!test_folder)
		{
			OP_ASSERT(!"OPFILE_HOME_FOLDER missing");
			return NULL;
		}

		if (uni_strlen(test_folder) >= maxlen)
		{
			OP_ASSERT(!"Too long OPFILE_HOME_FOLDER path");
			return NULL;
		}

		uni_strcpy(buffer, test_folder);
		return buffer;
	}
	return NULL;
}

#endif // SELFTEST && _FILE_UPLOAD_SUPPORT_
