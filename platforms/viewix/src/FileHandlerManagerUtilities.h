/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef FILEHANDLERMANAGERUTILITIES_H
#define FILEHANDLERMANAGERUTILITIES_H

class OpFile;
class HandlerElement;

namespace FileHandlerManagerUtilities
{

/**
   Modifies in_string so that if suffix appears in it everything from start of suffix
   on is removed from the string.

   @param the string and the suffix to be removed
   @return TRUE if the string was altered, FALSE otherwise
*/
    BOOL RemoveSuffix(OpString& in_string,
					  const OpStringC & suffix);


/**
   Modifies in_string so that if prefix is a prefix of in_string, it is removed.

   @param the string and the prefix to be removed

   @return TRUE if the string was altered, FALSE otherwise
*/
    BOOL RemovePrefix(OpString& in_string,
					  const OpStringC & prefix);


/**
   If in_string starts with prefix, this prefix will be replaced by new_prefix.

   @param the string and the old and new prefix

   @return TRUE if the string was altered, FALSE otherwise
*/
    BOOL ReplacePrefix(OpString& in_string,
					   const OpStringC & prefix,
					   const OpStringC & new_prefix);


/**
   Sets result to the BaseName of the given filename.
*/
    OP_STATUS StripPath(OpString & result,
						const OpStringC & filename);


/**
   Returns a string where everything up to the last slash in filename is preserved.

   @param filename from which path should be extracted

   @return the extracted path (directory name part of filename)
*/
    OP_STATUS GetPath(const OpStringC & filename, OpString & path);


/**
   Determines if glob is a literal glob, meaning that it does not contain any special
   characters.  For some reason we treat empty as a non-literal glob.

   @param the glob to be analyzed
   @return TRUE if glob is literal, FALSE otherwise
*/
    BOOL LiteralGlob(const OpStringC & glob);


/**
   Determines if glob is a simple glob, meaning that it does not contain any special
   characters after "*." and that it does indeed start with "*.".

   @param the glob to be analyzed
   @return TRUE if glob is simple, FALSE otherwise
*/
    BOOL SimpleGlob(const OpStringC & glob);


/**
   Checks whether dirName is the full path to a directory (uses S_ISDIR)
*/
    BOOL IsDirectory(const OpStringC & dirName);


/**
   Checks whether fileName is the full path to a regular file (uses S_ISREG)
*/
    BOOL IsFile(const OpStringC & fileName);

/**
   Makes an array of simple globs from the file extension. Ex: file.tar.gz
   results in *.tar.gz and *.gz.

   @param filename to be processed for making globs
   @param globs found for the filename

   @return OpStatus::OK if successful
*/
    OP_STATUS MakeGlobs(const OpStringC & filename,
						OpVector<OpString> & globs);

/**
   Tests whether a file exists on $PATH with the given mode. If it does the method
   will return true and target set to the full path of the file.

   @param candidate - the file name to be searched for
   @param mode      - the mode to test for if found
   @param target    - reference to string where full path will be put if candidate
   is found

   @returns TRUE if file was found with correct mode (else returns FALSE)

   [espen - Espen Sand]
*/
    BOOL ExpandPath( const OpStringC & candidate,
					 int mode,
					 OpString& target );


/**

 */
    BOOL LiteralWord(const OpString& str);


/**

 */
    BOOL OptionalWord(const OpString& str);


/**

 */
    BOOL Option(const OpString& str);


/**

 */
    BOOL OptionalPair(const OpString& str);


/**

 */
    BOOL Acceptable(const OpString& str);


/**

 */
    OP_STATUS ParseCommand(const OpString& command,
						   OpVector<OpString>& items);

/**
   Comment: The standard is not very clear on how this line should be parsed
   - given this lack I will here try to specify how I will parse it.

   W  - (Word) sequence of char not including spaces
   LW - (Literal Word) a word and not containing '%'
   OW - (Option Word) a Literal Word starting with a '-'
   O  - (Option) a word containing '%'
   OP - (Option Pair) an Option Word and an Option

   Yealding the following expression:

   [LW]* [OP | O]*

   That is:  a possible sequence of Literal Words followed by a possible sequence
   containing Optional Pairs and/or Options
*/
    OP_STATUS ParseCommand(OpVector<OpString>& tokens,
						   OpVector<OpString>& items);

/**

 */
    BOOL TakesUrls(const OpString& command);


/**

 */
    OP_STATUS MakePixelString(UINT32 icon_size, OpString & out_string);

/**
   Extracts the first word of a command string.

   @param the command string

   @return the command to start the application - extracted from the command string
*/
    OP_STATUS GetCleanCommand(const OpString& command, OpString & clean_command);

    OP_STATUS FileToLineVector(OpFile* file,
							   OpVector<OpString>& lines,
							   UINT32 max_size);

    OP_STATUS GetTerminalCommand(OpString & command);

	BOOL isKDERunning();

	BOOL isGnomeRunning();

	OP_STATUS SplitString( OpVector<OpString>& list, const OpStringC &candidate, int sep, BOOL keep_strings=FALSE );

	OpBitmap* GetBitmapFromPath(const OpStringC& path);

	BOOL ValidateFolderHandler(const OpStringC & folder,
							   const OpStringC & handler,
							   BOOL& do_not_show_again);

	BOOL AcceptExecutableHandler(BOOL found_security_issue,
								 const OpStringC& filename,
								 const OpStringC& handler,
								 BOOL& do_not_show_again);

	BOOL HandleProtocol(const OpStringC& protocol, const OpStringC& parameters);

	BOOL LaunchHandler(const OpStringC& app, const OpStringC& params, BOOL in_terminal = FALSE);

	void ExecuteFailed(const OpStringC& app,
					   const OpStringC& protocol);

	void RunExternalApplication(const OpStringC& handler, const OpStringC& path);

	void NativeString(const OpStringC& input, OpString& output);
};

#endif // FILEHANDLERMANAGERUTILITIES_H
