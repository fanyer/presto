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

#include "unixutils.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/util/opfile/opfile.h"

#include "platforms/posix/posix_native_util.h"
#include "platforms/posix/posix_file_util.h"

#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/desktop_util/opfile/desktop_opfile.h"

#include <errno.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

extern void SSL_Process_Feeder();
#include "modules/libssl/ssl_api.h"

#ifndef PATHSEPCHAR
#define PATHSEPCHAR '/'
#endif // PATHSEPCHAR

namespace UnixUtils
{

	OP_STATUS FromNativeOrUTF8(const char* str, OpString *target)
	{
		int rc = PosixNativeUtil::FromNative(str, target);
		if (rc == OpStatus::ERR_PARSING_FAILED)
			rc = target->SetFromUTF8(str);
		return rc;
	}

	BOOL UseStrictFilePermission()
	{
		static int val = -1;
		if( val == -1 )
		{
			const char* env = op_getenv("OPERA_STRICT_FILE_PERMISSIONS");
			if( env && *env )
			{
				if( *env == '1' || !strcasecmp(env,"true") || !strcasecmp(env,"yes") )
					val = 1;
				else
					val = 0;
			}
			else
			{
				val = 0; // Default is system default as set by umask
			}
		}

		return val == 1;
	}

	BOOL CreatePathInternal(char* path)
	{
		if( !path )
		{
			return FALSE;
		}

		if( path[0] == 0 && path[1] != 0 )
		{
			// A 0-character can be used as a path separator as long as the next
			// is not a 0-character as well.
			path[0] = PATHSEPCHAR;
		}

		if( access(path, F_OK) == -1 )
		{
			int err_code;
			if( UseStrictFilePermission() )
			{
				mode_t mask = umask(77);
				err_code = mkdir(path, 777);
				umask(mask);
			}
			else
			{
				err_code = mkdir(path, 777);
			}

			if( err_code == -1 )
			{
				return FALSE;
			}
		}

		int path_length = strlen(path);
		if( path[path_length+1] == 0 )
		{
			return TRUE;
		}

		path[path_length] = PATHSEPCHAR;

		if( CreatePathInternal(path) )
		{
			return TRUE;
		}
		else
		{
			path[path_length] = 0;
			rmdir(path);
			return FALSE;
		}
	}

	OP_STATUS CreatePath(const uni_char* filename, BOOL stop_on_last_pathsep)
	{
		PosixNativeUtil::NativeString locale_string(filename);
		return CreatePath(locale_string.get(), stop_on_last_pathsep);
	}

	OP_STATUS CreatePath(const char* filename, BOOL stop_on_last_pathsep)
	{
		if( !filename || !*filename )
		{
			return OpStatus::ERR;
		}

		int filename_length = strlen(filename);

		char* path = OP_NEWA(char, filename_length+2);
		if( !path )
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		const char* last_separator = stop_on_last_pathsep ? strrchr(filename, PATHSEPCHAR) : 0;
		char* p = path;
		char prev_char = 0;

		for( int i=0; i<filename_length; i++ )
		{
			if( last_separator && (&filename[i] > last_separator) )
			{
				break;
			}

			if( filename[i] == PATHSEPCHAR && prev_char == PATHSEPCHAR )
			{
				continue;
			}
			prev_char = filename[i];
			*p++ = prev_char == PATHSEPCHAR ? 0 : prev_char;
		}

		*p++ = 0;
		*p++ = 0;

		OP_STATUS status = CreatePathInternal(path) ? OpStatus::OK : OpStatus::ERR;

		if (status == OpStatus::OK)
		{
			struct stat statbuf;
			if ( stat(path, &statbuf) == -1 || !S_ISDIR(statbuf.st_mode) )
			{
				// entry does not exist or is not a directory
				status = OpStatus::ERR;
			}
		}

		OP_DELETEA(path);

		return status;
	}


