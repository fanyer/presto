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

#include "adjunct/quick/dialogs/PreferencesDialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/dialogs/TrustedProtocolDialog.h"
#include "adjunct/desktop_util/prefs/DesktopPrefsTypes.h"
#include "adjunct/desktop_util/string/stringutils.h"

#include "modules/inputmanager/inputmanager.h"

#include "modules/dochand/docman.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/doc/frm_doc.h"
#include "modules/formats/uri_escape.h"
#include "modules/util/opstrlst.h"
#include "modules/url/url_man.h"

#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#include "platforms/unix/base/common/execmonitor.h"
#include "platforms/unix/base/common/externalapplication.h"
#include "platforms/posix/posix_native_util.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"

#include "applicationlauncher.h"

#include <errno.h>


static void Replace( OpString &target, UINT32 at, UINT32 len, const OpString &replacement )
{
	if( !replacement.IsEmpty() )
	{
		int length = target.Length();
		if( (int)at > length )
		{
			target.Append( replacement );
		}
		else
		{
			OpString tmp1,tmp2;
			tmp1.Set( target.SubString(0), at);
			tmp2.Set( target.SubString(at+len));
			tmp1.Append( replacement );
			tmp1.Append( tmp2 );
			target.Set( tmp1 );
		}
	}
}


static int FindRev( const OpString &target, int at, int ch )
{
	if( at < 0 )
	{
		at = 0;
	}

	int length = target.Length();
	if( at > length )
	{
		at = length;
	}

	for( int i=at; i>=0; i-- )
	{
		if( target.CStr()[i] == (uni_char)ch )
		{
			return i;
		}
	}

	return KNotFound;
}


static int RemoveBracketSegment( OpString &target, UINT32 at, UINT32 content_length )
{
	const uni_char *left = UNI_L("[");
	const uni_char *right = UNI_L("]");

	int p1 = FindRev( target, at, left[0] );
	int p2 = FindRev( target, at, right[0] );
	int p3 = target.FindFirstOf( right, at+content_length );
	int p4 = target.FindFirstOf( left, at+content_length );
	if( p1 == KNotFound || p3 == KNotFound || p1 >= p3 )
	{
		return -1;
	}
	if( p2 != KNotFound && p2 > p1 && p2 != p3 )
	{
		return -1;
	}
	if( p4 != KNotFound && p4 < p3 && p4 != p1 )
	{
		return -1;
	}

	if( content_length == 0 )
	{
		int length = p3 - p1 + 1;
		target.Delete( p1, length );
		return p1;
	}
	else
	{
		target.Delete( p3, 1 );
		target.Delete( p1, 1 );
		return p3;
	}
}


void SimplifyWhiteSpace( OpString &target )
{
	int length = target.Length();
	for( int i=0; i<length; i++ )
	{
		if( uni_isspace(target[i]) )
		{
			target[i] = ' ';
		}
	}
}


void ReplaceChar( OpString &target, int remove, int replace )
{
	int length = target.Length();
	for( int i=0; i<length; i++ )
	{
		if( target[i] == remove )
		{
			target[i] = replace;
		}
	}
}


static void TruncateAfterSpaceDash( OpString& candidate )
{
	BOOL last_was_space = TRUE; // catch candidate that starts with a '-'

	for(uni_char* p = candidate.CStr(); p; p++ )
	{
		if( *p == '-' && last_was_space )
		{
			INT32 length = p - candidate.CStr();
			if( length < 1 )
			{
				candidate.Empty();
			}
			else
			{
				OpString tmp;
				tmp.Set( candidate.CStr(), length );
				candidate.Set(tmp);
			}
			return;
		}
		last_was_space = uni_isspace(*p) ? TRUE : FALSE;
	}
}


static void EscapeString( OpString &target )
{
	OpString8 tmp;
	tmp.SetUTF8FromUTF16( target.CStr() );

	target.SetFromUTF8( tmp.CStr() );

	INT32 len = target.Length();

	OpString dest;
	dest.Reserve(len*3);

	for( INT32 i=0; i<len; i++ )
	{
		int val = target.CStr()[i];
		if( val > 0x20 && val <= 0x7F )
		{
			char str[2] = { val, 0 };
			dest.Append( str );
		}
		else
		{
			char str[4];
			sprintf(str, "%%%02X",val);
			dest.Append(str);
		}
	}

	target.Set(dest);
}







