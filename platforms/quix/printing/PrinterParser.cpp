/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "core/pch.h"

#include "platforms/quix/printing/PrinterParser.h"

#include "modules/util/opfile/opfile.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/system/OpFolderLister.h"
#include "modules/regexp/include/regexp_api.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"

#include <dlfcn.h> // dlopen(), dlsym(), dlclose()
#include <stdlib.h>
#include <ctype.h>

#define DEBUG_PRINTCAP_PARSER

namespace PrinterParser
{

/**************************************************************************************
 *
 * Parse
 *
 * Parse printer files and add all (unique) printers to printerList
 *
 * @param printerList  - 
 * @param current_item - 
 *
 *
 **************************************************************************************/

void Parse( OpVector<PrinterListEntry>& printerList, int &current_item )
{
	OpString8 etcLpDefault;

	ParseCUPS( printerList, etcLpDefault );

    //if ( printerList.GetCount() == 0 )
	{
		// We only use other schemes when cups fails.
		ParseStdPrintcap( printerList , UNI_L("/etc/printcap"));
		ParseStdPrintcapMultiline( printerList, UNI_L("/etc/printcap"));

		// Check if this is a directory and call Parse
		if( FileHandlerManagerUtilities::IsDirectory(UNI_L("etc/lp/printers")))
		{
			ParseEtcLpPrinters( printerList , UNI_L("/etc/lp/printers"));

			OpFile def;
			if (OpStatus::IsError(def.Construct(UNI_L("/etc/lp/default")))) 
				return;
			if (OpStatus::IsError(def.Open(OPFILE_READ)))
				return;

			def.ReadLine( etcLpDefault); //, 1024 

			char * p = etcLpDefault.CStr();
			while( p && *p )
			{
				if ( !isprint(*p) || isspace(*p) )
					*p = 0;
				else
					p++;
			}
		}

		if (FileHandlerManagerUtilities::IsFile(UNI_L("etc/printers.conf")))
		{
			ParsePrintersConf( printerList, etcLpDefault, UNI_L("/etc/printers.conf"));
		}

    }

	// Determine the best printer to select
	OpString8 dollarPrinter;
    {
		const char * t;
		t = op_getenv( "PRINTER" );
		if ( !t || !*t )
			t = op_getenv( "LPDEST" );
		
		dollarPrinter.Set(t);
    }


	//--------------------------------------------------
	//  
	// Set current_item based on 'quality'
	// quality is set based on whether ps / lp is in comments
	// and whether name is dollarPrinter or etcLpDefault
	//
	//--------------------------------------------------
	
    int quality = 0;

	for ( uint i = 0; i < printerList.GetCount(); i++)
	{
		const PrinterListEntry *entry = printerList.Get(i);

		if( entry )
		{
			OpRegExp *re_ps1;
			OpRegExp *re_ps2;
			OpRegExp *re_lp1;
			OpRegExp *re_lp2;

			OpREMatchLoc match_loc;
			OpREFlags flags;
			flags.multi_line        = FALSE;
			flags.ignore_whitespace = FALSE;
			flags.case_insensitive  = TRUE;
			
			RETURN_VOID_IF_ERROR(OpRegExp::CreateRegExp(&re_ps1, UNI_L("[^a-z]ps[^a-z]"), &flags));
			RETURN_VOID_IF_ERROR(OpRegExp::CreateRegExp(&re_lp1, UNI_L("[^a-z]lp[^a-z]"), &flags));

			RETURN_VOID_IF_ERROR(OpRegExp::CreateRegExp(&re_ps2, UNI_L("[^a-z]ps$"), &flags));
			RETURN_VOID_IF_ERROR(OpRegExp::CreateRegExp(&re_lp2, UNI_L("[^a-z]lp$"), &flags));

			BOOL comment_match_lp = FALSE;
			BOOL comment_match_ps = FALSE;

			if (re_ps1)
				if (OpStatus::IsSuccess(re_ps1->Match(entry->comment.CStr(), &match_loc)) && match_loc.matchloc != REGEXP_NO_MATCH
					|| OpStatus::IsSuccess(re_ps1->Match(entry->comment.CStr(), &match_loc)) && match_loc.matchloc != REGEXP_NO_MATCH)
					{
						comment_match_ps = TRUE;
					}
			
			if (re_lp1)
				if (OpStatus::IsSuccess(re_lp1->Match(entry->comment.CStr(), &match_loc)) && match_loc.matchloc != REGEXP_NO_MATCH
					|| OpStatus::IsSuccess(re_lp1->Match(entry->comment.CStr(), &match_loc)) && match_loc.matchloc != REGEXP_NO_MATCH)
					{
						comment_match_lp = TRUE;
					}
			
			if ( quality < 4 && entry->name.Compare(dollarPrinter) == 0)
			{
				current_item = i;
				quality = 4;
				break;
			}
			else if ( quality < 3 && etcLpDefault.HasContent() &&
					  entry->name.Compare(etcLpDefault) == 0 )
			{
				current_item = i;
				quality = 3;
			}
			// if matches entry->comment
			else if ( quality < 2 && (entry->name.Compare("ps") == 0) || comment_match_ps )
			{
				current_item = i;
				quality = 2;
			}
			else if ( quality < 1 && ( entry->name.Compare("lp") == 0) || comment_match_lp )
			{
				current_item = i;
				quality = 1;
			}
			
			OP_DELETE(re_ps1);
			OP_DELETE(re_lp1);
			OP_DELETE(re_ps2);
			OP_DELETE(re_lp2);

		}
	}

#ifdef DEBUG_PRINTCAP_PARSER
	OpString8 defaultName;
	defaultName.Set(printerList.Get(current_item)->name.CStr());
	fprintf( stderr, " current_item is %d => %s\n", current_item, defaultName.CStr());
#endif
}

/****************************************************************
 *
 * AddPrinter
 *
 * @param printerList
 * @param name
 * @param host
 * @param comment
 *
 * If name is non-empty and there is not yet any entry in printerList
 * with this name, then a new entry is allocated and added to 
 * printerList
 *
 ****************************************************************/
void AddPrinter(OpVector<PrinterListEntry>& printerList,
				const OpStringC& name,
				const OpStringC& host,
				const OpStringC& comment)
{

	if( !name.HasContent() )
	{
		return;
	}

	for ( uint i = 0; i < printerList.GetCount(); i++ )
	{
		const PrinterListEntry *entry = printerList.Get(i);
		if( entry && entry->name.Compare(name) == 0)
		{
			return;
		}
	}

	PrinterListEntry *entry = OP_NEW(PrinterListEntry, ());
	entry->name.Set(name);
	entry->host.Set(host);
	entry->comment.Set(comment);

	entry->name.Strip();
	entry->host.Strip();
	entry->comment.Strip();

	if( !entry->host.HasContent() )
	{
		g_languageManager->GetString(Str::DI_QT_PRINT_LOCAL, entry->host);
	}

#ifdef DEBUG_PRINTCAP_PARSER	
	OpString8 name8, host8, comment8;
	name8.Set(entry->name.CStr());
	host8.Set(entry->host.CStr());
	comment8.Set(entry->comment.CStr());
	fprintf( stderr, " Adding printer(%s, %s, %s)\n", name8.CStr(), host8.CStr(), comment8.CStr());
#endif

	printerList.Add( entry );
}


/**************************************************************************************
 *
 * ParsePrintcapEntry
 *
 * @param printerList     - 
 * @param OpString &entry - 
 * Parses each entry in /etc/printcap and creates PrinterListEntries
 * with name, description (from the aliases) and host (if rm)
 *
 **************************************************************************************/

void ParsePrintcapEntry( OpVector<PrinterListEntry>& printerList, const OpStringC& entry )
{
#ifdef DEBUG_PRINTCAP_PARSER	
	OpString8 entry8;
	entry8.Set(entry.CStr());
	fprintf( stderr, " ParsePrintcapEntry(%s)\n", entry8.CStr());
#endif

	if( !entry.HasContent())
	{
		return;
	}

	int i = entry.Find(UNI_L(":")); // delimiter

	OpString printerName, printerComment, printerHost;

	if( i != KNotFound )
	{
		// 1. Set printerName -------------------
		int j = entry.Find("|");

		if (j != KNotFound) // there is alias
		{
			printerName.Set(entry.CStr(), j);
		}
		else // no alias
		{
			printerName.Set(entry.CStr(), i);
		}

		// 2. Set printerComment ---------------- is set to 'Aliases: alias1, alias2' if any aliases
		if( j != KNotFound ) 
		{
			g_languageManager->GetString(Str::DI_QT_PRINT_ALIASES, printerComment);
			printerComment.Append(" ");
			printerComment.Append(entry.SubString(j+1), i-j-1);
			RETURN_VOID_IF_ERROR(StringUtils::Replace(printerComment, UNI_L(" | "), UNI_L(", ")));
			RETURN_VOID_IF_ERROR(StringUtils::Replace(printerComment, UNI_L(" |"), UNI_L(", ")));
			RETURN_VOID_IF_ERROR(StringUtils::Replace(printerComment, UNI_L("| "), UNI_L(", ")));
			RETURN_VOID_IF_ERROR(StringUtils::Replace(printerComment, UNI_L("|"), UNI_L(",")));
		}


		// 3. find host -------------------------
		OpRegExp *re_all;
		OpREMatchLoc match_loc;
		OpREFlags flags;

		flags.multi_line        = FALSE;
		flags.ignore_whitespace = FALSE;
		flags.case_insensitive  = TRUE;
			
		RETURN_VOID_IF_ERROR(OpRegExp::CreateRegExp(&re_all, UNI_L(": *all *="), &flags));
		RETURN_VOID_IF_ERROR(re_all->Match(entry.CStr(), &match_loc));
		
		// look for lprng pseudo all printers entry
		if( match_loc.matchloc !=  REGEXP_NO_MATCH)
		{
			return;
		}

		// look for signs of this being a remote printer
		OpRegExp* re_rm;
		OpRegExp::CreateRegExp(&re_rm, UNI_L(": *rm *="), &flags);
		RETURN_VOID_IF_ERROR(re_rm->Match(entry.CStr(), &match_loc));

		if( match_loc.matchloc !=  REGEXP_NO_MATCH)
		{
			// set printerHost (... = ... printerHost : ...) from entry
			int i = entry.FindFirstOf(UNI_L("="), match_loc.matchloc + 1);
			int j = entry.FindFirstOf(UNI_L(":"), i);

			if (j != KNotFound)
			{
				printerHost.Set(entry.SubString(i + 1), j - i -1);
			}
			else
			{
				printerHost.Set(entry.SubString(i+1), entry.Length() - 1 - i);
			}
			printerHost.Strip();
		}
	}
	if( printerName.HasContent() )
	{
		AddPrinter( printerList, printerName, printerHost, printerComment);
	}
}


/**************************************************************************************
 *
 * ParseStdPrintcap
 *
 * @param printerList  - 
 *
 * Parses each entry in /etc/printcap and creates PrinterListEntries
 * with name, description (from the aliases) and host (if rm)
 *
 **************************************************************************************/
// static
void ParseStdPrintcap( OpVector<PrinterListEntry>& printerList, const OpStringC& filepath)
{
	OpFile printcap;

	if (OpStatus::IsError(printcap.Construct(filepath.CStr())))
		return;
	if (OpStatus::IsError(printcap.Open(OPFILE_READ)))
		return;

	OpString8 line; // 1024
    OpString printerDesc; // Holds the entry

	// while not EOF and lineLength of readLine(1024) > 0
	while (OpStatus::IsSuccess(printcap.ReadLine(line)) && !line.IsEmpty())
	{
		line.Strip();
		if ( line.Find("#") == 0 ) // comment
		{
			line.Empty();
			continue;
		}

		if ( line.Length() >= 2 && line.Find("\\") == line.Length() - 2 )  // Build the whole entry
		{
			INT32 lineLength = line.Length();
			printerDesc.Append(line.CStr(), lineLength - 2); 
		} 
		else  // Parse one entry ---------------------------------------
		{
			printerDesc.Append(line); 
			printerDesc.Strip(); // TODO: Should strip double whitespaces within string too? (StringUtils::AppendHightlight)

			if( printerDesc.Length() > 0 )
			{
				ParsePrintcapEntry( printerList, printerDesc );
				printerDesc.Empty(); // for next line
			}
		}
    }
}


/**************************************************************************************
 *
 * ParseStdPrintcapMultiline
 *
 * @param printerList  - 
 *
 **************************************************************************************/
// static
void ParseStdPrintcapMultiline( OpVector<PrinterListEntry>& printerList, const OpStringC& filepath)
{
	OpFile printcap;
	if (OpStatus::IsError(printcap.Construct(filepath.CStr())))
		return;
	if (OpStatus::IsError(printcap.Open(OPFILE_READ)))
		return;

	OpString8 line;
    OpString printerDesc;

	while (OpStatus::IsSuccess(printcap.ReadLine(line)) && !line.IsEmpty())
	{
		line.Strip();
		if( line.Length() == 0 )
		{
			continue;
		}

		if( line[0] == '#' )
		{
			continue;
		}

		OpString s;
		if( line[line.Length()-1] == '\\' ) // check
		{
			s.Set(line.CStr(), line.Length() - 2);
			if( s.Length() == 0 )
			{
				continue;
			}

			if(s[0] == ':' || s[0] == '|' || printerDesc.Length() == 0 )
			{
				printerDesc.Append(s);
			}
		}
		else if( printerDesc.Length() > 0 )
		{
			printerDesc.Append(line.Strip());
			ParsePrintcapEntry( printerList, printerDesc );
			printerDesc.Empty(); //Set(s);
		}
	}

	ParsePrintcapEntry( printerList, printerDesc );

}


// I do not include the "cups/cups.h" file yet.

typedef struct
{
  char  *name;  /* Name of option */
  char  *value; /* Value of option */
} cups_option_t;

typedef struct
{
  char  *name;             /* Printer or class name */
  char  *instance;         /* Local instance name or NULL */
  int   is_default;       /* Is this printer the default? */
  int   num_options;      /* Number of options */
  cups_option_t  *options; /* Options */
} cups_dest_t;



/**********************************************************************
 *
 * ParseCUPS
 *
 * @param printerList
 *
 * @return 
 *
 *********************************************************************/
// TODO: Use DLOpen Posix interface?
void ParseCUPS( OpVector<PrinterListEntry>& printerList, 
								 OpString8& etcLpDefault )
{
	void *cupsHandle = dlopen( "libcups.so.2", RTLD_NOW );

	if( !cupsHandle )
	{
		cupsHandle = dlopen( "libcups.so", RTLD_NOW );

		if( !cupsHandle )
		{
			printf("opera: could not open libcups.so.2 or libcups.so\n");
			return;
		}
	}

	typedef int (*cupsGetDests_t)(cups_dest_t **dest);
	typedef void (*cupsFreeDests_t)(int, cups_dest_t *);
	typedef const char* (*cupsGetOption_t)(const char*, int, cups_option_t *);

	cupsGetDests_t cupsGetDests   = (cupsGetDests_t)dlsym(cupsHandle, "cupsGetDests");
	cupsFreeDests_t cupsFreeDests = (cupsFreeDests_t)dlsym(cupsHandle, "cupsFreeDests");
	cupsGetOption_t cupsGetOption = (cupsGetOption_t)dlsym(cupsHandle, "cupsGetOption");

	if( !cupsGetDests || !cupsFreeDests )
	{
		printf("opera: could not load functions from libcups\n");
		dlclose(cupsHandle);
		return;
	}

	cups_dest_t *destination;

    int numPrinter = cupsGetDests( &destination );

	OpString host;
	OpString comment;

	cups_dest_t *dest;
	int i;
	const char *value = 0;
	
	for (i = numPrinter, dest = destination; i > 0; i--, dest++)
	{
		OpString name;
		name.Set(dest->name);
		if (dest->instance == NULL)
		{
			value = cupsGetOption("printer-info", dest->num_options, dest->options);
			if (value)
			{
				comment.Set(value);
			}
			value = 0;
			
			value = cupsGetOption("printer-location", dest->num_options, dest->options);
			if (value)
			{
				host.Set(value);
			}
			value = 0;
		}

		if (host.IsEmpty())
			g_languageManager->GetString(Str::DI_QT_PRINT_UNKNOWN_LOCATION, host);

		if (comment.IsEmpty())
			comment.Set(UNI_L("Cups"));

		AddPrinter( printerList, name, host, comment );

		if( dest->is_default && !etcLpDefault.HasContent() )
		{
			if( dest->instance )
			{
				etcLpDefault.Set(dest->instance);
			}
		}

	}

	cupsFreeDests(numPrinter,destination);
	dlclose(cupsHandle);
}


/**************************************************************************************
 *
 * ParseEtcLpPrinters
 *
 * @param printerList  - 
 *
 **************************************************************************************/
// solaris, not 2.6
// static
void ParseEtcLpPrinters( OpVector<PrinterListEntry>& printerList, const OpStringC& path)
{
	
	OpAutoPtr<OpFolderLister> lister(OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*"), path.CStr()));
	if (!lister.get())
		return;

	OpString tmp;
	
	while (lister->Next())
	{
		const uni_char* file_path = lister->GetFullPath();
		if (!file_path)
			continue;

		if( lister->IsFolder() )
		{
			const uni_char* candidate = lister->GetFileName();
			tmp.AppendFormat(UNI_L("/etc/lp/printers/%s/configuration"), candidate);

			OpFile config;
			RETURN_VOID_IF_ERROR(config.Construct(tmp.CStr()));
			RETURN_VOID_IF_ERROR(config.Open(OPFILE_READ));

			OpString8 line;
			OpString printerHost;
			bool canPrintPostscript = FALSE;

			while( config.ReadLine( line ) )
			{
				if (line.Find("Remote:") == 0)
				{
					printerHost.Set(line.SubString(7));
					printerHost.Strip();
				}
				else if (line.Find("Content types:") == 0)
				{
					OpString tmp;
					tmp.Set(line.SubString(14));
					tmp.Strip();

					OpVector<OpString> strings;
					RETURN_VOID_IF_ERROR(StringUtils::SplitString( strings, tmp, ','));
					for (UINT32 i = 0; i < strings.GetCount(); i++)
					{
						// For each part, skip alphanumeric chars. and test for Equal to postscript or any
						if (tmp.Find("postscript") != KNotFound || tmp.Find("any") != KNotFound)
						{
							canPrintPostscript = TRUE;
							break;
						}
					}
				}
				if ( canPrintPostscript )
				{
					OpString empty;
					AddPrinter(printerList, lister->GetFileName(), printerHost, empty); 
				}
			}
		}
    }
}

/**************************************************************************************
 *
 * ParsePrintersConf
 *
 * @param printerList  - 
 *
 * @return 
 *
 **************************************************************************************/
// ?? Builds printerComment string but doesnt use it for anything
// solaris 2.6
// static
void ParsePrintersConf( OpVector<PrinterListEntry>& printerList, 
										OpString8& defaultPrinter,
										const OpStringC& filePath)
{
	OpFile printer_conf;
	if (OpStatus::IsError(printer_conf.Construct(filePath.CStr())))
		return;
	if (OpStatus::IsError(printer_conf.Open(OPFILE_READ)))
		return;

	OpString printerDesc;

	OpString8 line;

	while( OpStatus::IsSuccess(printer_conf.ReadLine(line)) && !line.IsEmpty())
	{
		if (line.CStr()[0] == '#')
		{
			continue; // skip comment
		}

		if ( line.Length() >= 2 && line.Find("\\") == line.Length()-2) // Append to line
		{
			line[line.Length() - 2] = '\0';
			printerDesc.Append(line);
		} 
		else  // Parse entry
		{
			printerDesc.Append(line);
			printerDesc.Strip(); // TODO: Remove internal extra whitespace too

			int i = printerDesc.Find( ":" );
			
			OpString printerName, printerHost, printerComment;

			if ( i != KNotFound ) 
			{
				//--------------------------------------------------
				// printerName
				//--------------------------------------------------

				int j = printerDesc.Find( "|" );

				if ( j != KNotFound && j < i ) 
				{
					printerName.Set(printerDesc.CStr(), j);
				}
				else // No aliases
				{
					printerName.Set(printerDesc.CStr(), i);

					//--------------------------------------------------
					// printerComment
					//--------------------------------------------------
					
					// Set PrinterComment set to 'Aliases: alias1, alias2' if any aliases
					g_languageManager->GetString(Str::DI_QT_PRINT_ALIASES, printerComment);
					printerComment.Append(" ");
					printerComment.Append(printerDesc.SubString(j+1), i-j-1);
					// TODO: Error handling
					StringUtils::Replace(printerComment, UNI_L("|"), UNI_L(","));
				}

				if (printerName.Compare(UNI_L("_default")) == 0)
				{
					//--------------------------------------------------
					// defaultPrinter
					//--------------------------------------------------

					OpRegExp *re_use;
					OpREMatchLoc match_loc;
					
					OpREFlags flags;
					flags.multi_line        = FALSE;
					flags.ignore_whitespace = FALSE;
					flags.case_insensitive  = TRUE;
					
					// TODO: Error handling
					OpRegExp::CreateRegExp(&re_use, UNI_L(": *use *="), &flags);
					re_use->Match(printerDesc.CStr(), &match_loc);
					
					// look for lprng psuedo all printers entry
					if( match_loc.matchloc !=  REGEXP_NO_MATCH)
					{
						// set printer (... = ... printer : ...) from entry
						int i = printerDesc.FindFirstOf(UNI_L("="), match_loc.matchloc);
						int j = printerDesc.FindFirstOf(UNI_L(":,"), i);
						if (j != KNotFound)
							defaultPrinter.Set(printerDesc.SubString(i+1), j-i-1); 
					}
				} 
				else if ( printerName.Compare(UNI_L("_all")) == 0 ) // skip it.. any other cases we want to skip?
				{
					printerName.Empty();
					printerDesc.Empty();
					continue;
				}

				//--------------------------------------------------
				// printerHost
				//--------------------------------------------------

				// look for signs of this being a remote printer
				OpRegExp *re_bsd;
				OpREMatchLoc match_loc;
				OpREFlags flags;

 				flags.multi_line        = FALSE;
 				flags.case_insensitive  = TRUE;
 				flags.ignore_whitespace = FALSE;
				
				// Handle errors
				OpRegExp::CreateRegExp(&re_bsd, UNI_L(": *bsdaddr *="), &flags);
				re_bsd->Match(printerDesc.CStr(), &match_loc);
		
				// look for lprng psuedo all printers entry
				if( match_loc.matchloc !=  REGEXP_NO_MATCH)
				{
					// i == match_loc.matchloc
					int i = printerDesc.FindFirstOf(UNI_L("="), match_loc.matchloc);
					int j = printerDesc.FindFirstOf(UNI_L(":,"), i);
					if (j != KNotFound)
						printerHost.Set(printerDesc.SubString( i+1 ), j-i-1 );
					else
						printerHost.Set(printerDesc.SubString( i+1 ));

					printerHost.Strip();


					//--------------------------------------------------
					// printerComment
					//--------------------------------------------------
					// TODO
					int l = printerDesc.FindFirstOf(UNI_L(","));
					// maybe stick the remote printer name into the comment
					if ( l != KNotFound)
					{
						int k = printerDesc.FindFirstOf(UNI_L(":,"),j);
						printerComment.Set(UNI_L("Remote name: "));
						printerComment.Append(printerDesc.SubString(j), k - j);
						printerComment.Strip();
					}
				}
			}
			if ( printerComment.Compare(":") == 0)
			{
				printerComment.Empty(); // for cups
			}
			if ( printerName.HasContent() )
			{
				OpString empty;
				AddPrinter( printerList, printerName, printerHost, empty);
			}

			// chop away the line, for processing the next one
			printerDesc.Empty();
		}

	}
}

} // End namespace PrinterParser