	BOOL SplitDirectoryAndFilename(const OpString& src, OpString& directory, OpString& filename)
	{
		OpString expanded;
		RETURN_VALUE_IF_ERROR(g_op_system_info->ExpandSystemVariablesInString(src, &expanded), FALSE);

		if (expanded.Find("/") != 0)
		{
			char buf[_MAX_PATH];
			if (getcwd(buf, MAX_PATH) != 0)
				return FALSE;
			OpString expanded_path;
			RETURN_VALUE_IF_ERROR(PosixNativeUtil::FromNative(buf, &expanded_path), FALSE);
			RETURN_VALUE_IF_ERROR(expanded_path.Append("/"),FALSE);
			RETURN_VALUE_IF_ERROR(expanded_path.Append(expanded),FALSE);
			expanded.TakeOver(expanded_path);
		}

		CanonicalPath(expanded);
		if (expanded.IsEmpty())
			return FALSE;

		OpString candidate;
		RETURN_VALUE_IF_ERROR(candidate.Set(expanded), FALSE);

		BOOL match = FALSE;
		while (candidate.HasContent())
		{
			OpFile file;
			if (OpStatus::IsSuccess(file.Construct(candidate)))
			{
				BOOL exists;
				if (OpStatus::IsSuccess(file.Exists(exists)) && exists)
				{
					OpFileInfo::Mode mode;
					if (OpStatus::IsSuccess(file.GetMode(mode)) && mode == OpFileInfo::DIRECTORY)
					{
						match = TRUE;
						break;
					}
				}
			}
			int pos = candidate.FindLastOf('/');
			if (pos == KNotFound || pos == 0)
			{
				if (pos == 0 && candidate.Length() > 1)
					candidate.Delete(pos+1);
				else
					break;
			}
			else
				candidate.Delete(pos);
		}

		if (match)
		{
			int len = candidate.Length();
			RETURN_VALUE_IF_ERROR(directory.Set(candidate), FALSE);
			if( len > 0 && directory.CStr()[len-1] != '/')
				RETURN_VALUE_IF_ERROR(directory.Append("/"), FALSE);

			OpString tmp;
			RETURN_VALUE_IF_ERROR(tmp.Set(&expanded.CStr()[len]), FALSE);
			int pos = tmp.FindLastOf('/');
			RETURN_VALUE_IF_ERROR(filename.Set(&tmp.CStr()[pos == KNotFound ? 0 : pos+1]), FALSE);
		}

		return match;
	}

	// Name is taken from QDir class. Does not do as much as Qt does yet.

	void CanonicalPath(OpString& candidate)
	{
		const uni_char* ptr = candidate.CStr();
		if( ptr )
		{
			OpString result;
			int length=0;

			while(ptr[length])
			{
				if(ptr[length] == '/' && ptr[length+1] == '/' )
				{
					length++;
					result.Append(ptr, length);
					ptr += length;

					while( ptr[0] == '/' ) ptr++;
					length=0;
				}
				else if (ptr[length] == '.' && ptr[length+1] == '.' && (ptr[length+2] == '/' || !ptr[length+2]))
				{
					length += 2;
					result.Append(ptr, length);
					ptr += length;
					length=0;

					int i = result.Length();

					if (i > 2 && result.CStr()[i-3] == '/')
					{
						if (i==3 && result.CStr()[1] == '.' && result.CStr()[2] == '.')
							result.Delete(1);
						else
						{
							for (i-=4; i>=0; i--)
							{
								if (result.CStr()[i] == '/')
								{
									result.Delete(i+1);
									// 'result' already ends with a '/' so we remove the rest if any
									while (ptr[0] == '/') ptr++;
									break;
								}
							}
						}
					}
				}
				else if( ptr[length] == '.' )
				{
					if( ptr[length+1] == '/' || ptr[length+1] == 0 )
					{
						length++;
						result.Append(ptr, length);
						ptr += length;
						length=0;

						int i = result.Length();
						if( i > 1 && result.CStr()[i-2]=='/')
						{
							OpString tmp;
							tmp.Set(result.CStr(), ptr[0] == 0 ? i-1 : i-2);
							result.Set(tmp);
						}
					}
					else
					{
						length++;
					}
				}
				else
				{
					length++;
				}
			}

			if (result.HasContent())
			{
				if (length > 0)
					result.Append(ptr, length);
				candidate.Set(result);
			}
		}
	}
	
