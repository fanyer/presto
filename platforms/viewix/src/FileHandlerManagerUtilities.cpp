/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "platforms/viewix/src/FileHandlerManagerUtilities.h"
#include "platforms/viewix/src/HandlerElement.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/str.h"

#ifdef QUICK
# include "adjunct/quick/dialogs/SimpleDialog.h"
# include "adjunct/quick/dialogs/PreferencesDialog.h" // Open a specific page
# include "adjunct/desktop_util/string/stringutils.h"
# include "adjunct/desktop_util/image_utils/iconutils.h"
# include "modules/prefs/prefsmanager/collections/pc_doc.h"
# include "platforms/quix/environment/DesktopEnvironment.h"
#endif // QUICK

#ifdef UNIX
# include "platforms/unix/base/common/externalapplication.h"
# include "platforms/unix/base/common/applicationlauncher.h"
# include "platforms/posix/posix_native_util.h" // Posix
#endif // UNIX

/***********************************************************************************
 ** Modifies in_string so that if suffix appears in it everything from start of suffix
 ** on is removed from the string.
 **
 **	@param the string and the suffix to be removed
 ** @return TRUE if the string was altered, FALSE otherwise
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::RemoveSuffix(OpString& in_string,
											   const OpStringC & suffix)
{
	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	// The strings cannot be null:
	OP_ASSERT(suffix.HasContent());
	if(suffix.IsEmpty())
		return FALSE;

	OP_ASSERT(in_string.HasContent());
	if(in_string.IsEmpty())
		return FALSE;
	//-----------------------------------------------------

	const uni_char* heystack = in_string.CStr();

	INT32 s_i = uni_strlen(suffix.CStr())   - 1;
	INT32 h_i = uni_strlen(heystack) - 1;

	//If the suffix is longer than the string it cannot be there
	if(s_i > h_i)
		return FALSE;

	//Make sure the string ends with the suffix
	for( ; s_i >= 0 && h_i >= 0; s_i--, h_i--)
	{
		if(suffix[s_i] != heystack[h_i])
			break;
	}

	//If we went through the whole suffix then we have a match:
	INT32 index = (s_i == -1) ? (h_i+1) : -1;

	//Strip off the suffix:
	if(index >= 0)
	{
		in_string.Set(in_string.CStr(), index);
		return TRUE;
	}

	return FALSE;
}

/***********************************************************************************
 ** Modifies in_string so that if prefix is a prefix of in_string, it is removed.
 **
 ** @param the string and the prefix to be removed
 **
 ** @return TRUE if the string was altered, FALSE otherwise
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::RemovePrefix(OpString& in_string,
											   const OpStringC & prefix)
{
	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	OP_ASSERT(prefix.HasContent());
	if(prefix.IsEmpty())
		return FALSE;
	//-----------------------------------------------------

	if(0 == in_string.Find(prefix))
	{
		unsigned int len = uni_strlen(prefix.CStr());
		in_string.Set(in_string.SubString(len));
		return TRUE;
	}
	return FALSE;
}


/***********************************************************************************
 ** If in_string starts with prefix, this prefix will be replaced by new_prefix.
 **
 ** @param the string and the old and new prefix
 **
 ** @return TRUE if the string was altered, FALSE otherwise
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::ReplacePrefix(OpString& in_string,
												const OpStringC & prefix,
												const OpStringC & new_prefix)
{
	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	OP_ASSERT(prefix.HasContent() && new_prefix.HasContent());
	if(prefix.IsEmpty() || new_prefix.IsEmpty())
		return FALSE;
	//-----------------------------------------------------

	if(RemovePrefix(in_string, prefix))
	{
		in_string.Insert(0, new_prefix);
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns a pointer to the start of the last name component in filename.
 *
 * @param filename possibly including a path to be skipped over
 *
 * @return the first position in filename after which no '/' appears
 */
static const uni_char* BaseName(const OpStringC & filename)
{
	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
//	OP_ASSERT(filename.HasContent());
	if(filename.IsEmpty())
		return 0;

	const uni_char * name = filename.CStr();

	for (int i = 0; name[i]; )
		if (name[i] == '/')
		{
			name += i+1;
			i = 0;
		}
		else
			i++;

	return name;
}


/**
 * Sets result to the BaseName of the given filename.
 */