void BuildMissingApplicationMessage( Str::LocaleString application_id, OpString &message )
{
	OpString tmp;
	TRAPD(err,g_languageManager->GetStringL(application_id, message));
	message.Append("\n\n");
	TRAP(err,g_languageManager->GetStringL(Str::D_EXTERNAL_CLIENT_MISSING, tmp));
	message.Append( tmp.CStr() );
	message.Append("\n");
}


// static
BOOL ApplicationLauncher::ExecuteProgram( const OpStringC &application, const OpStringC &arguments, BOOL run_in_terminal )
{
	int fd = -1;

	if( ExecMonitor::get() )
	{
		fd = ExecMonitor::get()->start( 0, -1 );
	}

	OpString tmp;
	tmp.Append(application.CStr());

	if( run_in_terminal )
	{
		SetTerminalApplication( tmp );
	}

	tmp.Append(" ");
	tmp.Append(arguments.CStr());

	OpAutoVector<OpString> list;
	StringUtils::SplitString( list, tmp, ' ', TRUE );

	DisplayDialogOnError(ExternalApplication::run( list, fd ));

	return TRUE;
}

// static
BOOL ApplicationLauncher::ExecuteProgram( const OpStringC &application, int argc, const char* const* argv, BOOL run_in_terminal )
{
	// Create complete uni_char argv
	uni_char **uni_argv = OP_NEWA(uni_char *, argc+2);
	if (!uni_argv)
		return FALSE; // OOM
	op_memset(uni_argv, 0, sizeof(uni_char)*(argc+2));

	for (int i = 0; i < argc + 1; i++)
	{
		if (i==0)
		{
			uni_argv[i] = uni_stripdup(application.CStr());
		}
		else
		{
			OpString tmp;
			tmp.SetFromUTF8(argv[i-1]);
			uni_argv[i] = uni_stripdup(tmp.CStr());
		}

		// OOM
		if (!uni_argv[i])
		{
			for (unsigned int j=i-1; j>=0; j--)
				OP_DELETEA(uni_argv[j]);
			OP_DELETEA(uni_argv);
			return FALSE; 
		}
	}
	uni_argv[argc+1]=0;
	
	int fd = -1;
	
	if( ExecMonitor::get() )
	{
		fd = ExecMonitor::get()->start( 0, -1 );
	}

	OpString tmp;
	tmp.Append(application.CStr());

	if( run_in_terminal )
	{
		SetTerminalApplication( tmp );
	}

	DisplayDialogOnError(ExternalApplication::run( uni_argv, fd ));

	if (uni_argv)
	{
		for (int i=0; i < argc+1; i++)
			OP_DELETEA(uni_argv[i]);
		OP_DELETEA(uni_argv);
	}
	return TRUE;
}

// static
BOOL ApplicationLauncher::ViewSource( Window* window, FramesDocument* doc )
{
	if( !doc )
	{
		return FALSE;
	}

	URL url = doc->GetURL();
	url.PrepareForViewing(TRUE);

	OpString filename;
	TRAPD(err, url.GetAttributeL(URL::KFilePathName_L, filename, TRUE));
	if (err!=OpStatus::OK || filename.IsEmpty())
		return FALSE;

	OpString8 encoding;
	if( doc->GetWindow() )
		encoding.Set(doc->GetWindow()->GetForceEncoding());

	return ViewSource(filename, encoding);
}