	OP_STATUS CleanUpFolder(const OpString &path)
	{
		OpFile folder;
		RETURN_IF_ERROR(folder.Construct(path.CStr()));

		BOOL exists = FALSE;
		RETURN_IF_ERROR(folder.Exists(exists));
		if (exists)
			RETURN_IF_ERROR(folder.Delete());
		
		OpString fullpath;
		RETURN_IF_ERROR(fullpath.Set(path.CStr()));
		RETURN_IF_ERROR(fullpath.Append(UNI_L(".exist")));
		RETURN_IF_ERROR(PosixFileUtil::CreatePath(fullpath.CStr(), true)); // This only makes the folder, not the file '.exists'

		return OpStatus::OK;
	}

	int LockFile(const OpStringC& filename, int* file_descriptor)
	{
		if (filename.IsEmpty())
			return -1;

		// OpFile::Open() will create full path to file should it not exist already

		OpFile file;
		file.Construct(filename.CStr());
		file.Open(OPFILE_WRITE);
		file.Close();
		
		OpString8 tmp;
		tmp.Set(filename.CStr());
		if (!tmp.CStr())
			return -1;

		struct flock fl;
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = 0;
		fl.l_len = 0;
		fl.l_pid = getpid();
	
		int fd = open(tmp.CStr(), O_RDWR|O_CREAT, 666);
		if (fd == -1)
			return -1;
		
		if (fcntl(fd, F_SETLK, &fl) == -1)
		{
			int rc = errno == EACCES || errno == EAGAIN ? 0 : -1;
			close(fd);
			return rc;
		}

		if (file_descriptor)
			*file_descriptor = fd;
		
		return 1;
	}
	