OP_STATUS FileHandlerManagerUtilities::StripPath(OpString & result,
												 const OpStringC & filename)
{
	return result.Set(BaseName(filename.CStr()));
}


/***********************************************************************************
 ** Returns a string where everything up to the last slash in filename is preserved.
 **
 ** @param filename from which path should be extracted
 **
 ** @return the extracted path (directory name part of filename)
 **
 ***********************************************************************************/
OP_STATUS FileHandlerManagerUtilities::GetPath(const OpStringC & filename,
											   OpString & path)
{
	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	OP_ASSERT(filename.HasContent());
	if(filename.IsEmpty())
		return 0;
	//-----------------------------------------------------

	const uni_char * name = BaseName(filename);
	// Must have a string to extract a path
	if(!name)
		return 0;

	return path.Set(filename.CStr(), name - filename.CStr());
}

/***********************************************************************************
 ** Determines if glob is a literal glob, meaning that it does not contain any special
 ** characters.  For some reason we treat empty as a non-literal glob.
 **
 **	@param the glob to be analyzed
 ** @return TRUE if glob is literal, FALSE otherwise
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::LiteralGlob(const OpStringC & glob)
{
	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	OP_ASSERT(glob.HasContent());
	if (glob.IsEmpty())
		return FALSE;

	for (int i = 0; glob[i]; i++)
		switch (glob[i])
		{
		case '[': case ']': case '*': case '\\':
		case '?': case '!': case '^': case '-':
			return FALSE;
		}

	return TRUE;
}

/***********************************************************************************
 ** Determines if glob is a simple glob, meaning that it does not contain any special
 ** characters after "*." and that it does indeed start with "*.".
 **
 **	@param the glob to be analyzed
 ** @return TRUE if glob is simple, FALSE otherwise
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::SimpleGlob(const OpStringC & glob)
{
	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	OP_ASSERT(glob.HasContent());
	if (glob.IsEmpty() || glob[0] != '*' || glob[1] != '.')
		return FALSE;

	return glob[2] == '\0' || LiteralGlob(glob.CStr() + 2);
	// LiteralGlob(glob + 2) would return false if glob were simply "*." because it treats empty as non-literal ...
}

/***********************************************************************************
 ** Checks whether dirName is the full path to a directory (uses S_ISDIR)
 **
 **
 **
 **
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::IsDirectory(const OpStringC & dirName)
{
	PosixNativeUtil::NativeString path (dirName.CStr());
	struct stat buf;
	return path.get() &&
		stat(path.get(), &buf) == 0 &&
		S_ISDIR(buf.st_mode);
}

/***********************************************************************************
 ** Checks whether fileName is the full path to a regular file (uses S_ISREG)
 **
 **
 **
 **
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::IsFile(const OpStringC & fileName)
{
	PosixNativeUtil::NativeString path (fileName.CStr());
	struct stat buf;
	return path.get() &&
		stat(path.get(), &buf) == 0 &&
		S_ISREG(buf.st_mode);
}


/***********************************************************************************
 ** Makes an array of simple globs from the file extension. Ex: file.tar.gz
 ** results in *.tar.gz and *.gz.
 **
 ** @param filename to be processed for making globs
 **	@param pointer to int to give number of globs found
 **
 ** @return array of globs produced
 ** NOTE: The array returned and the strings in it are the caller's resposibility to delete
 ***********************************************************************************/
