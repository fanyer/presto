#include "platforms/mac/QuickOperaApp/QuickWidgetApp.h"
#include "platforms/mac/QuickOperaApp/QuickWidgetLibHandler.h"
#include "platforms/mac/QuickOperaApp/CocoaQuickWidgetMenu.h"
#include "platforms/mac/QuickOperaApp/QuickCommandConverter.h"

#include "adjunct/quick/managers/LaunchManager.h"

#ifdef AUTO_UPDATE_SUPPORT
#include "adjunct/autoupdate/updater/auupdater.h"
#endif // AUTO_UPDATE_SUPPORT

#include "platforms/mac/QuickOperaApp/CocoaApplication.h"

Boolean Initialize(int argc, const char** argv);
void BuildStandardMenu();

extern QuickWidgetMenu * gAppleQuickMenu;
extern const char* g_crashaction;

#include "platforms/crashlog/crashlog.h"
#include "platforms/mac/util/systemcapabilities.h"

int main(int argc, const char** argv)
{
#ifdef CRASHLOG_DEBUG
	fprintf(stderr, "int main(%d, {", argc);
	for (int i = 0; i<argc; i++) {
		if (i)
			fprintf(stderr, ", ");
		fprintf(stderr, "\"%s\"", argv[i]);
	}
	fprintf(stderr, "});\n");
#endif
	if (argc >= 2 && !strncmp(argv[1], "-autotestmode", 13))
	{
		g_crashaction = "exit";
	}
	if (argc >= 3 && !strncmp(argv[1], "-write_crashlog", 15))
	{
		char crashlog_filename[48];
		unsigned long u1, u2;
		sscanf(argv[2], "%lu %lu", &u1, &u2);
		pid_t pid = u1;
		GpuInfo *gpu_info = (GpuInfo *)u2;
		WriteCrashlog(pid, gpu_info, crashlog_filename, 48);
		if (argc >= 5 && !strncmp(argv[3], "-crashaction", 12) && !strncmp(argv[4], "exit", 4))
			return 0;

		pid_t fork_pid = fork();

		if (!fork_pid)
	 	{
#ifdef CRASHLOG_DEBUG
			fprintf(stderr, "opera [crashlog debug %d]: in restarting child\n", time(NULL));
#endif
			// child process
			// give the crashed instance and the crashlogging instance time to exit
			usleep(10000);

#ifdef CRASHLOG_DEBUG
			fprintf(stderr, "opera [crashlog debug %d]: restarting %s\n", time(NULL), argv[0]);
#endif

			// now we restart Opera
			execl(argv[0], argv[0], "-crashlog", crashlog_filename, (char *)0);
		}

		return 0;
	}

	// Only install the crash handler if this isn't a -crashlog run
	if (argc >= 3 && !strncmp(argv[1], "-crashlog", 9))
	{
		// A restart from a crashlog crash so don't install the crash handler or you will
		// be stuck in an endless launch loop
	}
	else
	{
		BOOL skip = FALSE;
		for (int i = 1; i < argc && !skip; i++)
			skip = !strncmp(argv[i], "-nocrashhandler", 16);
		if (!skip)
			InstallCrashSignalHandler();
	}

	if (argc >= 3 && !strncmp(argv[1], "-multiproc", 10))
	{
		// Load the Opera.Framework
		CFBundleRef bundle = CFBundleGetMainBundle(), opera_framework_bundle = NULL;
		opera_framework_bundle = QuickWidgetLibHandler::LoadOperaFramework(bundle);

		// Call the function in the framework to Create the PosixIpcComponentPlatform
		if (opera_framework_bundle)
		{
			typedef int (*RunComponentP)(const char *token);
			RunComponentP run_component = (RunComponentP)CFBundleGetFunctionPointerForName(opera_framework_bundle, CFSTR("RunComponent"));

			if (run_component)
				run_component(argv[2]);
		}
	}
	else
	{
#ifdef AUTO_UPDATE_SUPPORT
		// Store the command line so if the upgrade is fired at the end it will launch with
		// the same command line which can be passed back to Opera when it restarts
		// Always skip over the application name
		if (argc > 1)
			g_launch_manager->SetOriginalArgs(argc - 1, argv + 1);
#endif

#ifdef AUTO_UPDATE_SUPPORT
		AUUpdater au_updater;
		if (!au_updater.Run())
			return 0;
#endif // AUTO_UPDATE_SUPPORT

		BuildStandardMenu();

		gQuickCommandConverter = new QuickCommandConverter;
		gWidgetLibHandler = new QuickWidgetLibHandler(argc, argv);

		if (gWidgetLibHandler && gWidgetLibHandler->Initialized())
		{
			// Commented out as part of fix for bug 210821, Opera window doesn't activate completely
			//ProcessSerialNumber	psnOpera = {0, kCurrentProcess};
			//::SetFrontProcess(&psnOpera);
			ApplicationActivate();

			if (Initialize(argc, argv))
			{
				ApplicationRun();
			}
		}

		delete gWidgetLibHandler;
		delete gQuickCommandConverter;
		delete gAppleQuickMenu;
	}

#ifdef AUTO_UPDATE_SUPPORT
	// Launch any application waiting to run on exit
	g_launch_manager->LaunchOnExitApplication();
	g_launch_manager->Destroy();
#endif // AUTO_UPDATE_SUPPORT

	return 0;
}

Boolean Initialize(int argc, const char** argv)
{
	if (emBrowserNoErr != gWidgetLibHandler->CallStartupQuickMethod(argc, argv))
		ExitToShell();

	switch (gWidgetLibHandler->CallStartupQuickSecondPhaseMethod(argc, argv))
	{
		case emBrowserNoErr:
			// Nothing to do
		break;

		case emBrowserExitingErr:
			// This is to force a "clean" exit
		return false;

		default:
			ExitToShell();
		break;
	}

	return true;
}