// static
BOOL ApplicationLauncher::ViewSource(const OpString& filename, const OpString8& encoding)
{
	if( !filename.CStr() || !*filename.CStr() )
	{
		return FALSE;
	}

	OpString appName;
	TrustedProtocolList::GetSourceViewer(appName);
	const NewPreferencesDialog::AdvancedPageId page = NewPreferencesDialog::ProgramPage;

	if( appName.IsEmpty() )
	{
		OpString msg;
		BuildMissingApplicationMessage(Str::DI_IDM_SRC_VIEWER_BOX, msg);

		int rc = SimpleDialog::ShowDialog(WINDOW_NAME_RUN_EXTERNAL_ERROR,0, msg.CStr(), UNI_L("Opera"), Dialog::TYPE_YES_NO, Dialog::IMAGE_INFO );
		if( rc == Dialog::DIALOG_RESULT_YES )
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_PREFERENCES, page);
		}
		return FALSE;
	}

	if(g_pcapp->GetIntegerPref(PrefsCollectionApp::RunSourceViewerInTerminal))
	{
		SetTerminalApplication( appName );
	}

	int fd = -1;
	if( ExecMonitor::get() )
	{
		OpString program_type;
		TRAPD(rc,g_languageManager->GetStringL(Str::DI_IDM_SRC_VIEWER_BOX, program_type));
		fd = ExecMonitor::get()->start(program_type.CStr(), page);
	}

	ExternalApplication app( appName.CStr() );

	DisplayDialogOnError(app.run(filename.CStr(), fd, encoding.CStr()));

	return TRUE;
}

// static
BOOL ApplicationLauncher::GenericMailAction( const uni_char* to,
											 const uni_char* cc,
											 const uni_char* bcc,
											 const uni_char* subject,
											 const uni_char* message,
											 const uni_char* rawurl )
{
	// We reach this function if we are going to use a custom application or system application
	int mail_handler = g_pcui->GetIntegerPref(PrefsCollectionUI::MailHandler);
	if (mail_handler != MAILHANDLER_SYSTEM && mail_handler != MAILHANDLER_APPLICATION)
		return FALSE;

	OpString client;
	if (mail_handler == MAILHANDLER_SYSTEM)
	{
		OpString protocol;
		RETURN_VALUE_IF_ERROR(protocol.Set("mailto"), FALSE);
		OP_STATUS rc = g_op_system_info->GetProtocolHandler(OpString(), protocol, client);
		if (OpStatus::IsError(rc) && rc != OpStatus::ERR_NOT_SUPPORTED)
			return FALSE;
		client.Strip();
	}

	if (client.IsEmpty())
	{
		RETURN_VALUE_IF_ERROR(client.Set(g_pcapp->GetStringPref(PrefsCollectionApp::ExternalMailClient)), FALSE);
		client.Strip();
	}

	const NewPreferencesDialog::AdvancedPageId page = NewPreferencesDialog::ProgramPage;

	if (client.IsEmpty() || !client.Compare("\"\"")) // Older versions of Opera wrote an empty '""' string from prefs dialog
	{
		OpString msg;
		BuildMissingApplicationMessage(Str::D_EMAIL_CLIENT, msg);

		int rc = SimpleDialog::ShowDialog(WINDOW_NAME_RUN_EXTERNAL_ERROR,0, msg.CStr(), UNI_L("Opera"), Dialog::TYPE_YES_NO, Dialog::IMAGE_INFO );
		if( rc == Dialog::DIALOG_RESULT_YES )
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_PREFERENCES, page);
		}
		return FALSE;
	}

	if (mail_handler == MAILHANDLER_APPLICATION)
	{
		if (g_pcapp->GetIntegerPref(PrefsCollectionApp::RunEmailInTerminal))
		{
			RETURN_VALUE_IF_ERROR(SetTerminalApplication(client), FALSE);
		}
	}

	OpAutoVector<OpString> list;
	if (!ParseEmail( list, client.CStr(), to, cc, bcc, subject, message, rawurl ) )
	{
		return FALSE;
	}

	int fd = -1;
	if (ExecMonitor::get())
	{
		OpString program_type;
		TRAPD(rc,g_languageManager->GetStringL(Str::D_EMAIL_CLIENT, program_type));
		fd = ExecMonitor::get()->start(program_type.CStr(), page);
	}

	DisplayDialogOnError(ExternalApplication::run( list, fd ));
	return TRUE;
}