	BOOL SearchInStandardFoldersL( OpFile &file, const uni_char* filename, BOOL allow_local, BOOL copy_file )
	{
		OP_ASSERT(filename);

		// filename may begin with "ini/" but we're trying to get rid of that ...
		const uni_char *const iniprefix = UNI_L("ini/");
		const BOOL hasprefix = (uni_strncmp(filename, iniprefix, 4) == 0);
		OP_ASSERT(!hasprefix); // project teams should get rid of the ini/ prefix.
		BOOL exists;

		OpString readwrite_path; ANCHOR(OpString, readwrite_path);
		LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, readwrite_path));
		readwrite_path.AppendL(UNI_L("/"));
		if (!hasprefix) readwrite_path.AppendL(iniprefix);
		readwrite_path.AppendL( filename );
		CanonicalPath(readwrite_path);

		if( allow_local )
		{
			// Look in write (personal) directory  By default /home/<user>/.opera
			LEAVE_IF_ERROR(file.Construct(readwrite_path.CStr()));
			LEAVE_IF_ERROR(file.Exists(exists));
			if ( exists )
				return TRUE;
		}

		OpString readonly_path; ANCHOR(OpString, readonly_path);
		// Look in read (shared) directory. By default /usr/share/opera
		LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(hasprefix ? OPFILE_RESOURCES_FOLDER : OPFILE_INI_FOLDER, readonly_path));
		readonly_path.AppendL(UNI_L("/"));
		readonly_path.AppendL( filename );
		CanonicalPath(readonly_path);

		LEAVE_IF_ERROR(file.Construct(readonly_path.CStr()));
		LEAVE_IF_ERROR(file.Exists(exists));
		if( !exists )
		{
			/* Look in current directory. Can work if the user runs an opened
			 * .tar.gz file for testing or evaluation */
			readonly_path.SetL( filename );
			LEAVE_IF_ERROR(file.Construct(readonly_path.CStr()));
			LEAVE_IF_ERROR(file.Exists(exists));
		}

		if( exists )
		{
			if ( !copy_file ) return TRUE;

			OpFile dest, src; ANCHOR(OpFile, dest); ANCHOR(OpFile, src);
			LEAVE_IF_ERROR(dest.Construct( readwrite_path.CStr())); // assert: dest doesn't exist
			LEAVE_IF_ERROR(src.Construct( readonly_path.CStr()));
			LEAVE_IF_ERROR(dest.CopyContents(&src, TRUE));
			LEAVE_IF_ERROR(dest.Exists(exists));
			if( exists )
			{
				if ( !allow_local ) return TRUE;

				LEAVE_IF_ERROR(file.Construct( readwrite_path.CStr()));
				LEAVE_IF_ERROR(file.Exists(exists));
				if ( exists ) return TRUE;
				else if (filename)
				{
					char *name = uni_down_strdup(filename);
					if (name) {
						fprintf(stderr, "opera: Failed to use local \"%s\"\n", name);
						op_free( name );
					}
				}
			}
			else if (filename)
			{
				char *name = uni_down_strdup(filename);
				if (name) {
					fprintf(stderr, "opera: Could not copy \"%s\"\n", name );
					op_free( name );
				}
			}

			// Use shared file as fallback
			LEAVE_IF_ERROR(file.Construct(readonly_path.CStr()));
			LEAVE_IF_ERROR(file.Exists(exists));
			return exists;
		}

		if (filename)
		{
			char *name = uni_down_strdup(filename);
			if (name)
			{
				fprintf(stderr, "opera: Could not find \"%s\"\n", name);
				op_free( name );
			}
		}
		return FALSE;
	}

	/* The random animal's psychedelic breakfast.
	 *
	 * Call this during start-up, after g_opera->InitL() has succeeded, so that
	 * the random animal doesn't growl at us while we're https loading pages
	 * into all the open tabs the user starts with, if the user isn't doing
	 * stuff with mouse and keyboard that'll feed the random animal.
	 *
	 * Normally, we wouldn't use /dev/urandom because it's already being used by
	 * libssl, so we'd just be duplicating effort.  However, there is (at least)
	 * no risk of libssl getting the same bytes as we serve up.
	 *
	 * Note that, if /dev/hwrandom is present, rather than us trying to use it,
	 * we should rely on (and encourge) the user to use the rng-tools package;
	 * its rngd will feed randomness from /dev/hwrandom into /dev/random's pool
	 * of entropy.  Crucially, rngd will do some testing to check /dev/hwrandom
	 * actually works; if not, it leaves /dev/random to its own devices.
	 */
	bool BreakfastSSLRandomEntropy()
	{
		// Ensure the random animal is up and about:
		if (!g_opera || // No chance if the day hasn't started yet.
			!g_ssl_api->CheckSecurityManager() || // That should do it, but just make sure:
			!SSL_RND_feeder_data)
		{
			OP_ASSERT(!"Yngve said this would only happen if you were about to die of OOM anyway"); // Eddy/2007/11/20
			return false;
		}
		if (SSL_RND_feeder_pos >= SSL_RND_feeder_len)
			// The random animal isn't hungry.
			return false;

		// Find a sauce of randomness:
		FILE *fd = fopen("/dev/urandom", "r");
#ifdef BLOCKING_FOR_A_FEW_MINUTES_IS_NO_PROBLEM
		if (fd == 0)
			fd = fopen("/dev/random", "r");
#endif
		if (fd == 0)
			return false;

		// Even for a greedy random animal, 1K noisy words should be enough:
		const size_t len = min(0x400u, SSL_RND_feeder_len - SSL_RND_feeder_pos);
		UINT32 *porridge = OP_NEWA(UINT32, len);
		if (!porridge)
			return false;

		// Pour out the sauce:
		const size_t got = fread(porridge, sizeof(UINT32), len, fd);
		// Put the lid back on the bottle:
		fclose(fd);

		if (got > 0)
		{
			// We didn't ask for more than the random animal could swallow:
			OP_ASSERT(SSL_RND_feeder_pos + got <= SSL_RND_feeder_len);
			// Feed it:
			op_memcpy(porridge, SSL_RND_feeder_data + SSL_RND_feeder_pos, got * sizeof(UINT32));
			SSL_RND_feeder_pos += got;
		}
		// Wash up:
		OP_DELETEA(porridge);
		return got > 0;
	}

	/* Throw the random animal some scraps generated from noisy words from X and
	 * Qt events caused by user interaction.
	 */
	void FeedSSLRandomEntropy(UINT32 noise)
	{
		if (!SSL_RND_feeder_data)
			return;

		static int last_stamp;
		timeval now;
		gettimeofday(&now, 0);

		if (SSL_RND_feeder_pos + 2 > SSL_RND_feeder_len)
			SSL_Process_Feeder();

		if (SSL_RND_feeder_pos <= SSL_RND_feeder_len)
		{
			SSL_RND_feeder_data[SSL_RND_feeder_pos++] = now.tv_usec - last_stamp;
			last_stamp = now.tv_usec;

			if (SSL_RND_feeder_pos <= SSL_RND_feeder_len)
				SSL_RND_feeder_data[SSL_RND_feeder_pos++] = noise;
		}
	}

	int CalculateScaledSize(int src, double scale)
	{
		if (src <= 0)
			return 0;
		int result;

		result = (int)(scale * (double)src + 0.5);
		return result > 0 ? result : 1;
	}


	bool SnapFontSize(int req_size, int avail_size)
	{
		if (!req_size || !avail_size)
		{
			OP_ASSERT(FALSE);
			return false;
		}

		int max_larger = 0;
		int max_smaller = 0;

		if (req_size < 8)
		{
			max_larger = max_smaller = 1;
		}
		else if (req_size < 12)
		{
			max_larger = 1;
			max_smaller = 2;
		}
		else if (req_size < 24)
		{
			max_larger = 2;
			max_smaller = req_size > 16 ? 3 : 2;
		}
		else
		{
			max_larger = 12 * req_size / 100;
			max_smaller = 18 * req_size / 100;
		}

		if (req_size > avail_size)
			return req_size - avail_size <= max_larger;
		else
			return avail_size - req_size <= max_smaller;
	}

	int GrandFork()
	{
		pid_t pid = fork();
		if (pid < 0)
			return -1; // failed
		else if (pid)
		{
			// original process
			int stat = 0;
			waitpid(pid, &stat, 0);
			// Decode top-level child's _exit() status (see below):
			return (!WIFEXITED(stat) || WEXITSTATUS(stat)) ? -1 : 1;
		}

		/* Top-level child.
		 *
		 * Note that we use _exit() rather than exit() to avoid calling atexit()
		 * hooks, which we've inherited from our parent, who'd rather *not* have the
		 * child "tidy away" various things it's probably still using ...
		 */
		pid = fork();
		if( pid < 0 )
			_exit( 1 ); // Failure. Caught above

		else if( pid > 0 )
			_exit( 0 ); // Ok. Caught above

		else
			return 0; // grand-child. This will do the job
	}

	
	void PromptMissingFilename(DesktopWindow* parent)
	{
		SimpleDialog::ShowDialog(WINDOW_NAME_MISSING_FILENAME, parent, Str::DI_OPERA_QT_MUSTSELECTFILE, Str::DI_OPERA_QT_MUSTSELECTFILE_TITLE, Dialog::TYPE_OK, Dialog::IMAGE_WARNING);
	}

	void PromptFilenameIsDirectory(DesktopWindow* parent)
	{
		SimpleDialog::ShowDialog(WINDOW_NAME_FILE_IS_DIR, parent, Str::DI_OPERA_QT_EXISTS_SELECTNEW, Str::DI_OPERA_QT_FILE_EXISTS_ILLEGAL,Dialog::TYPE_OK, Dialog::IMAGE_WARNING );
	}

	void PromptNoWriteAccess(DesktopWindow* parent, const OpStringC& filename)
	{
		if (filename.IsEmpty())
		{
			SimpleDialog::ShowDialog(WINDOW_NAME_NO_WRITE_ACCESS, parent, Str::DI_OPERA_QT_FILE_NO_WRITE_ACCESS, Str::DI_OPERA_QT_FILE_ACCESS_DENIED_TITLE,Dialog::TYPE_OK, Dialog::IMAGE_WARNING );
		}
		else
		{
			OpString title;
			TRAPD(rc,g_languageManager->GetStringL(Str::DI_OPERA_QT_FILE_ACCESS_DENIED_TITLE, title));
			
			OpString msg;
			TRAP(rc,g_languageManager->GetStringL(Str::DI_OPERA_QT_FILE_NO_WRITE_ACCESS, msg));

			OpString s;
			s.Set(msg);
			s.Append("\n\n");
			s.Append(filename);

			SimpleDialog::ShowDialog(WINDOW_NAME_NO_WRITE_ACCESS , parent, s.CStr(), title.CStr(),Dialog::TYPE_OK, Dialog::IMAGE_WARNING);
		}
	}


	BOOL PromptFilenameAlreadyExist(DesktopWindow* parent, const OpStringC& filename, BOOL ask_continue)
	{
		if (ask_continue)
		{
			OpString msg, title, tmp;

			TRAPD(rc, g_languageManager->GetStringL(Str::DI_OPERA_QT_FILE_EXISTS, tmp));
			TRAP(rc, g_languageManager->GetStringL(Str::DI_OPERA_QT_FILE_EXISTS_REPLACE, title));

			msg.Set(filename);
			msg.Append("\n\n");
			msg.Append(tmp);

			rc = SimpleDialog::ShowDialog( WINDOW_NAME_FILENAME_EXISTS, parent, msg.CStr(), title.CStr(), Dialog::TYPE_YES_NO, Dialog::IMAGE_WARNING);
			return rc == Dialog::DIALOG_RESULT_OK;
		}
		else
		{
			SimpleDialog::ShowDialog(WINDOW_NAME_FILENAME_EXISTS, parent, Str::DI_OPERA_QT_EXISTS_SELECTNEW, Str::DI_OPERA_QT_FILE_EXISTS_ILLEGAL,Dialog::TYPE_OK, Dialog::IMAGE_WARNING );
			return FALSE;
		}
	}

	BOOL CheckFileToWrite(DesktopWindow* parent, const OpStringC& candidate, BOOL ask_replace)
	{
		OpString filename;
		filename.Set(candidate);
		filename.Strip();

		if (filename.IsEmpty())
		{
			PromptMissingFilename(parent);
			return FALSE;
		}

		OpFile file;
		file.Construct(filename);

		BOOL exists = FALSE;
		if (OpStatus::IsSuccess(file.Exists(exists)) && exists)
		{
			OpFileInfo info;
			info.flags = OpFileInfo::WRITABLE | OpFileInfo::MODE;
			RETURN_VALUE_IF_ERROR(file.GetFileInfo(&info), FALSE);
			if (info.mode == OpFileInfo::DIRECTORY)
			{
				PromptFilenameIsDirectory(parent);
				return FALSE;
			}

			if (!info.writable)
			{
				PromptNoWriteAccess(parent, filename);
				return FALSE;
			}

			BOOL ok_to_overwrite = PromptFilenameAlreadyExist(parent, filename, ask_replace);
			if (!ok_to_overwrite)
				return FALSE;
		}
		else
		{
			OpString directory;
			file.GetDirectory(directory);
			PosixNativeUtil::NativeString directory_path (directory);
			if (directory_path.get() && access(directory_path.get(), W_OK))
			{
				PromptNoWriteAccess(parent, filename);
				return FALSE;
			}
		}

		return TRUE;
	}

	BOOL HasEnvironmentFlag(const char* name)
	{
		OpString8 str;
		str.Set(getenv(name));
		return str.HasContent() && (!str.Compare("1") || !str.CompareI("yes") || !str.CompareI("true"));
	}

    OP_STATUS UnserializeFileName(const uni_char* in, OpString* out)
    {
        if (in && *in == '{') {
            if (!out)
                return OpStatus::ERR;
            if (!out->CStr()) // start with an empty string:
                RETURN_IF_ERROR(out->Set(""));
            else // clear the string without deallocating the buffer:
                out->Delete(0);

            const char* expansion;

            OpStringC src = in;
            int in_pos = src.FindFirstOf('}');
            if (in_pos == KNotFound)
                return OpStatus::OK; // Syntax error, return empty string
            in_pos++;

            if (src.Compare(UNI_L("{Home}"), in_pos) == 0
                || src.Compare(UNI_L("{Users}"), in_pos) == 0)
            {
                expansion = op_getenv("HOME");
            }
            else if (src.Compare(UNI_L("{Preferences}"), in_pos) == 0
                || src.Compare(UNI_L("{SmallPreferences}"), in_pos) == 0
                || src.Compare(UNI_L("{LargePreferences}"), in_pos) == 0)
            {
                expansion = op_getenv("OPERA_PERSONALDIR");
            }
            else if (src.Compare(UNI_L("{Binaries}"), in_pos) == 0)
            {
                expansion = op_getenv("OPERA_BINARYDIR");
            }
            else if (src.Compare(UNI_L("{Resources}"), in_pos) == 0)
            {
                expansion = op_getenv("OPERA_DIR");
            }
            else if (src.Compare(UNI_L("{Root}"), in_pos) == 0)
            {
                expansion = "/";
            }
            else
            {
                return OpStatus::OK; // Syntax error, return empty string
            }

            RETURN_IF_ERROR(PosixNativeUtil::FromNative(expansion, out));
            if (out->IsEmpty())
                return OpStatus::OK;

            // Skip slashes in the input string
            while (src[in_pos] == '/')
                in_pos++;

            // Append the rest of the input if there is any
            if (src[in_pos])
            {
                // Add a slash if there isn't one
                int out_pos = out->Length() - 1;
                if (out_pos < 0 || (*out)[out_pos] != '/')
                    RETURN_IF_ERROR(out->Append(UNI_L("/")));

                RETURN_IF_ERROR(out->Append(in + in_pos));
            }

            return OpStatus::OK;
        }
        else
            // Try old-school variable expansion
            return PosixFileUtil::DecodeEnvironment(in, out);
    }

    uni_char* SerializeFileName(const uni_char *path)
    {
        struct { const char* env_name; const uni_char* var; } subst[] = {
            { "OPERA_PERSONALDIR", UNI_L("{Preferences}")},
            { "OPERA_BINARYDIR",   UNI_L("{Binaries}")},
            { "OPERA_DIR",         UNI_L("{Resources}")},
            { "HOME",              UNI_L("{Home}")}
        };

        int mylen = uni_strlen(path), len;
        const uni_char* use_var = NULL;

        for (unsigned i = 0; i < sizeof(subst)/sizeof(*subst); i++)
        {
            OpString value;
            RETURN_VALUE_IF_ERROR(PosixNativeUtil::FromNative(op_getenv(subst[i].env_name), &value), NULL);

            // Skip trailing slashes
            len = value.Length();
            while (len > 0 && value[len - 1] == '/')
                len--;

            // Ignore if empty or points to root
            if (!len)
                continue;

            // If matches all or prefix followed by a slash, use it
            if ((len == mylen || len < mylen && path[len] == '/')
                && value.Compare(path, len) == 0)
            {
                use_var = subst[i].var;
                break;
            }

            // Try the real path, too
            char full[_MAX_PATH + 1];
            if (realpath(op_getenv(subst[i].env_name), full)) {
                RETURN_VALUE_IF_ERROR(PosixNativeUtil::FromNative(full, &value), NULL);
                len = value.Length();
                while (len > 0 && value[len - 1] == '/')
                    len--;
                if (!len)
                    continue;
                if ((len == mylen || len < mylen && path[len] == '/')
                    && value.Compare(path, len) == 0)
                {
                    use_var = subst[i].var;
                    break;
                }
            }
        }

        // Use {Root} if the path contains special characters
        if (!use_var && uni_strpbrk(path, UNI_L("$~\\")))
        {
            use_var = UNI_L("{Root}");
            len = 0;
        }

        uni_char* res;
        if (use_var) {
            int varlen = uni_strlen(use_var);

            // Skip following slashes in source string
            while (path[len] == '/')
                len++;

            res = OP_NEWA(uni_char, mylen - len + varlen + 1);
            if (!res)
                return NULL;

            uni_strcpy(res, use_var);
            uni_strcpy(res + varlen, path + len);
        } else {
            res = OP_NEWA(uni_char, mylen + 1);
            if (!res)
                return NULL;

            uni_strcpy(res, path);
        }

        return res;
    }

}
