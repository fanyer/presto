/* -*- Mode: c++; indent-tabs-mode: t; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_SYS // API_POSIX_SYS

# ifndef POSIX_INTERNAL
#define POSIX_INTERNAL(X) X
# endif
#include "platforms/posix/posix_system_info.h"
#include "platforms/posix/posix_native_util.h"
#include "platforms/posix/posix_file_util.h"
#include "modules/encodings/utility/charsetnames.h"

#if defined(POSIX_OK_HOST) || defined(POSIX_OK_DNS)
#include <sys/utsname.h>
#endif // API_POSIX_HOST or API_POSIX_DNS

#ifdef POSIX_OK_PROCESS
#include <unistd.h>
# ifdef EXTERNAL_APPLICATIONS_SUPPORT
#include <sys/wait.h>
#include <signal.h>
# endif
#endif // API_POSIX_PROCESS

#ifdef POSIX_OK_SYS_LOCALE
#include <locale.h>
# ifdef POSIX_HAS_LANGINFO
#include <langinfo.h>
# endif
#endif // API_POSIX_LOCALE

#include <sys/times.h>

PosixSystemInfo::PosixSystemInfo()
	: PosixOpSysInfoBase() // no-op (makes punctuation OK with #if-ery !)
#ifdef OPSYSTEMINFO_GETPROCESSTIME
	, m_cpu_tick_rate(sysconf(_SC_CLK_TCK))
#endif // OPSYSTEMINFO_GETPROCESSTIME
#ifdef POSIX_USE_PROC_CPUINFO
    , m_cpu_info_initialized(FALSE)
# ifdef OPSYSTEMINFO_CPU_FEATURES
    , m_cpu_features(CPU_FEATURES_NONE)
# endif // OPSYSTEMINFO_CPU_FEATURES
# ifdef OPSYSTEMINFO_CPU_ARCHITECTURE
    , m_cpu_architecture(CPU_ARCHITECTURE_UNKNOWN)
# endif // OPSYSTEMINFO_CPU_ARCHITECTURE
#endif // POSIX_USE_PROC_CPUINFO
	  // m_zone initializes itself
{
}

/** Initialize cached data for non-LEAVE()ing methods.
 *
 * Various OpSystemInfo methods that aren't allowed to LEAVE are, none the less,
 * capable of failing.  However, if we can cache their values when creating the
 * system info object, we can have the failure happen at creation instead.
 */
OP_STATUS PosixSystemInfo::Construct()
{
#if defined(POSIX_OK_HOST) || defined(POSIX_OK_DNS)
	struct utsname buf;
	int unret = uname(&buf);
#endif // HOST or DNS

#ifdef POSIX_OK_HOST
	// set m_platform
	if (unret >= 0)
	{
		RETURN_IF_ERROR(PosixNativeUtil::FromNative(
							buf.sysname, m_platform, POSIX_PLATNAME_LENGTH));
	}
	else // Fall-backs:
	{
		uni_strlcpy(m_platform, UNI_L("UNIX"), POSIX_PLATNAME_LENGTH);
	}
#endif // POSIX_OK_HOST

#ifdef POSIX_OK_DNS // API_POSIX_DNS
# ifdef _POSIX_HOST_NAME_MAX
	const size_t len = _POSIX_HOST_NAME_MAX;
# elif defined(HOST_NAME_MAX)
	const size_t len = HOST_NAME_MAX;
# else
	const size_t len = 0xff;
# endif

	char localname[len];		// ARRAY OK 2010-08-12 markuso
	if (0 == gethostname(localname, len))
		RETURN_IF_ERROR(m_host_name.Set(localname, len));
	else if (unret >= 0)
		RETURN_IF_ERROR(m_host_name.Set(buf.nodename));

# ifdef OPSYSTEMINFO_GETSYSTEMIP // see ../net/posix_interface.cpp
	RETURN_IF_ERROR(QueryLocalIP());
# endif // OPSYSTEMINFO_GETSYSTEMIP
#endif // POSIX_OK_DNS

	return OpStatus::OK;
}