BOOL ApplicationLauncher::GenericNewsAction(const uni_char* application, const uni_char* urlstring)
{
	OpString app_name;
	app_name.Set(application);


	OpAutoVector<OpString> list;
	if( !ParseNewsAddress( list, app_name.CStr(), urlstring ) )
	{
		return FALSE;
	}


	int fd = -1;
	if( ExecMonitor::get() )
	{
		OpString program_type;
		TRAPD(rc,g_languageManager->GetStringL(Str::D_PREFERENCES_NEWS_PROGRAM, program_type));

		NewPreferencesDialog::AdvancedPageId page = NewPreferencesDialog::ProgramPage;
		fd = ExecMonitor::get()->start(program_type.CStr(), page);
	}

	DisplayDialogOnError(ExternalApplication::run( list, fd ));
	return TRUE;
}


// static 
BOOL ApplicationLauncher::GenericTelnetAction(const uni_char* application, const uni_char* urlstring)
{
	OpString app_name;
	app_name.Set(application);

	URL	url = urlManager->GetURL(urlstring);

	OpString hostname, port, user_name, password, fullurl;
	hostname.Set(url.GetAttribute(URL::KHostName));
	if( url.GetServerPort() > 0 )
		port.AppendFormat(UNI_L("%d"), url.GetServerPort() );	
	user_name.Set(url.GetAttribute(URL::KUserName));	
	password.Set(url.GetAttribute(URL::KPassWord));


	// We used PurifyHostname() from doc_man.cpp before merlin.
	int i;
	for(i=0; i<hostname.Length(); i++ )
	{
		if( hostname.CStr()[i] != '-' && hostname.CStr()[i] != '/' )
		{
			break;
		}
	}
	if( i > 0 )
	{
		hostname.Delete(0,i);
	}


	OpAutoVector<OpString> list;
	if(!ParseTelnetAddress( list, app_name.CStr(), hostname.CStr(), port.CStr(), user_name.CStr(), password.CStr(), fullurl.CStr()))
	{
		return FALSE;
	}

	int fd = -1;
	if( ExecMonitor::get() )
	{
		OpString program_type;
		TRAPD(rc,g_languageManager->GetStringL(Str::DI_IDM_TELNET_BOX, program_type));

		NewPreferencesDialog::AdvancedPageId page = NewPreferencesDialog::ProgramPage;
		fd = ExecMonitor::get()->start(program_type.CStr(),page);
	}

	DisplayDialogOnError(ExternalApplication::run( list, fd ));
	return TRUE;
}


// static
BOOL ApplicationLauncher::GenericTrustedAction( Window *, const URL& url )
{
	OpString name;
	name.Set( url.UniName(PASSWORD_SHOW));
	if( url.UniRelName() )
	{
		name.Append("#");
		name.Append(url.UniRelName());
	}

	int pos = name.Find("://");
	if( pos == -1 )
	{
		return FALSE;
	}

	OpString protocol, address;
	protocol.Set( name.CStr(), pos );
	address.Set( name.SubString( pos+3) );
	if( protocol.Length() == 0 || address.Length() == 0 )
	{
		return FALSE;
	}

	TrustedProtocolData data;
	if (g_pcdoc->GetTrustedProtocolInfo(protocol, data) == -1)
	{
		return FALSE;
	}

	OpString filename;
	if (OpStatus::IsError(filename.Set(data.filename)) || filename.IsEmpty())
	{
		return FALSE;
	}

	if (data.in_terminal)
	{
		SetTerminalApplication( filename );
	}

	OpAutoVector<OpString> list;
	if (!ParseTrustedApplication( list, filename.CStr(), name.CStr(), address.CStr() ) )
	{
		return FALSE;
	}

	int fd = -1;
	if( ExecMonitor::get() )
	{
		fd = ExecMonitor::get()->start(0,-1);
	}

	DisplayDialogOnError(ExternalApplication::run( list, fd ));

	return TRUE;
}



// static
OP_STATUS ApplicationLauncher::SetTerminalApplication( OpString &application )
{
	OpString tmp;
	RETURN_IF_ERROR(FileHandlerManagerUtilities::GetTerminalCommand(tmp));
	RETURN_IF_ERROR(tmp.Append(application));
	return application.Set( tmp );
}