OP_STATUS FileHandlerManagerUtilities::MakeGlobs(const OpStringC & filename,
												 OpVector<OpString> & globs)
{
	OpAutoVector<OpString> substrings;

	SplitString(substrings, filename, '.');

	UINT32 i = 0;

	if(substrings.GetCount() == 0)
		return OpStatus::ERR;

	for(i = 1; i < substrings.GetCount(); i++)
	{
		OpString * s = OP_NEW(OpString, ());
		OP_STATUS err = s->Set("*");

		for(UINT32 j = i; OpStatus::IsSuccess(err) && j < substrings.GetCount(); j++)
		{
			OpString * sub = substrings.Get(j);

			err = s->Append(".");
			if (OpStatus::IsSuccess(err))
				err = s->Append(sub->CStr());
		}

		if (OpStatus::IsSuccess(err))
			err = globs.Add(s);
		if (OpStatus::IsError(err))
		{
			OP_DELETE(s);
			return err;
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Tests whether a file exists on $PATH with the given mode. If it does the method
 ** will return true and target set to the full path of the file.
 **
 ** @param candidate - the file name to be searched for
 ** @param mode      - the mode to test for if found
 ** @param target    - reference to string where full path will be put if candidate
 **                    is found
 **
 ** @returns TRUE if file was found with correct mode (else returns FALSE)
 **
 ** [espen - Espen Sand]
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::ExpandPath(const OpStringC & candidate,
											 int mode,
											 OpString& target)
{
	PosixNativeUtil::NativeString candidate_path (candidate.CStr());

	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	OP_ASSERT(candidate.HasContent());
	if (candidate.IsEmpty() || !candidate_path.get())
		return FALSE;
	//-----------------------------------------------------
	
	if (access(candidate_path.get(), mode) == 0)
		return OpStatus::IsSuccess(target.Set(candidate));

	OpString8 path_env;
	path_env.Set(op_getenv("PATH"));
	if (path_env.IsEmpty())
		return FALSE;

	const char* path = strtok(path_env.CStr(), ":");
	while( path )
	{
		OpString8 tmp;
		RETURN_VALUE_IF_ERROR(tmp.AppendFormat("%s%c%s", path, PATHSEPCHAR, candidate_path.get()), FALSE);

		if (access(tmp.CStr(), mode) == 0)
		{
			RETURN_VALUE_IF_ERROR(PosixNativeUtil::FromNative(tmp.CStr(), &target), FALSE);
			return TRUE;
		}
		path = strtok(0, ":");
	}
	return FALSE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::LiteralWord(const OpString& str)
{
	return (str.Find("%") < 0);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::OptionalWord(const OpString& str)
{
	return LiteralWord(str) && (str.Find("-") == 0);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::Option(const OpString& str)
{
	return (str.Find("%") >= 0);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::OptionalPair(const OpString& str)
{
	return (str.Find(" ") > 1) && (str.Find("-") == 0);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::Acceptable(const OpString& str)
{
	return (str.Find(UNI_L("%f")) >= 0 || // file
			str.Find(UNI_L("%F")) >= 0 || // files
			str.Find(UNI_L("%u")) >= 0 || // url
			str.Find(UNI_L("%U")) >= 0); // urls
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerManagerUtilities::ParseCommand(const OpString& command,
													OpVector<OpString>& items)
{
	OpAutoVector<OpString> tokens;
	SplitString(tokens, command, ' ');
	return ParseCommand(tokens, items);
}

/***********************************************************************************
 ** Comment: The standard is not very clear on how this line should be parsed
 **          - given this lack I will here try to specify how I will parse it.
 **
 **          W  - (Word) sequence of char not including spaces
 **          LW - (Literal Word) a word and not containing '%'
 **          OW - (Option Word) a Literal Word starting with a '-'
 **          O  - (Option) a word containing '%'
 **          OP - (Option Pair) an Option Word and an Option
 **
 ** Yealding the following expression:
 **
 **          [LW]* [OP | O]*
 **
 ** That is:  a possible sequence of Literal Words followed by a possible sequence
 ** containing Optional Pairs and/or Options
 ***********************************************************************************/
OP_STATUS FileHandlerManagerUtilities::ParseCommand(OpVector<OpString>& tokens,
													OpVector<OpString>& items)
{
	UINT32 index = 0;

	OpString * literal_words = OP_NEW(OpString, ());

	for( ; index < tokens.GetCount(); index++)
	{
		OpString * t = tokens.Get(index);

		if( LiteralWord(*t) &&
		   !OptionalWord(*t))
		{
			if(literal_words->Length())
				literal_words->Append(" ");
			literal_words->Append(t->CStr());
		}
		else
			break;
	}

	items.Add(literal_words);

	OpString * optional_pair = 0;

	for( ; index < tokens.GetCount(); index++)
	{
		OpString * t = tokens.Get(index);

		if(optional_pair && Option(*t))
		{
			optional_pair->Append(" ");
			optional_pair->Append(t->CStr());
			items.Add(optional_pair);
			optional_pair = 0;
			continue;
		}
		else if(optional_pair && OptionalWord(*t)) // Single option
		{
			items.Add(optional_pair);
			optional_pair = 0;
		}
		else if(optional_pair)
		{
			optional_pair->Append(" ");
			optional_pair->Append(t->CStr());
			continue;
		}

		if(OptionalWord(*t))
		{
			optional_pair = OP_NEW(OpString, ());
			optional_pair->Append(t->CStr());
		}
		else if(Option(*t))
		{
			OpString * copy = OP_NEW(OpString, ());
			copy->Set(t->CStr());
			items.Add(copy);
		}
		else
		{
			OpString * copy = OP_NEW(OpString, ());
			copy->Set(t->CStr());
			items.Add(copy);
		}
	}

	if(optional_pair)
		items.Add(optional_pair);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
BOOL FileHandlerManagerUtilities::TakesUrls(const OpString& command)
{
	OpAutoVector<OpString> items;
	ParseCommand(command, items);

	for(UINT32 i = 0; i < items.GetCount(); i++)
	{
		OpString * t = items.Get(i);

		if( (t->Find(UNI_L("%u")) >= 0) || (t->Find(UNI_L("%U")) >= 0) )
			return TRUE;
	}

	return FALSE;
}

/***********************************************************************************
 **
 **
 **
 **
 ** Caller needs to delete this string
 ***********************************************************************************/
OP_STATUS FileHandlerManagerUtilities::MakePixelString(UINT32 icon_size,
													   OpString & out_string)
{
	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	OP_ASSERT(icon_size <= 128 && icon_size >= 16);
	if(icon_size > 128 || icon_size < 16)
		return OpStatus::ERR;
	//-----------------------------------------------------

	char height[4];
	char width[4];

	op_sprintf(height, "%d", icon_size);
	op_sprintf(width, "%d",  icon_size);

	OpString pixel_str;
	pixel_str.Append(height);
	pixel_str.Append("x");
	pixel_str.Append(width);

	return out_string.Set(pixel_str.CStr());
}


/***********************************************************************************
 ** Extracts the first word of a command string.
 **
 ** @param the command string
 **
 ** @return the command to start the application - extracted from the command string
 **
 **
 ** Caller needs to delete this string
 ***********************************************************************************/
OP_STATUS FileHandlerManagerUtilities::GetCleanCommand(const OpString& command,
													   OpString & clean_command)
{
	OpAutoVector<OpString> items;

	FileHandlerManagerUtilities::ParseCommand(command, items);

	OpString clean_comm;

	if(items.GetCount())
	{
		OpString * t = items.Get(0);
		clean_comm.Append(t->CStr());
	}

	for(UINT32 i = 1; i < items.GetCount(); i++)
	{
		OpString * t = items.Get(i);

		if(FileHandlerManagerUtilities::OptionalPair(*t))
		{
			if(FileHandlerManagerUtilities::Acceptable(*t))
			{
				if(clean_comm.Length())
					clean_comm.Append(" ");

				//Since we don't do replacement:
				if(t->Find(" ") > 0)
					t->Delete(t->Find(" "));

				clean_comm.Append(t->CStr());
				break;
			}
		}
		else if(FileHandlerManagerUtilities::Option(*t))
		{
			if(FileHandlerManagerUtilities::Acceptable(*t))
			{
				// If we would do replacement :
/*
  if(clean_comm.Length())
  clean_comm.Append(" ");

  clean_comm.Append(t->CStr());
*/
				break;
			}
		}
		else
			break; //ERROR - but this is only experimental
	}

	return clean_command.Set(clean_comm.CStr());
}

/***********************************************************************************
 ** FileToLineVector
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerManagerUtilities::FileToLineVector(OpFile* file,
														OpVector<OpString>& lines,
														UINT32 max_size)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //File pointer cannot be null
    OP_ASSERT(file);
    if(!file)
		return OpStatus::OK;
    //-----------------------------------------------------

    OpFileLength size;
	RETURN_IF_ERROR(file->GetFileLength(size));

    if( size > 0 && size < max_size )
    {
		char* buffer = OP_NEWA(char, size+1);

		if( buffer )
		{
			OpFileLength bytes_read = 0;
			file->Read(buffer,size, &bytes_read);
			buffer[size]='\0';
		}
		else
			return OpStatus::ERR_NO_MEMORY;

		OpString candidate;
		candidate.SetFromUTF8(buffer);

		SplitString(lines, candidate, '\n');

		OP_DELETEA(buffer);
    }

    return OpStatus::OK;
}

OP_STATUS FileHandlerManagerUtilities::GetTerminalCommand(OpString & command)
{
	OpString term;
	term.Set(op_getenv("TERM"));
	term.Strip();

	if(term.Compare("konsole") == 0)
	{
		return command.Set("konsole --noclose -e ");
	}
	else if(term.Compare("gnome-terminal") == 0)
	{
		return command.Set("gnome-terminal -x ");
	}
	else if(term.Compare("rxvt") == 0)
	{
		return command.Set("rxvt -hold -e ");
	}
	else if(term.Compare("urxvt") == 0)
	{
		return command.Set("urxvt -hold -e ");
	}
	else if(term.Compare("eterm") == 0)
	{
		return command.Set("eterm --pause ");
	}
	else if(term.Compare("aterm") == 0)
	{
		return command.Set("aterm -hold -e ");
	}
	else
	{
		return command.Set("xterm -hold -e ");
	}
}

BOOL FileHandlerManagerUtilities::isKDERunning()
{
#ifdef QUICK
	return DesktopEnvironment::GetInstance().GetToolkitEnvironment() == DesktopEnvironment::ENVIRONMENT_KDE;
#else
#warning "isKDERunning not implemented"
	return FALSE;
#endif // QUICK
}

BOOL FileHandlerManagerUtilities::isGnomeRunning()
{
#ifdef QUICK
	return DesktopEnvironment::GetInstance().GetToolkitEnvironment() == DesktopEnvironment::ENVIRONMENT_GNOME;
#else
#warning "isGnomeRunning not implemented"
	return FALSE;
#endif // QUICK
}

OP_STATUS Split(OpVector<OpString>& list,
				const OpStringC &candidate,
				int sep,
				BOOL keep_strings)

{
	OpString search_string;
	RETURN_IF_ERROR(search_string.Set(candidate.CStr()));

    int pos = 0;
    BOOL done = FALSE;
	OpString seperator;
	if(!seperator.Reserve(2))
		return OpStatus::ERR_NO_MEMORY;

	seperator.CStr()[0] = sep;
	seperator.CStr()[1] = 0;

    while (TRUE)
    {
		uni_char *word = MyUniStrTok(const_cast<uni_char*>(search_string.CStr()), seperator.CStr(), pos, done);

		if (done)
			break;

		if (!word || !*word)
			continue;

		OpAutoPtr<OpString> str(new OpString);
		
		if(!str.get())
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(str->Set(word));

		RETURN_IF_ERROR(list.Add(str.get()));

		str.release();
    }

	return OpStatus::OK;
}


OP_STATUS FileHandlerManagerUtilities::SplitString(OpVector<OpString>& list,
												   const OpStringC &candidate,
												   int sep,
												   BOOL keep_strings)
{
#ifdef QUICK
	return StringUtils::SplitString(list, candidate, sep, keep_strings);
#else
	return Split(list, candidate, sep, keep_strings);
#endif // QUICK
}

OpBitmap* FileHandlerManagerUtilities::GetBitmapFromPath(const OpStringC& path)
{
#ifdef QUICK
	return IconUtils::GetBitmap(path, -1, -1);
#else
#warning "GetBitmapFromPath not implemented"
	return 0;
#endif // QUICK
}

BOOL FileHandlerManagerUtilities::ValidateFolderHandler(const OpStringC & folder,
														const OpStringC & handler,
														BOOL& do_not_show_again)
{
#ifdef QUICK
	OpString s, message;
	
	TRAPD(rc,g_languageManager->GetStringL(Str::S_OPEN_FOLDER, s));
	message.Set(s);
	message.Append("\n");
	message.Append(folder);
	message.Append("\n\n");
	TRAP(rc,g_languageManager->GetStringL(Str::S_WITH_APPLICATION, s));
	message.Append(s);
	message.Append("\n");
	message.Append(handler);
	
	int result = SimpleDialog::ShowDialog(WINDOW_NAME_VALIDATE_FOLDER, 0, message.CStr(), UNI_L("Opera"), Dialog::TYPE_NO_YES, Dialog::IMAGE_INFO, &do_not_show_again);
	
	return result == Dialog::DIALOG_RESULT_YES;
#else
	return TRUE;
#endif // QUICK
}

BOOL FileHandlerManagerUtilities::AcceptExecutableHandler(BOOL found_security_issue,
														  const OpStringC& filename,
														  const OpStringC& handler,
														  BOOL& do_not_show_again)
{
#ifdef QUICK
	OP_STATUS status = OpStatus::OK;

	OpString s, message;
	if( found_security_issue )
	{
		TRAP(status,g_languageManager->GetStringL(Str::D_WARN_EXECUTABLE_FILE, message));
	}
	
	TRAP(status,g_languageManager->GetStringL(Str::S_OPEN_FILE, s));
	message.Append(s);
	message.Append("\n");
	message.Append(filename);
	message.Append("\n\n");
	TRAP(status,g_languageManager->GetStringL(Str::S_WITH_APPLICATION, s));
	message.Append(s);
	message.Append("\n");
	message.Append(handler);
	
	int result;
	if( found_security_issue )
	{
		result = SimpleDialog::ShowDialog(WINDOW_NAME_ACCEPT_HANDLER, 0, message.CStr(), UNI_L("Opera"), Dialog::TYPE_NO_YES, Dialog::IMAGE_WARNING);
	}
	else
	{
		result = SimpleDialog::ShowDialog(WINDOW_NAME_ACCEPT_HANDLER, 0, message.CStr(), UNI_L("Opera"), Dialog::TYPE_NO_YES, Dialog::IMAGE_INFO, &do_not_show_again);
	}
	return result == Dialog::DIALOG_RESULT_YES;
#else
	return TRUE;
#endif // QUICK
}

BOOL FileHandlerManagerUtilities::HandleProtocol(const OpStringC& protocol, const OpStringC& parameters)
{
#ifdef QUICK
	TrustedProtocolData data;
	int index = g_pcdoc->GetTrustedProtocolInfo(protocol, data);
	if (index >= 0)
	{
#ifdef UNIX
		OpString filename;
		if (OpStatus::IsError(filename.Set(data.filename)) || filename.IsEmpty())
			return FALSE;

		if (data.in_terminal)
			ApplicationLauncher::SetTerminalApplication(filename);

		if (!protocol.Compare("telnet"))
			return ApplicationLauncher::GenericTelnetAction( filename.CStr(), parameters.CStr() );
		else if (!protocol.Compare("news"))
			return ApplicationLauncher::GenericNewsAction( filename.CStr(), parameters.CStr() );
#endif // UNIX
	}
#else
#warning "HandleProtocol not implemented"
#endif // QUICK

	return FALSE;
}

BOOL FileHandlerManagerUtilities::LaunchHandler(const OpStringC& app, const OpStringC& params, BOOL in_terminal)
{
#ifdef UNIX
	return ApplicationLauncher::ExecuteProgram( app, params, in_terminal );
#else
#warning "LaunchHandler not implemented"
	return FALSE;
#endif // UNIX
}

void FileHandlerManagerUtilities::ExecuteFailed(const OpStringC& app,
												const OpStringC& protocol)
{
#ifdef QUICK
	OpString msg;
	TRAPD(rc,g_languageManager->GetStringL(Str::D_EXTERNAL_EXECUTION_FAILURE, msg));
	
	msg.Append("\n\n");
	msg.Append(app.CStr());
	
	rc = SimpleDialog::ShowDialog(WINDOW_NAME_EXECUTE_FAILED, 0, msg.CStr(), UNI_L("Opera"), Dialog::TYPE_YES_NO, Dialog::IMAGE_INFO );
	
	if( rc == Dialog::DIALOG_RESULT_YES )
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_PREFERENCES, protocol.CStr() ? NewPreferencesDialog::ProgramPage : NewPreferencesDialog::DownloadPage );
	}
#endif // QUICK
}

void FileHandlerManagerUtilities::RunExternalApplication(const OpStringC& handler, const OpStringC& path)
{
#ifdef UNIX
	ExternalApplication helperapp(handler.CStr());
	helperapp.run(path.CStr());
#else
#warning "RunExternalApplication not implemented"
#endif // UNIX
}

void FileHandlerManagerUtilities::NativeString(const OpStringC& input, OpString& output)
{
#ifdef UNIX // POSIX really
	PosixNativeUtil::NativeString native(input.CStr());
	output.Set(native.get());
#else

#warning "NativeString not implemented"
	output.Set(input.CStr());
#endif // UNIX
}