#ifdef POSIX_OK_SYS_LOCALE
inline const char *PosixSystemInfo::GetLanginfoEncodingL() const
{
#ifdef POSIX_HAS_LANGINFO
	PosixNativeUtil::TransientLocale ignoreme(LC_CTYPE);
	if (!ignoreme.Happy())
		LEAVE(OpStatus::ERR);

	const char *const canonical_encoding =
		g_charsetManager->GetCanonicalCharsetName(nl_langinfo(CODESET));
	if (canonical_encoding)
		return canonical_encoding;
#endif
	return POSIX_SYS_DEFAULT_ENCODING;
}

const char *PosixSystemInfo::GetSystemEncodingL()
{
	if (m_system_encoding.IsEmpty())
		m_system_encoding.SetL(GetLanginfoEncodingL());

	return m_system_encoding.CStr();
}

OP_STATUS PosixSystemInfo::LookupUserLanguages(OpString *result)
{
	result->Empty();

	/* Unix systems use several different environment variables for user
	 * locale settings:
	 *
	 *  LANGUAGE (GNU extension)
	 *        a colon-separated list of locales
	 *  LC_ALL
	 *        overrides all other LC_* defines
	 *  LC_MESSAGES
	 *        setting for the message locale, which is what we are interested in
	 *  LANG
	 *        fallback for missing LC_* defines
	 *
	 * Since the syntax is so similar (LANGUAGE is the only one with a list),
	 * just use the first one that is available.  (Array is traversed in reverse
	 * order.)
	 */
	const char* vars[] = { "LANG", "LC_MESSAGES", "LC_ALL", "LANGUAGE" };
	for (int i = sizeof(vars) / sizeof(vars[0]); i-- > 0 && result->IsEmpty(); )
	{
		OpString8 env, token_string;
		RETURN_IF_ERROR(env.Set(op_getenv(vars[i])));
		while (env.HasContent())
		{
			/* The format is (usually) more or less what we want.  At least on
			 * GNU systems.  That is, "language[_region][.codeset][@modifier]".
			 * We clip everything after the full stop or at sign, keep the rest
			 * as is, and make the colon-separated list into a comma-separated
			 * one. */

			int cut = env.FindFirstOf(':');
			OpString8 fragment;
			const int length = env.SpanNotOf(".@:");
			OP_ASSERT(length >= 0 && (cut < 0 || length <= cut));
			RETURN_IF_ERROR(fragment.Set(env.CStr(), length));
			env.Delete(0, cut < 0 ? KAll : cut + 1);
			fragment.Strip(TRUE, TRUE);
			if (fragment.IsEmpty())
				continue;
			if (fragment.Compare("C") == 0 ||
				fragment.Compare("POSIX") == 0)
				RETURN_IF_ERROR(fragment.Set("en"));

			if (token_string.HasContent())
				RETURN_IF_ERROR(token_string.Append(","));
			RETURN_IF_ERROR(token_string.Append(fragment));

			int ulen = fragment.FindFirstOf('_');
			/* If the part before the '_' is two or three letters long, it is a
			 * generic language code, which we should include somewhere later in
			 * the list.  If there is no '_' or the part before it is of some
			 * other length, it's just some random thing we shouldn't try to
			 * parse, e.g. i_klingon is a valid IANA language code.
			 */
			if (ulen < 2 || ulen > 3)
				continue;
			// else: our new language has a region specifier

			fragment.Delete(ulen); // The matching generic language.
			bool need = true; // Do we need to append this matching generic language ?
			if (env.HasContent())
			{
				// Don't insert generic before a later region variant:
				const char *later = env.CStr() - 1;
				while (later++)
					if (fragment.CompareI(later, ulen) == 0 &&
						op_strchr(":_.@", later[ulen]))
					{
						need = false;
						break;
					}
					else if (!op_isspace(*later))
						later = op_strchr(later, ':');
			}
			if (need)
			{
				char *prior = token_string.CStr() - 1;
				while (prior++)
					/* NB: no need to check prior[ulen] != '\0', as we've
					 * already appended the current region-specific token;
					 * and we wouldn't count it even if it did match. */
					if (fragment.CompareI(prior, ulen) == 0 &&
						prior[ulen] == ',')
					{
						need = false;
						break;
					}
					else
						prior = op_strchr(prior, ',');

				if (need)
				{
					RETURN_IF_ERROR(token_string.Append(","));
					RETURN_IF_ERROR(token_string.Append(fragment));
				}
			}
		}
		RETURN_IF_ERROR(result->Set(token_string));
	}

	return OpStatus::OK;
}