// static
void ApplicationLauncher::ReplaceEscapedCharacters( OpString &target, UINT32 start, BOOL special_character_conversion )
{
	OpString tmp;
	UINT32 length = target.Length();

	for( UINT32 i=start; i<length; i++ )
	{
		uni_char ch = target[i];
		if( ch == '+' )
		{
			if( special_character_conversion )
				ch = ' ';
		}
		else if( ch == '&' )
		{
			if( special_character_conversion )
				ch = '\n';
		}
		else if( ch == '%' && i+2 < length )
		{
			ch = static_cast<unsigned char>(UriUnescape::Decode(target.CStr()[i+1], target.CStr()[i+2]));
			if( ch )
			{
				i+=2;
			}
			else
			{
				ch = '%';
			}
		}

		// Partial fix for bug #272872
		if( ch == '\\' || ch == '\"' )
		{
			uni_char buf[2]={0,0};
			buf[0]='\\';
			tmp.Append(buf);
		}

		uni_char buf[2]={0,0};
		buf[0]=ch;
		tmp.Append(buf);
	}

	target.Set( tmp );
}


// static
void ApplicationLauncher::GetCommandLineOptions(const OpString& protocol, OpString& text)
{
	if( !protocol.IsEmpty() )
	{
		if( protocol.Compare("mailto") == 0)
		{
			TRAPD(err,g_languageManager->GetStringL(Str::S_EMAIL_CLIENT_SETUP_HELP_TEXT, text));
		}
		else if( protocol.Compare("telnet") == 0)
		{
			TRAPD(err,g_languageManager->GetStringL(Str::S_TELNET_CLIENT_SETUP_HELP_TEXT, text));
		}
	}
}