void
PosixSystemInfo::GetUserCountry(uni_char result[3])
{
	OpString s;
	LookupUserLanguages(&s);

	// Pick out territory (see GetUserLanguage)
	const int pos = s.FindFirstOf('_');
	if (pos != KNotFound && s.Length() > pos + 2)
		uni_strlcpy(result, s.CStr() + pos + 1, 3);
	else
		result[0] = 0;
}

# ifdef EXTERNAL_APPLICATIONS_SUPPORT
const uni_char* PosixSystemInfo::GetDefaultTextEditorL()
{
	if (m_default_text_editor.IsEmpty())
	{
		uni_char* editor = PosixNativeUtil::UniGetEnvL("VISUAL");
		if (!editor || !editor[0])
		{
			OP_DELETEA(editor);
			editor = PosixNativeUtil::UniGetEnvL("EDITOR");
		}
		if (editor && editor[0])
		{
			OP_STATUS err = m_default_text_editor.Set(editor);
			OP_DELETEA(editor);
			LEAVE_IF_ERROR(err);
		}
		else
		{
			OP_DELETEA(editor); // In case it was non-NULL but empty.
			OP_STATUS err;
			{
#ifdef POSIX_HANDLER_AS_EDITOR
				// Use text/plain handler as fall-back, if set:
				OpString dummy_filename;
				OpString dummy_content_type;
				err = dummy_filename.Set("txt.txt");
				if (OpStatus::IsSuccess(err))
					err = dummy_content_type.Set("text/plain");
				if (OpStatus::IsSuccess(err))
					err = g_op_system_info->GetFileHandler(&dummy_filename,
														   dummy_content_type,
														   m_default_text_editor);
				if (OpStatus::IsError(err) || m_default_text_editor.IsEmpty())
#endif
				// final fall-back:
				err = m_default_text_editor.Set("xedit");
			} // dummy strings release their memory on leaving scope, so now we can:
			LEAVE_IF_ERROR(err);
		}
	}

	return m_default_text_editor.CStr();
}
# endif
#endif // POSIX_OK_SYS_LOCALE

#ifdef POSIX_OK_FILE_UTIL
# ifdef OPSYSTEMINFO_FILEUTILS
/** Can we run the named file ? */
BOOL PosixSystemInfo::GetIsExecutable(OpString* filename)
{
	if( filename )
	{
		PosixNativeUtil::NativeString name(filename->CStr());
		if (OpStatus::IsError(name.Ready()))
			return FALSE;

		struct stat buf;
		if (stat(name.get(), &buf) == 0 &&
			(buf.st_mode & S_IFREG) &&
			access(name.get(), X_OK) == 0)
			return TRUE;

		if (op_strchr(name.get(), PATHSEPCHAR) == NULL) // No / in name; so search PATH for it
		{
			const size_t nlen = op_strlen(name.get()) + 1; // include the trailing '\0'
			const char *path = op_getenv("PATH");

			while (path)
			{
				const char * colon = op_strchr(path, ':');
				if (colon == 0)
					colon = path + op_strlen(path);

				if (size_t plen = colon - path) // Ignore empty PATH entries.
				{
					// Do we need a separator ?
					size_t slash = colon[-1] == PATHSEPCHAR ? 0 : 1;
					char *copy = OP_NEWA(char, plen + slash + nlen);
					if (copy)
					{
						op_memcpy(copy, path, plen);
						if (slash)
							copy[plen] = PATHSEPCHAR;
						op_memcpy(copy + plen + slash, name.get(), nlen);

						int ret = access(copy, X_OK);
						OP_DELETEA(copy);

						if (ret == 0)
							return TRUE;
					}
				}

				path = colon[0] ? colon + 1 : 0;
			}
		}
	}

	return FALSE;
}
# endif // OPSYSTEMINFO_FILEUTILS

# ifdef POSIX_OK_NATIVE
OP_STATUS PosixSystemInfo::PathsEqual(const uni_char* p0, const uni_char* p1, BOOL* equal)
{
	if (!(p0 && p1 && equal))
		return OpStatus::ERR_NULL_POINTER;

	if (uni_strcmp(p0, p1) == 0)
	{
		*equal = TRUE;
		return OpStatus::OK;
	}

	char r0[_MAX_PATH+1];	// ARRAY OK 2010-08-12 markuso
	RETURN_IF_ERROR(PosixFileUtil::RealPath(p0, r0));
	char r1[_MAX_PATH+1];	// ARRAY OK 2010-08-12 markuso
	RETURN_IF_ERROR(PosixFileUtil::RealPath(p1, r1));
	*equal = !op_strncmp(r0, r1, _MAX_PATH);

#ifdef POSIX_PATHSEQUAL_HARDLINK
	if (!*equal)
	{
		struct stat st0, st1;
		if (stat(r0, &st0) == 0 && stat(r1, &st1) == 0)
			*equal = (st0.st_dev == st1.st_dev &&
					  st0.st_ino == st1.st_ino);
		// else stat failure fails to contradict !*equal
	}
#endif // POSIX_PATHSEQUAL_HARDLINK

	return OpStatus::OK;
}
# endif // POSIX_OK_NATIVE
#endif // POSIX_OK_FILE_UTIL

#ifdef OPSYSTEMINFO_GETPROCESSTIME
/* virtual */ OP_STATUS
PosixSystemInfo::GetProcessTime(double *time)
{
	// 'm_cpu_tick_rate' contains ticks per second.
	if (m_cpu_tick_rate == -1)
		return OpStatus::ERR_NOT_SUPPORTED;

	struct tms t;

	clock_t err = times(&t);

	if (err == static_cast<clock_t>(-1))
	{
		switch (errno)
		{
		case EACCES: return OpStatus::ERR_NO_ACCESS;
		case ENOMEM: return OpStatus::ERR_NO_MEMORY;
		default: return OpStatus::ERR;
		}
	}

	clock_t total = t.tms_utime + t.tms_stime;

	*time = total * 1e3 / m_cpu_tick_rate;

	return OpStatus::OK;
}
#endif // OPSYSTEMINFO_GETPROCESSTIME

#ifdef POSIX_OK_MISC

#undef LLONG
#undef LLFMT
#undef STRTOULL // turns string to ull (wool)
# ifdef HAVE_LONGLONG // system system
#define LLFMT "%llu"
#define LLONG long long
#define STRTOULL strtoull
# else
#define LLFMT "%lu"
#define LLONG long
#define STRTOULL strtoul
# endif // HAVE_LONGLONG
/* With any luck, any half-way competent compiler can discard the assertions on
 * sizeof(OpFileLength) at compile-time, so the cost of asserting it "every
 * time" we call the function (rather than just once during start-up) isn't an
 * issue - unless the assertion is failing, in which case ... see its message !
 */

OP_STATUS PosixSystemInfo::OpFileLengthToString(OpFileLength length, OpString8* result)
{
	OP_ASSERT(sizeof(unsigned LLONG) == sizeof(OpFileLength) ||
			  // Check SYSTEM_LONGLONG is correct; and that OpFileLength is sensible.
			  !"Derived class needs to over-ride this method");
	char buf[UINT_STRSIZE(unsigned LLONG) + 1]; // digits, '\0' // ARRAY OK 2010-08-12 markuso
	op_sprintf(buf, LLFMT, static_cast<unsigned LLONG>(length));
	return result->Set(buf);
}
#undef LLONG
#undef LLFMT
#undef STRTOULL

#endif // POSIX_OK_MISC

#ifdef POSIX_SERIALIZE_FILENAME
/* TODO: move to PosixFileUtil as static methods; have pi's virtuals call
 * those. */