//
// Supported flags
//
// %t (to) - Replaced by the receiver
// %c (cc) - Replaced by the cc receiver
// %b (bcc) - Replaced with the bcc receiver
// %s (subject) - Replaced by the subject text
// %m (body) - Replaced by the body text
// %r (raw) - Replaced by the raw url. The result will always start
//            with "mailto:"
// %w (raw) - As %r but without the "mailto:" prefix.
//
//
// The specifiers can be placed within a set of brackets. If the
// specifiers are not replaced by real data, then the text within the
// brackets (and the brackets themselves) are removed to avoid
// confusion when starting the mailer.
//
// Example 1:
// Text stored in opera6.ini
// "kmail [%t] [-cc %c] [-subject %s] [-body %b]"
//
// url:
// <"mailto:abc@xyz.nn?body=hello">
// becomes:
// kmail abc@xyz.nn -body hello
//
// url:
// <"mailto:abc@xyz.nn?body=hello?subject=how are you">
// becomes:
// kmail abc@xyz.nn -body hello -subject how are you
//
// Example 2:
// Text stored in opera6.ini
// "evolution [%r]"
//
// url:
// <"mailto:abc@xyz.nn?body=hello?subject=how are you">
// becomes:
// evolution mailto:abc@xyz.nn?body=hello?subject=how are you
//
// static
BOOL ApplicationLauncher::ParseEmail( OpVector<OpString>& list,
									  const uni_char* application,
									  const uni_char* to,
									  const uni_char* cc,
									  const uni_char* bcc,
									  const uni_char* subject,
									  const uni_char* message,
									  const uni_char* rawurl )
{
	int separator = 3; // Replace with '|' to make it readable

	OpString tmp, tmp1;
	tmp1.Set(application);
	tmp.Set( tmp1.Strip(TRUE,TRUE) );
	ReplaceChar( tmp, ' ', separator );

	int pos = 0;
	BOOL string_replaced = FALSE;
	BOOL warn_user_about_mail = FALSE;

	while(1)
	{
		pos = tmp.FindFirstOf( UNI_L("%"), pos );
		if( pos == KNotFound || pos + 1 > tmp.Length() )
		{
			break;
		}

		OpString candidate;
		int ch = uni_tolower( tmp[pos+1] );
		if( ch == '%' ) // added escaping (%% -> %) in peregrine
		{
			candidate.Set("%");
		}
		else if( ch == 't' )
		{
			candidate.Set( to );
			ReplaceEscapedCharacters( candidate, 0, TRUE );
		}
		else if( ch == 'c' )
		{
			candidate.Set( cc );
			ReplaceEscapedCharacters( candidate, 0, TRUE );
		}
		else if( ch == 'b' ) // was 'a' before peregrine
		{
			candidate.Set( bcc );
			ReplaceEscapedCharacters( candidate, 0, TRUE );
		}
		else if( ch == 's' )
		{
			candidate.Set( subject );
			ReplaceEscapedCharacters( candidate, 0, TRUE );
		}
		else if( ch == 'm' ) // was 'b' before peregrine
		{
			candidate.Set( message );
			ReplaceEscapedCharacters( candidate, 0, FALSE );
		}
		else if( ch == 'r' )
		{
			if( rawurl )
			{
				// If link (can be a page-url) does not start with mailto we will add one and subject if possible
				if( uni_strncmp(rawurl, "mailto:", 7) )
				{
					candidate.Set("mailto:");
					if( subject && *subject )
					{
						candidate.Append("subject=");
						candidate.Append(subject);
					}
					candidate.Append("&body=<URL: ");
					candidate.Append(rawurl);
					candidate.Append(">");
				}
				else
				{
					candidate.Set( rawurl );
				}
			}
		}
		else if( ch == 'w' )
		{
			if( rawurl )
			{
				// Remove mailto: from raw URL if possible
				int offset = 0;
				if (uni_strncmp(rawurl, "mailto:", 7) == 0)
					offset = 7;

				// If link (can be a page-url) does not start with mailto we will add subject if possible
				if( offset == 0 )
				{
					if( subject && *subject )
					{
						OpString escaped_subject;
						escaped_subject.Set(subject);
						EscapeString(escaped_subject);

						if( escaped_subject.Length() > 0 )
						{
							candidate.Append("subject=");
							candidate.Append(escaped_subject);
						}
					}
					candidate.Append("&body=<URL: ");
					candidate.Append(rawurl+offset);
					candidate.Append(">");
				}
				else
				{
					candidate.Set( rawurl+offset );
				}
			}
		}
		else
		{
			pos = pos + 1;
			string_replaced = TRUE;
			continue;
		}

		candidate.Strip();
		int dash_pos = candidate.Find("-");
		if( dash_pos == 0 || (dash_pos > 0 && uni_isspace(candidate.CStr()[dash_pos-1])) )
		{
			warn_user_about_mail = TRUE;
		}


		Replace( tmp, pos, 2, candidate );
		int length = candidate.Length();
		int p = RemoveBracketSegment( tmp, pos, length );
		pos = p == -1 ? pos + 1 : p;
		string_replaced = TRUE;
	}

	// What is commented out below is what we had before bug #272872 was fixed
	// We also assume no argument means the same as %r [espen 2007-01-01]
	if( !string_replaced && rawurl/*to*/ )
	{
		OpString candidate;
		candidate.Set( rawurl/*to*/ );
		//ReplaceEscapedCharacters( candidate, 0, TRUE );
		candidate.Strip();

		int pos = candidate.Find("-");
		if( pos == 0 || (pos > 0 && uni_isspace(candidate.CStr()[pos-1])) )
		{
			warn_user_about_mail = TRUE;
		}

		// Fallback
		uni_char sep[2]={separator,0};
		tmp.Append(sep);
		tmp.Append(candidate);
	}

	if( warn_user_about_mail )
	{
		OpString message;
		TRAPD(err, g_languageManager->GetStringL(Str::S_MAIL_ADDRESS_WITH_COMMAND_CHAR, message));
		message.Append("\n\n");

		OpString command;
		command.Set(tmp);
		ReplaceChar( command, separator, ' ' );
		message.Append( command );

		int ret_code = SimpleDialog::ShowDialog(WINDOW_NAME_MAIL_WARN,0, message.CStr(), UNI_L("Opera"), Dialog::TYPE_YES_NO, Dialog::IMAGE_WARNING );
		if( ret_code == Dialog::DIALOG_RESULT_NO )
		{
			return FALSE;
		}
	}

	StringUtils::SplitString( list, tmp, separator, FALSE );
	return TRUE;

}