OP_STATUS PosixSystemInfo::ExpandSystemVariablesInString(const uni_char* in, OpString* out)
{
	return PosixFileUtil::DecodeEnvironment(in, out);
}

uni_char* PosixSystemInfo::SerializeFileName(const uni_char *path)
{
	uni_char *ans;
	if (OpStatus::IsSuccess(PosixFileUtil::EncodeEnvironment(path, &ans)))
		return ans;
	return 0;
}
#endif // POSIX_SERIALIZE_FILENAME


#ifdef POSIX_OK_PATH
# ifdef POSIX_EXTENDED_SYSINFO
BOOL PosixSystemInfo::RemoveIllegalFilenameCharacters(OpString& path, BOOL replace)
{
	/* FIXME: when we open the file, we'll convert its utf-16 path into a wcs in
	 * which the locale-dependent encoding may cause some '/' characters to
	 * appear even though they were not present in the original.
	 */
	if (replace)
	{
		for (int i = path.Length(); i-- > 0;)
			if (path[i] == PATHSEPCHAR)
				path[i] = '_';
	}
	else
	{
		const size_t end = path.Length();
		size_t i = 0;
		while (i < end && path[(unsigned int)i] != PATHSEPCHAR)
			i++;
		size_t j = i++;
		while (i < end)
			if (path[(unsigned int)i] == PATHSEPCHAR)
				i++;
			else
				path[(unsigned int)j++] = path[(unsigned int)i++];
		path[(unsigned int)j] = 0;
	}

	path.Strip();
	return path.HasContent();
}

BOOL PosixSystemInfo::RemoveIllegalPathCharacters(OpString& path, BOOL replace)
{
	path.Strip();
	return path.HasContent();
}

OP_STATUS PosixSystemInfo::GetIllegalFilenameCharacters(OpString *illegalchars)
{
	/* FIXME: unicode characters, encoded in our utf-16 strings, may decode to
	 * multi-byte strings that include a '/' as one of the bytes encoding some
	 * code-point that wasn't '/' in our utf-16.  Any such character needs to be
	 * included in this returned string.
	 */
	if (illegalchars)
		return illegalchars->Set(PATHSEP);

	return OpStatus::ERR_NULL_POINTER;
}
# endif // POSIX_EXTENDED_SYSINFO
#endif // POSIX_OK_PATH


#ifdef POSIX_OK_PROCESS // API_POSIX_PROCESS
# ifdef EXTERNAL_APPLICATIONS_SUPPORT
static int grandfork(void)
{
	/* Creates a grand-child process to run an external program.
	 *
	 * A process, when complete, sends SIGCHLD to its parent and becomes defunct
	 * (a.k.a. a zombie) until its parent wait()s for it.  If the parent has
	 * died before the child, the child just exits peacefully (in fact because
	 * its parent's death orphaned it, at which point it was adopted by the init
	 * process, pid = 1, which ignores child death).
	 *
	 * This function does a double-fork and waits for the primary child's exit;
	 * the grand-child gets a return of 0 and should exec..() something; the
	 * original process doesn't need to handle SIGCHLD.  If the primary fork
	 * fails, or the primary child is unable to perform the secondary fork, the
	 * parent process sees a negative return value; otherwise, a positive one.
	 */
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

OP_STATUS PosixSystemInfo::PosixExecuteApplication(const uni_char* application,
												   const uni_char* args,
												   BOOL)
{
	if (!application) // no command !
		return OpStatus::ERR_NULL_POINTER;
	if (!application[0]) // empty command :-(
		return OpStatus::ERR;

	OpString command;
	RETURN_IF_ERROR(command.Set(application));
	if (args)
	{
		RETURN_IF_ERROR(command.Append(UNI_L(" ")));
		RETURN_IF_ERROR(command.Append(args));
	}
	PosixNativeUtil::NativeString native(command.CStr());
	command.Empty();
	RETURN_IF_ERROR(native.Ready());

#ifdef POSIX_USE_SIGNAL
	sig_t original = signal(SIGCHLD, SIG_DFL);

	pid_t pid = grandfork();

	if (pid) // parent, success or failure.
		signal(SIGCHLD, original);
#else
	struct sigaction transient;
	transient.sa_handler = SIG_DFL;
	/* We coud pass .sa_flags = SA_NOCLDSTOP to suppress SIGCHLD in parent when
	 * child gets SIGSTOP/SIGCONT: however, the (child and) grandchild shall
	 * inherit our signal mask and we want the grandchild to have the default,
	 * at least for SIGCHLD.  Since child doesn't live long enough to get much
	 * of a chance to get SIGSTOP/SIGCONT, parent doesn't really care, and child
	 * isn't going to be around to care about SIGCHLD from grand-child, so we
	 * don't really nead it anyway. */
	transient.sa_flags = 0;
	struct sigaction original;
	sigaction(SIGCHLD, &transient, &original);

	pid_t pid = grandfork();

	if (pid) // parent, success or failure.
		sigaction(SIGCHLD, &original, NULL);
#endif

	if (pid + 1) // else we're in parent and failed
	{
		if (pid) // parent, success (as far as we'll be able to tell)
			return OpStatus::OK;

#ifdef POSIX_OK_SETENV
		/* Restore any LD_PRELOAD predating Opera's invocation (wrapper script on
		 * Unix has set LD_PRELOAD suitably for our use of Java; this might upset
		 * our application): */
		if (const char * preload = op_getenv("OPERA_LD_PRELOAD"))
			g_opera->posix_module.m_env.Set("LD_PRELOAD", preload);
#endif // POSIX_OK_SETENV

		int error = 0;
		PosixNativeUtil::CommandSplit split(native.get());
		if (char** args = split.get())
			error = execvp(args[0], args); // ... from which we never return.

		_exit(error); // _exit() omits exit()'s running of atexit hooks.
	}
	return OpStatus::ERR;
}
# endif // EXTERNAL_APPLICATIONS_SUPPORT

# if defined(OPSYSTEMINFO_GETBINARYPATH) && defined(POSIX_HAS_GETBINARYPATH)
OP_STATUS PosixSystemInfo::GetBinaryPath(OpString *path)
{
	OP_ASSERT(path);

	path->Empty();

	OP_STATUS status = OpStatus::ERR;

#ifdef POSIX_USE_CUSTOM_GETBINARYPATH
	const char* program_path = posix_program_path;
	if (program_path)
		status = PosixNativeUtil::FromNative(program_path, path);

	if (OpStatus::IsMemoryError(status) ||
		(OpStatus::IsSuccess(status) && path->HasContent()))
		return status;
#endif // POSIX_USE_CUSTOM_GETBINARYPATH

#ifdef POSIX_USE_PROC_SELF_EXE
	status = PosixFileUtil::ReadLink("/proc/self/exe", path);

	if (OpStatus::IsMemoryError(status) ||
		(OpStatus::IsSuccess(status) && path->HasContent()))
		return status;
#endif // POSIX_USE_PROC_SELF_EXE

	return OpStatus::ERR;
}
# endif // OPSYSTEMINFO_GETBINARYPATH && POSIX_HAS_GETBINARYPATH
#endif // POSIX_OK_PROCESS


#ifdef POSIX_USE_PROC_CPUINFO

void PosixSystemInfo::InitCPUInfo()
{
	if (m_cpu_info_initialized)
		return;
	m_cpu_info_initialized = TRUE;

    OpFile f;
    RETURN_VOID_IF_ERROR(f.Construct(UNI_L("/proc/cpuinfo")));
    RETURN_VOID_IF_ERROR(f.Open(OPFILE_READ));

# ifdef OPSYSTEMINFO_CPU_FEATURES
    int cpu_features = -1;
# endif // OPSYSTEMINFO_CPU_FEATURES
# ifdef OPSYSTEMINFO_CPU_ARCHITECTURE
    CPUArchitecture cpu_architecture = CPU_ARCHITECTURE_UNKNOWN;
    BOOL cpu_architecture_found = FALSE;
# endif // OPSYSTEMINFO_CPU_ARCHITECTURE

    while (!f.Eof())
    {
        OpString8 line;
        RETURN_VOID_IF_ERROR(f.ReadLine(line));

        if (line.HasContent())
        {

            OpData data;
            RETURN_VOID_IF_ERROR(line.AppendToOpData(data));
            OpAutoPtr< OtlCountedList<OpData> > key_value(data.Split(':', 1));
            if (key_value->Count() == 2)
            {
                OpData key(key_value->First());
                key.Strip();

# ifdef OPSYSTEMINFO_CPU_FEATURES
#  ifdef ARCHITECTURE_ARM
                if (key.Equals("Features"))
#  elif defined(ARCHITECTURE_IA32)
                if (key.Equals("flags"))
#  endif // ARCHITECTURE_ARM || ARCHITECTURE_IA32
                {
                    OpAutoPtr< OtlCountedList<OpData> > flags(key_value->Last().Split(' '));
                    if (flags.get())
                    {
                        int cpu_features_tmp = 0;
                        for (OtlCountedList<OpData>::Range r=flags->All(); r; ++r)
                        {
#  ifdef ARCHITECTURE_ARM
                            if      (r->Equals("vfpv3")) cpu_features_tmp |= CPU_FEATURES_ARM_VFPV3;
                            else if (r->Equals("vfp")) cpu_features_tmp |= CPU_FEATURES_ARM_VFPV2;
                            else if (r->Equals("neon"))  cpu_features_tmp |= CPU_FEATURES_ARM_NEON;
#  elif defined(ARCHITECTURE_IA32)
                            if      (r->Equals("sse2"))   cpu_features_tmp |= CPU_FEATURES_IA32_SSE2;
                            else if (r->Equals("sse3"))   cpu_features_tmp |= CPU_FEATURES_IA32_SSE3;
                            else if (r->Equals("ssse3"))  cpu_features_tmp |= CPU_FEATURES_IA32_SSSE3;
                            else if (r->Equals("sse4_1")) cpu_features_tmp |= CPU_FEATURES_IA32_SSE4_1;
                            else if (r->Equals("sse4_2")) cpu_features_tmp |= CPU_FEATURES_IA32_SSE4_2;
#  endif // ARCHITECTURE_ARM || ARCHITECTURE_IA32
                        }

                        if (cpu_features == -1)
                            cpu_features = cpu_features_tmp;
                        else if (cpu_features != cpu_features_tmp)
                            cpu_features &= cpu_features_tmp;
                    }
                }
# endif // OPSYSTEMINFO_CPU_FEATURES

# ifdef OPSYSTEMINFO_CPU_ARCHITECTURE
                if (key.Equals("CPU architecture"))
                {
                    CPUArchitecture architecture_tmp = CPU_ARCHITECTURE_UNKNOWN;
                    long archNum;
                    if (OpStatus::IsSuccess(key_value->Last().ToLong(&archNum)))
                    {
                        if      (archNum == 5) architecture_tmp = CPU_ARCHITECTURE_ARMV5;
                        else if (archNum == 6) architecture_tmp = CPU_ARCHITECTURE_ARMV6;
                        else if (archNum >= 7) architecture_tmp = CPU_ARCHITECTURE_ARMV7;
                    }

                    if (!cpu_architecture_found)
                    {
                        cpu_architecture_found = TRUE;
                        cpu_architecture = architecture_tmp;
                    }
                    else if (cpu_architecture != architecture_tmp)
                    {
                        cpu_architecture = CPU_ARCHITECTURE_UNKNOWN;
                    }
                }
# endif // OPSYSTEMINFO_CPU_ARCHITECTURE
            }
        }
    }

# ifdef OPSYSTEMINFO_CPU_ARCHITECTURE
    m_cpu_architecture = cpu_architecture;
# endif // OPSYSTEMINFO_CPU_ARCHITECTURE
# ifdef OPSYSTEMINFO_CPU_FEATURES
    m_cpu_features = cpu_features == -1 ? 0 : static_cast<unsigned int>(cpu_features);
# endif // OPSYSTEMINFO_CPU_FEATURES
}

#endif // POSIX_USE_PROC_CPUINFO

#endif // POSIX_OK_SYS