//
// Supported flags
//
// %s (server) - Replaced by the server
// %g (group)  - Replaced by the group
// %r (raw)    - The raw string. The result will always start
//               with "news://" and the generic server name
//               ("unknown.newsserver") will be removed if
//               present.
// %w (raw)    - As %r but without the "news://" prefix.
//
// The specifiers can be placed within a set of brackets. If the
// specifiers are not replaced by real data, then the text within the
// brackets (and the brackets themselves) are removed to avoid
// confusion when starting the telnet client
//
// Example:
// Text stored in opera6.ini
// "knode [%r]"
//
// static
BOOL ApplicationLauncher::ParseNewsAddress( OpVector<OpString>& list,
											const uni_char *application,
											const uni_char *url )
{
	if( !application || !url )
	{
		return FALSE;
	}

	OpString server, group, full;
	OpString tmp1, tmp2;

	tmp1.Set( url );
	int pos = tmp1.Find("news://");
	if( pos != KNotFound )
	{
		tmp2.Set( tmp1.SubString(pos+7) );
		tmp1.Set(tmp2);
	}
	full.Set(tmp1);

	pos = tmp1.Find("/");
	if( pos != KNotFound )
	{
		server.Set( tmp1.SubString(0), pos );
		group.Set( tmp1.SubString(pos+1) );
	}
	else
	{
		group.Set( tmp1 );
	}

	tmp2.Set(application);
	tmp1.Set( tmp2.Strip(TRUE,TRUE) );

	pos = 0;
	while(1)
	{
		pos = tmp1.FindFirstOf( UNI_L("%"), pos );
		if( pos == KNotFound || pos + 1 > tmp1.Length() )
		{
			break;
		}

		OpString candidate;
		int ch = uni_tolower( tmp1[pos+1] );
		if( ch == 's' )
		{
			candidate.Set( server );
			ReplaceEscapedCharacters( candidate, 0, TRUE );
		}
		else if( ch == 'g' )
		{
			candidate.Set( group );
			ReplaceEscapedCharacters( candidate, 0, TRUE );
		}
		else if( ch == 'r' )
		{
			candidate.Set("news://");
			if( server.Length() == 0 )
			{
				candidate.Append("/");
			}
			candidate.Append( full );
			ReplaceEscapedCharacters( candidate, 0, TRUE );
		}
		else if( ch == 'w' )
		{
			candidate.Set( full );
			ReplaceEscapedCharacters( candidate, 0, TRUE );
		}

		TruncateAfterSpaceDash( candidate ); // Vulnerability like bug #139916

		Replace( tmp1, pos, 2, candidate );
		int length = candidate.Length();
		int p = RemoveBracketSegment( tmp1, pos, length );
		pos = p == -1 ? pos + 1 : p;
	}

	StringUtils::SplitString( list, tmp1, ' ', FALSE );
	return TRUE;
}


//
// Supported flags
//
// %a (address) - Replaced by the address (server)
// %p (port) - Replaced by the port
// %u (user name) - Replaced by the user name
// %w (password) - Replaced by the password
//
// The specifiers can be placed within a set of brackets. If the
// specifiers are not replaced by real data, then the text within the
// brackets (and the brackets themselves) are removed to avoid
// confusion when starting the telnet client
//
// Example:
// Text stored in opera6.ini
// "telnet [-l %u] [%a] [%p]"
//
// static
BOOL ApplicationLauncher::ParseTelnetAddress( OpVector<OpString>& list,
											  const uni_char *application,
											  const uni_char *address,
											  const uni_char *port,
											  const uni_char *user,
											  const uni_char *password,
											  const uni_char *rawurl )
{
	if( !application || !address )
	{
		return FALSE;
	}

	OpString tmp1, tmp2;
	tmp2.Set(application);
	tmp1.Set( tmp2.Strip(TRUE,TRUE) );

	BOOL address_replaced = FALSE;
	BOOL port_replaced = FALSE;
	BOOL raw_replaced = FALSE;

	int pos = 0;
	while(1)
	{
		pos = tmp1.FindFirstOf( UNI_L("%"), pos );
		if( pos == KNotFound || pos + 1 > tmp1.Length() )
		{
			break;
		}

		OpString candidate;
		int ch = uni_tolower( tmp1[pos+1] );
		if( ch == 'a' )
		{
			candidate.Set(address);
			address_replaced = TRUE;
		}
		else if( ch == 'p' )
		{
			candidate.Set(port);
			port_replaced = TRUE;
		}
		else if( ch == 'u' )
		{
			candidate.Set(user);
		}
		else if( ch == 'w' )
		{
			candidate.Set(password);
		}
		else if( ch == 'r' )
		{
			candidate.Set(rawurl);
			raw_replaced = TRUE;
		}

		Replace( tmp1, pos, 2, candidate );
		int length = candidate.Length();
		int p = RemoveBracketSegment( tmp1, pos, length );
		pos = p == -1 ? pos + 1 : p;
	}

	StringUtils::SplitString( list, tmp1, ' ', FALSE );

	if( !raw_replaced )
	{
		if( !address_replaced )
		{
			if( address )
				StringUtils::AppendToList( list, address );
		}
		if( !port_replaced && port )
		{
			if( port )
				StringUtils::AppendToList( list, port );
		}
	}

	return TRUE;
}


//
// Replace %u or %v
// %u => Replace with full address
// %v => Replace with address but removing releative name or trailing '/'
// %U => Replace with full address (protocol in front:  "rtsp://<.....>")
// %V => Replace with address but removing releative name or trailing '/' (protocol in front:  "rtsp://<.....>")
//
// static
BOOL ApplicationLauncher::ParseTrustedApplication( OpVector<OpString>& list,
												   const uni_char *application,
												   const uni_char *full_address,
												   const uni_char *address )
{
	if( !full_address || !application || !address )
	{
		return FALSE;
	}

	OpString cmd;
	cmd.Set( application );
	int pos = cmd.FindI("%u");
	if( pos != KNotFound )
	{
		OpString tmp1;
		tmp1.Set( cmd.CStr()[pos+1] == 'U' ? full_address : address );

		TruncateAfterSpaceDash( tmp1 ); // Vulnerability like bug #139961

		if( tmp1.Length() > 0 )
		{
			Replace( cmd, pos, 2, tmp1 );
		}
		else
		{
			cmd.Delete( pos, 2 );
		}
	}
	else
	{
		pos = cmd.FindI("%v");
		if( pos != KNotFound )
		{
			OpString tmp1, tmp2;
			tmp1.Set( cmd.CStr()[pos+1] == 'V' ? full_address : address );

			TruncateAfterSpaceDash( tmp1 ); // Vulnerability like bug #139961

			tmp2.Set( tmp1.Strip(TRUE,TRUE) );
			int p = tmp2.Find("/#");
			if( p == KNotFound )
			{
				p = FindRev( tmp2, tmp2.Length(), '/' );
				if( p != KNotFound && p != tmp2.Length()-1 )
				{
					p = KNotFound;
				}
			}

			if( p != KNotFound )
			{
				tmp1.Set( tmp2.SubString(0), p );
				tmp2.Set( tmp1 );
			}

			Replace( cmd, pos, 2, tmp2 );
		}
		else
		{
			// Important: Just append the address after the application name
			cmd.Append(" ");
			cmd.Append(full_address);
		}
	}

	StringUtils::SplitString( list, cmd, ' ', FALSE );

	return TRUE;
}

void ApplicationLauncher::DisplayDialogOnError(int return_value)
{
	if (return_value == -1)
	{
		OpString msg;
		if (OpStatus::IsSuccess(PosixNativeUtil::FromNative(strerror(errno), &msg)))
			SimpleDialog::ShowDialog(WINDOW_NAME_RUN_EXTERNAL_ERROR, 0, msg.CStr(), UNI_L("Opera"), Dialog::TYPE_OK, Dialog::IMAGE_ERROR );
	}
}
