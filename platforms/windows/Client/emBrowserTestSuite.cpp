//////////////////////////////////////////////////////////////////////////////////////////////
//  Automatic regression tests for the EmBrowser API
//////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#ifdef WIN32
#include <windows.h>	 	
#include <wchar.h>
#include "adjunct/embrowser/embrowser.h"
#else
#include <string.h>
#include <time.h>
#include "embrowser.h"
#endif // WIN32

#ifndef MAX_PATH
#define MAX_PATH 2048
#endif

#ifndef BOOL
#define BOOL bool
#endif

#if defined(TARGET_RT_MAC_CFM) && TARGET_RT_MAC_CFM
#define PATHSEP_CHAR ':'
#else
#define PATHSEP_CHAR '/'
#endif

// Terminate procedure and return error state
#define RETURN_IF_EM_ERROR(x) {EmBrowserStatus ret = x; if(ret != emBrowserNoErr) { ExitModule(); return ret;}}

// Terminate procedure, but do not return error state
#define RETURN_OK_IF_EM_ERROR(x) {EmBrowserStatus ret = x; if(ret != emBrowserNoErr) { ExitModule(); return emBrowserNoErr;}}

// Terminate procedure and return out of memory error
#define RETURN_IF_OOM_ERROR(x) { if(!x) { ExitModule(); return emBrowserOutOfMemoryErr;}}

FILE * g_logf = NULL;
char * g_logfname = NULL;
char * g_module_name   = "general";
int    g_module_errors = 0;
int    g_module_tests  = 0;
int    g_total_errors  = 0;
int    g_total_tests   = 0;
int	   g_part_number   = 0;
int    g_expected_errors = 0;

EmBrowserMethods * g_methods;
EmBrowserRef       g_browser;

void thumbnailCallback (IN void *clientData, IN EmThumbnailStatus status, IN EmThumbnailRef);


void MakeEmBrowserString(EmBrowserChar * buf, int len, const char * str)
{
	while ((--len > 0) && (*buf++ = *str++)) {}
	*buf = 0;
}

int EmBrowserStringCompare(const EmBrowserChar * str1, const EmBrowserChar * str2, int n)
{
	if (n--)
	{
		while (n-- && *str1 == *str2 && *str1 && *str2)
			str1++, str2++;
		return (int) *str1 - (int) *str2;
	}
	return 0;
}

EmBrowserChar *EmBrowserStrChr(const EmBrowserChar * s, EmBrowserChar c)
{
	while (*s && *s != c)
		s++;

	if (*s || !c)
		return (EmBrowserChar *) s;
	else
		return NULL;
}

//
//  FUNCTION: InitTestSuite
//
//  PURPOSE:  Prepare the test suite: open the log file and write head and body
//

EmBrowserStatus InitTestSuite(char * fname, EmBrowserMethods * met, EmBrowserRef bro)
{
    if(!g_logf) g_logf = fopen(fname,"w");

    fprintf(g_logf,"<html><head>\n<title>EmBrowser Test Suite Results</title>\n");

    fprintf(g_logf,"<link rel=\"stylesheet\" type=\"text/css\" href=\"emtestlog1.css\" title=\"emtestlog1\">\n");
    fprintf(g_logf,"<link rel=\"alternate stylesheet\" type=\"text/css\" href=\"emtestlog2.css\" title=\"emtestlog2\">\n");
    fprintf(g_logf,"<link rel=\"alternate stylesheet\" type=\"text/css\" href=\"emtestlog3.css\" title=\"emtestlog3\">\n");

    fprintf(g_logf,"</head>\n<body>\n");

	g_logfname		= fname;
	g_methods		= met;
    g_browser		= bro;
	g_module_name	= "general";

	return emBrowserNoErr;
}

//
//  FUNCTION: InitTable
//
//  PURPOSE:  Prepare the test suite part: write the table header
//

EmBrowserStatus InitTable(int number)
{
	g_part_number		= number;

    fprintf(g_logf,"<h1>EmBrowser Test Suite Part %d Results</h1>\n<table>\n", number);
    fprintf(g_logf,"<tr><th>Message</th><th>Module</th><th>Failed API method</th><th>Tests</th><th>Tests passed</th><th>Errors</th>\n");

	g_module_errors	= 0;
	g_module_tests	= 0;
	g_total_errors	= 0;
	g_total_tests	= 0;
	g_expected_errors = 0;

	return emBrowserNoErr;
}

//
//  FUNCTION: InitModule
//
//  PURPOSE:  Init the module variables
//

void InitModule(char * name)
{
    g_module_name	= name;
    g_module_errors	= 0;
	g_module_tests	= 0;
}

//
//  FUNCTION: ExitModule
//
//  PURPOSE:  Write the current test module result to the log file
//

void ExitModule()
{
	if (g_module_errors)
	{
		int number = 0; // number = minimum number of expected module tests
		if(strncmp(g_module_name, "Screen Port Management", 22) == 0)
			number = 11;
		else if(strncmp(g_module_name, "Visibility", 10) == 0)
			number = 7;
		else if(strncmp(g_module_name, "Selection", 9) == 0)
			number = 2;
		else if(strncmp(g_module_name, "Basic Navigation", 16) == 0)
			number = 3;
		else if(strncmp(g_module_name, "Properties2", 9) == 0)
			number = 2;
		else if(strncmp(g_module_name, "Properties", 10) == 0)
			number = 12;
		else if(strncmp(g_module_name, "Commands", 8) == 0)
			number = 18;
		else if(strncmp(g_module_name, "Miscellaneous2", 14) == 0)
			number = 21;
		else if(strncmp(g_module_name, "Miscellaneous", 13) == 0)
			number = 15;
		else if(strncmp(g_module_name, "Document Handling", 17) == 0)
			number = 9;
		else if(strncmp(g_module_name, "CookiesRegrRerun", 16) == 0)
			number = 6;
		else if(strncmp(g_module_name, "Cookies", 7) == 0)
			number = 17;
		else if(strncmp(g_module_name, "History", 7) == 0)
			number = 14;
		else if(strncmp(g_module_name, "JavaScript", 10) == 0)
			number = 7;
		else if(strncmp(g_module_name, "Document", 8) == 0)
			number = 16;
		else if(strncmp(g_module_name, "Thumbnail", 9) == 0)
			number = 2;
		else if(strncmp(g_module_name, "Plugin", 9) == 0)
			number = 2;
		else if(strncmp(g_module_name, "Image Display", 9) == 0)
			number = 15;
		else if(strncmp(g_module_name, "Visible Links", 9) == 0)
			number = 3;

		if (number == g_module_tests)
			fprintf(g_logf,"<tr><td>Test module failed (all tests are run)</td>");
		else if (number > g_module_tests)
			fprintf(g_logf,"<tr><td>Test module failed (%d tests should have been run)</td>", number);
		else
			fprintf(g_logf,"<tr><td>Test module failed (minimum %d tests should have been run)</td>", number);
	}
	else if (g_module_tests)
		fprintf(g_logf,"<tr><td>Test module completed</td>");

    fprintf(g_logf,"<td>%s</td>", g_module_name);
	fprintf(g_logf,"<td></td>");
	fprintf(g_logf,"<td>%d</td><td>%d</td><td>%d</td></tr>\n", g_module_tests, g_module_tests-g_module_errors, g_module_errors);
}

//
//  FUNCTION: ExitTable
//
//  PURPOSE:  At the end of the test suite part, write the result to the log file
//

EmBrowserStatus ExitTable(int number)
{
	if (g_total_errors > g_expected_errors)
	{
		fprintf(g_logf,"<tr><td>Test Suite Part %d completed with %d expected and %d unexpected errors</td>", number, g_expected_errors, g_total_errors - g_expected_errors);
	}
	else
		fprintf(g_logf,"<tr><td>Test Suite Part %d completed with %d expected errors</td>", number, g_expected_errors);
		
    fprintf(g_logf,"<td></td>");
    fprintf(g_logf,"<td></td>");
    fprintf(g_logf,"<td>%d</td>",g_total_tests);
    fprintf(g_logf,"<td>%d</td>",g_total_tests - g_total_errors);
    fprintf(g_logf,"<td>%d</td></tr>\n",g_total_errors);
	fprintf(g_logf,"</table>\n");

	return emBrowserNoErr;
}

//
//  FUNCTION: ExitTestSuite
//
//  PURPOSE:  At the end of the test suite: close the log file.
//

EmBrowserStatus ExitTestSuite()
{
    if(g_logf) fclose(g_logf);
    g_logf = NULL;

	return emBrowserNoErr;
}

//
//  FUNCTION: TestBrowserState
//
//  PURPOSE:  Check the result value of function. Terminate if error
//

EmBrowserStatus TestBrowserState(EmBrowserStatus state, char * name)
{
    if(state != emBrowserNoErr)
    {
		fprintf(g_logf,"<tr><td>Fatal error. Test suite part %d terminated abnormally</td>", g_part_number);
		fprintf(g_logf,"<td>%s</td>",g_module_name);
        fprintf(g_logf,"<td>\"%s\"</td>", name);
		fprintf(g_logf,"<td>%d</td>",g_total_tests);
		fprintf(g_logf,"<td>%d</td>",g_total_tests - g_total_errors);
		fprintf(g_logf,"<td>%d</td></tr>",g_total_errors);
		fprintf(g_logf,"</table>\n");

        if(g_logf) fclose(g_logf);
        g_logf = NULL;
    }
    return state;
}

//
//  FUNCTION: TestBrowserCall
//
//  PURPOSE:  Check the result value of function and write any error messages in log file
//

EmBrowserStatus TestBrowserCall(EmBrowserStatus state, char * name)
{
	g_total_tests++;
	g_module_tests++;
    if(state != emBrowserNoErr)
    {
        fprintf(g_logf,"<tr><td>Method returned error</td>");
        fprintf(g_logf,"<td>%s</td>",g_module_name);
        fprintf(g_logf,"<td>\"%s\"</td></tr>\n", name);
		g_total_errors ++;
		g_module_errors ++;
    }
    return state;
}

//
//  FUNCTION: TestBrowserImplementation
//
//  PURPOSE:  Check the value of a function pointer.
//

EmBrowserStatus TestBrowserImplementation(void * fpointer, char * name)
{
	g_total_tests++;
	g_module_tests++;
    if(fpointer == NULL)
    {
		fprintf(g_logf,"<tr><td>Method not implemented</td>");
        fprintf(g_logf,"<td>%s</td>",g_module_name);
        fprintf(g_logf,"<td>\"%s\"</td>", name);
        fprintf(g_logf,"</tr>\n");
        g_total_errors ++;
        g_module_errors ++;
        return emBrowserGenericErr;
    }
    return emBrowserNoErr;
}

//
//  FUNCTION: TestError
//
//  PURPOSE:  Check the condition for failed test and write any error messages in log file
//

EmBrowserStatus TestError(BOOL failed, char * message, char * name)
{
	g_total_tests++;
	g_module_tests++;
	if (failed)
	{
		fprintf(g_logf,"<tr><td>Unexpected bug:%s</td>",message);
		fprintf(g_logf,"<td>%s</td>",g_module_name);
		fprintf(g_logf,"<td>\"%s\"</td></tr>\n",name);
		g_total_errors ++;
		g_module_errors ++;
	}
    return emBrowserNoErr;
}

//
//  FUNCTION: TestExpectedError
//
//  PURPOSE:  Check the condition for expected failed test and write messages in log file
//

EmBrowserStatus TestExpectedError(BOOL failed, char * message, char * name, char * expected_bugnumber)
{
	g_total_tests++;
	g_module_tests++;
	if (failed)
	{
		fprintf(g_logf,"<tr><td>Expected bug:%s (%s)</td>",expected_bugnumber,message);
		fprintf(g_logf,"<td>%s</td>",g_module_name);
		fprintf(g_logf,"<td>\"%s\"</td></tr>\n",name);
		g_total_errors++;
		g_module_errors++;
		g_expected_errors++;
	}
	else
	{
		fprintf(g_logf,"<tr><td>Expected bug %s did succeed!(%s)</td>",expected_bugnumber,message);
		fprintf(g_logf,"<td>%s</td>",g_module_name);
		fprintf(g_logf,"<td>\"%s\"</td></tr>\n",name);
		
	}
    return emBrowserNoErr;
}

/* ---------------------------------------------------------------------------- 
 Test suite part 1 test modules
 ---------------------------------------------------------------------------- */

//
//  FUNCTION: TestScreenPort
//
//  PURPOSE:  Test Screen Port Managment.
//

EmBrowserStatus TestScreenPort()
{
    InitModule("Screen Port Management");

    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setLocation,"EmBrowserSetPortLocationProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getLocation,"EmBrowserGetPortLocationProc"));

	EmBrowserRect get_bounds;
	EmBrowserRect set_bounds;
	EmBrowserRect chk_bounds;
			
    RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getLocation(g_browser, &get_bounds ), "EmBrowserGetPortLocationProc"));
	
    int x  = (get_bounds.right - get_bounds.left) / 4;
    int y = (get_bounds.bottom - get_bounds.top) / 4;

    TestError((!x || !y),"Function retrieved improbable values","EmBrowserGetPortLocationProc");
	if (!x || !y)
		return emBrowserNoErr;

	set_bounds.left = get_bounds.left + x;
	set_bounds.top = get_bounds.top + y;
	set_bounds.right = get_bounds.right - x;
	set_bounds.bottom = get_bounds.bottom - y;

    RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setLocation(g_browser, &set_bounds , NULL), "EmBrowserSetPortLocationProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getLocation(g_browser, &chk_bounds ), "EmBrowserGetPortLocationProc"));
    
    TestError((set_bounds.left != chk_bounds.left),"Function set or retrieved wrong left value","EmBrowserG(S)etPortLocationProc");
    TestError((set_bounds.top != chk_bounds.top),"Function set or retrieved wrong top value","EmBrowserG(S)etPortLocationProc");
    TestError((set_bounds.right != chk_bounds.right),"Function set or retrieved wrong right value","EmBrowserG(S)etPortLocationProc");
    TestError((set_bounds.bottom != chk_bounds.bottom),"Function set or retrieved wrong bottom value","EmBrowserG(S)etPortLocationProc");

    // Reset values
    RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setLocation(g_browser, &get_bounds , NULL), "EmBrowserSetPortLocationProc"));

	ExitModule();
	return emBrowserNoErr;
}


//
//  FUNCTION: TestVisibility
//
//  PURPOSE:  Test Reading and writing visibility.
//

EmBrowserStatus TestVisibility()
{
    InitModule("Visibility");

    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setVisible,"EmBrowserSetVisibilityProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getVisible,"EmBrowserGetVisibilityProc"));

	long get_val;
	long set_val;
	long chk_val;
		
    // Retrieve the visibility setting
    get_val = g_methods->getVisible(g_browser);
	
    // Construct new value
	set_val = (get_val == 0);

    // Set new value
    RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setVisible(g_browser, set_val ), "EmBrowserSetVisibilityProc"));

    // Retrieve the visibility setting again
    chk_val = g_methods->getVisible(g_browser);
    
    // Compare value read with value written
    TestError((set_val != chk_val),"Function set or retrieved wrong visibility value","EmBrowserG(S)etVisibilityProc");

    // Construct another value
	set_val = (set_val == 0);

    // Set new value
    RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setVisible(g_browser, set_val ), "EmBrowserSetVisibilityProc"));

    // Retrieve the visibility setting again
    chk_val = g_methods->getVisible(g_browser);
    
    // Compare value read with value written
    TestError((set_val != chk_val),"Function set or retrieved wrong visibility value 2","EmBrowser(G)SetVisibilityProc");

    // Reset value
    RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setVisible(g_browser, get_val ), "EmBrowserSetVisibilityProc"));

	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestSelection
//
//  PURPOSE:  Test Scroll to selection
//

EmBrowserStatus TestSelection()
{
    InitModule("Selection");

    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->scrollToSelection,"EmBrowserScrollToSelectionProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->scrollToSelection(g_browser), "EmBrowserScrollToSelectionProc"));

	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestBasicNavigation
//
//  PURPOSE:  Test Open URL in browser
//

EmBrowserStatus TestBasicNavigation()
{
    InitModule("Basic Navigation");

    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->openURL, "EmBrowserOpenURLProc"));
#ifdef WIN32
	EmBrowserString destURL = L"http://www.opera.com";
#else
	EmBrowserChar destURL[] = {'h','t','t','p',':','/','/','w','w','w','.','o','p','e','r','a','.','c','o','m',0};
#endif // WIN32
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->openURL(g_browser, destURL, emBrowserURLOptionNoCache), "EmBrowserOpenURLProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->openURL(g_browser, destURL, emBrowserURLOptionDefault), "EmBrowserOpenURLProc"));

	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestProperties
//
//  PURPOSE:  Test Get download progress numbers, current load status and security level and get various text strings from the browser
//

EmBrowserStatus TestProperties()
{
    InitModule("Properties");

	// EmBrowserGetDLProgressNums
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->downloadProgressNums, "EmBrowserGetDLProgressNumsProc"));
	long currentRemaining = 0;
	long totalAmount = 0;
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->downloadProgressNums(g_browser, &currentRemaining, &totalAmount), "EmBrowserGetDLProgressNumsProc"));

	// EmBrowserGetPageBusy
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->pageBusy, "EmBrowserGetPageBusyProc"));
	TestError((g_methods->pageBusy(g_browser) < 0),"Method failed","EmBrowserGetPageBusyProc");

	// EmBrowserGetSecurityDescProc
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getSecurityDesc, "EmBrowserGetSecurityDescProcProc"));
	long securityOnOff = 0;
	long bufferSize = 128;
	EmBrowserString securityDescription = new EmBrowserChar[bufferSize];
	RETURN_IF_OOM_ERROR(securityDescription);
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getSecurityDesc(g_browser, &securityOnOff, bufferSize, securityDescription), "EmBrowserGetSecurityDescProc"));
	delete [] securityDescription;

	// EmBrowserGetTextProc
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getText, "EmBrowserGetTextProc"));
    unsigned short textBuffer[128];
	long textLength = 0;
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getText(g_browser, emBrowserURLText, bufferSize, textBuffer, &textLength), "EmBrowserGetTextProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getText(g_browser, emBrowserLoadStatusText, bufferSize, textBuffer, &textLength), "EmBrowserGetTextProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getText(g_browser, emBrowserTitleText, bufferSize, textBuffer, &textLength), "EmBrowserGetTextProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getText(g_browser, emBrowserMouseContextText, bufferSize, textBuffer, &textLength), "EmBrowserGetTextProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getText(g_browser, emBrowserCombinedStatusText, bufferSize, textBuffer, &textLength), "EmBrowserGetTextProc"));

	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestCommands
//
//  PURPOSE:  Test Query Opera to see if a message is currently available and send a command message to the browser
//

EmBrowserStatus TestCommands()
{
    InitModule("Commands");

    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->canHandleMessage, "EmBrowserCanHandleMessageProc"));

	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_copy_cmd) < 0),"emBrowser_msg_copy_cmd failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_paste_cmd) < 0),"emBrowser_msg_paste_cmd failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_cut_cmd) < 0),"emBrowser_msg_cut_cmd failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_clear_cmd) < 0),"emBrowser_msg_clear_cmd failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_select_all_cmd) < 0),"emBrowser_msg_select_all_cmd failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_undo_cmd) < 0),"emBrowser_msg_undo_cmd failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_redo_cmd) < 0),"emBrowser_msg_redo_cmd failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_quit_cmd) < 0),"emBrowser_msg_quit_cmd failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_back) < 0),"emBrowser_msg_back failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_forward) < 0),"emBrowser_msg_forward failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_reload) < 0),"emBrowser_msg_reload failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_stop_loading) < 0),"emBrowser_msg_stop_loading failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_searchpage) < 0),"emBrowser_msg_searchpage failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_has_document) < 0),"emBrowser_msg_has_document failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_clone_window) < 0),"emBrowser_msg_clone_window failed","EmBrowserCanHandleMessageProc");
	TestError((g_methods->canHandleMessage(g_browser,emBrowser_msg_print) < 0),"emBrowser_msg_print failed","EmBrowserCanHandleMessageProc");

    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->handleMessage, "EmBrowserDoHandleMessageProc"));

	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_copy_cmd))
		TestError((g_methods->handleMessage(g_browser,emBrowser_msg_copy_cmd) != emBrowserNoErr),"emBrowser_msg_copy_cmd failed","EmBrowserHandleMessageProc");
	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_paste_cmd))
		TestError((g_methods->handleMessage(g_browser,emBrowser_msg_paste_cmd) != emBrowserNoErr),"emBrowser_msg_paste_cmd failed","EmBrowserHandleMessageProc");
	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_cut_cmd))
		TestError((g_methods->handleMessage(g_browser,emBrowser_msg_cut_cmd) != emBrowserNoErr),"emBrowser_msg_cut_cmd failed","EmBrowserHandleMessageProc");
	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_clear_cmd))
		TestError((g_methods->handleMessage(g_browser,emBrowser_msg_clear_cmd) != emBrowserNoErr),"emBrowser_msg_clear_cmd failed","EmBrowserHandleMessageProc");
	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_select_all_cmd))
		TestError((g_methods->handleMessage(g_browser,emBrowser_msg_select_all_cmd) != emBrowserNoErr),"emBrowser_msg_select_all_cmd failed","EmBrowserHandleMessageProc");
	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_undo_cmd))
		TestError((g_methods->handleMessage(g_browser,emBrowser_msg_undo_cmd) != emBrowserNoErr),"emBrowser_msg_undo_cmd failed","EmBrowserHandleMessageProc");
	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_redo_cmd))
		TestError((g_methods->handleMessage(g_browser,emBrowser_msg_redo_cmd) != emBrowserNoErr),"emBrowser_msg_redo_cmd failed","EmBrowserHandleMessageProc");
	//  emBrowser_msg_quit_cmd is for testing if EmBrowser can be shut down, testing the actual
	//  message doesn't make sense AFAIK (johan@opera.com).
	//	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_quit_cmd))
	//		TestExpectedError((g_methods->handleMessage(g_browser,emBrowser_msg_quit_cmd) != emBrowserNoErr),"emBrowser_msg_quit_cmd failed","EmBrowserHandleMessageProc","135155");
	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_back))
		TestError((g_methods->handleMessage(g_browser,emBrowser_msg_back) != emBrowserNoErr),"emBrowser_msg_back failed","EmBrowserHandleMessageProc");
	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_forward))
		TestError((g_methods->handleMessage(g_browser,emBrowser_msg_forward) != emBrowserNoErr),"emBrowser_msg_forward failed","EmBrowserHandleMessageProc");
	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_reload))
		TestError((g_methods->handleMessage(g_browser,emBrowser_msg_reload) != emBrowserNoErr),"emBrowser_msg_reload failed","EmBrowserHandleMessageProc");
	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_stop_loading))
		TestError((g_methods->handleMessage(g_browser,emBrowser_msg_stop_loading) != emBrowserNoErr),"emBrowser_msg_stop_loading failed","EmBrowserHandleMessageProc");
	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_searchpage))
		TestError((g_methods->handleMessage(g_browser,emBrowser_msg_searchpage) != emBrowserNoErr),"emBrowser_msg_searchpage failed","EmBrowserHandleMessageProc");
	if (g_methods->canHandleMessage(g_browser,emBrowser_msg_has_document))
		TestError((g_methods->handleMessage(g_browser,emBrowser_msg_has_document) != emBrowserNoErr),"emBrowser_msg_has_document failed","EmBrowserHandleMessageProc");
	//if (g_methods->canHandleMessage(g_browser,emBrowser_msg_clone_window))
	//	TestError((g_methods->handleMessage(g_browser,emBrowser_msg_clone_window) != emBrowserNoErr),"emBrowser_msg_clone_window failed","EmBrowserHandleMessageProc");
	//if (g_methods->canHandleMessage(g_browser,emBrowser_msg_print))
	//	TestError((g_methods->handleMessage(g_browser,emBrowser_msg_print) != emBrowserNoErr),"emBrowser_msg_print failed","EmBrowserHandleMessageProc");

	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestMiscellaneous
//
//  PURPOSE:  Test forced widget draw, inform the browser about status and set zoom factor, user interface language and user stylesheet
//

EmBrowserStatus TestMiscellaneous()
{
    InitModule("Miscellaneous");

	// EmBrowserDrawProc
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->draw, "EmBrowserDrawProc"));
	EmBrowserRect updateRegion = {0, 0, 200, 200};
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->draw(g_browser, &updateRegion), "EmBrowserDrawProc"));

	// EmBrowserSetActiveProc
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setActive, "EmBrowserSetActiveProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setActive(g_browser, 1), "EmBrowserSetActiveProc"));

	// EmBrowserSetFocusProc
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setFocus, "EmBrowserSetFocusProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setFocus(g_browser, 0), "EmBrowserSetFocusProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setFocus(g_browser, 1), "EmBrowserSetFocusProc"));

	// EmBrowserSetZoomProc
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setZoom, "EmBrowserSetZoomProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setZoom(g_browser, 50), "EmBrowserSetZoomProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setZoom(g_browser, 150), "EmBrowserSetZoomProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setZoom(g_browser, 100), "EmBrowserSetZoomProc"));

	// EmBrowserSetUILanguageProc, EmBrowserSetUserStylesheetProc
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setUILanguage, "EmBrowserSetUILanguageProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setUserStylesheet, "EmBrowserSetUserStylesheetProc"));

    char file[MAX_PATH];
	unsigned short fileW[MAX_PATH];
	char * tmp = strdup(g_logfname);
    char * pos = strrchr(tmp, PATHSEP_CHAR);
    if(pos) 
		*pos = 0;

	// create and use emtestlog.lng file
    sprintf(file,"%s%cemtestlog.lng",tmp,PATHSEP_CHAR);
	FILE * lngfile = fopen(file,"w"); // create a emtestlog.lng file for test purpose
	if (lngfile)
	{
		fprintf(lngfile,"[Info]\nLanguage=\"test\"\nLanguageName=\"embrowsertest\"\nCharset=\"iso-8859-1\"");
		fclose(lngfile);

		MakeEmBrowserString(fileW, MAX_PATH, file);
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setUILanguage((EmBrowserString)fileW), "EmBrowserSetUILanguage"));
	}

	// create and use emtestlog1.css file
    sprintf(file,"%s%cemtestlog1.css",tmp,PATHSEP_CHAR);
	FILE * cssfile = fopen(file,"w"); // create a test.css file for test purpose
	if (cssfile)
	{
		fprintf(cssfile,"h1{color:maroon;}\n");
		fclose(cssfile);

		MakeEmBrowserString(fileW, MAX_PATH, file);
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setUserStylesheet((EmBrowserString)fileW), "EmBrowserSetUserStylesheetProc"));
	}

	delete [] tmp;

	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestDocumentHandling
//
//  PURPOSE:  Test Get document information
//

EmBrowserStatus TestDocumentHandling()
{
    InitModule("Document Handling");

	// Test method implementation
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getRootDoc, "EmBrowserGetRootDocProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->openURLInDoc, "EmBrowserOpenURLinDocProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getSourceSize, "EmBrowserGetSourceSizeProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getSource, "EmBrowserGetSourceProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getURL, "EmBrowserGetURLProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getNumberOfSubDocuments, "EmBrowserGetNumberOfSubDocumentsProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getSubDocuments, "EmBrowserGetSubDocumentsProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getParentDocument, "EmBrowserGetParentDocumentProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getBrowser, "EmBrowserGetBrowserProc"));

	// EmBrowserGetRootDocProc
	EmDocumentRef document;
	g_methods->getRootDoc(g_browser, &document);
	if (document)
	{
		// EmBrowserOpenURLInDocProc
#ifdef WIN32
		EmBrowserString destURL = L"http://www.test.com";
#else
		EmBrowserChar destURL[] = {'h','t','t','p',':','/','/','w','w','w','.','t','e','s','t','.','c','o','m',0};
#endif // WIN32
		TestError((g_methods->openURLInDoc(document, destURL, emBrowserURLOptionDefault) != emBrowserNoErr),"Method returned with error","EmBrowserOpenURLinDocProc");
		TestError((g_methods->openURLInDoc(document, destURL, emBrowserURLOptionNoCache) != emBrowserNoErr),"Method returned with error","EmBrowserOpenURLinDocProc");

		// EmBrowserGetSourceSizeProc
		long size = 0;
		TestError((g_methods->getSourceSize(document, &size) != emBrowserNoErr),"Method returned with error","EmBrowserGetSourceSizeProc");

		// EmBrowserGetSourceProc
		EmBrowserString textBuffer = new EmBrowserChar[2056];
		RETURN_IF_OOM_ERROR(textBuffer);
		TestError((g_methods->getSource(document, 2056, textBuffer) != emBrowserNoErr),"Method returned with error","EmBrowserGetSourceProc");
		delete [] textBuffer;

		// EmBrowserGetURLProc
		EmBrowserString docURL = new EmBrowserChar[MAX_PATH];
		RETURN_IF_OOM_ERROR(docURL);
		long textLength = 0;
		TestError((g_methods->getURL(document, MAX_PATH, docURL, &textLength) != emBrowserNoErr),"Method returned with error","EmBrowserGetURLProc");
		delete [] docURL;

		// EmBrowserGetNumberOfSubDocumentsProc
		long numSubDocs = 0;
		TestError(((numSubDocs = g_methods->getNumberOfSubDocuments(document)) < 0),"Method returned error","EmBrowserGetNumberOfSubDocumentsProc");

		// Test methods if any subdocuments available
		if (numSubDocs > 0)
		{
			// EmBrowserGetSubDocumentsProc
			EmDocumentRef* docArray = new EmDocumentRef[numSubDocs];
			RETURN_IF_OOM_ERROR(docArray);
			EmBrowserRect* locationArray = new EmBrowserRect[numSubDocs];
			RETURN_IF_OOM_ERROR(locationArray);
			TestError((g_methods->getSubDocuments(document, numSubDocs, docArray, locationArray) != emBrowserNoErr),"Method returned with error","EmBrowserGetSubDocumentsProc");

			// EmBrowserGetParentDocumentProc
			EmDocumentRef parentdocument;
			EmDocumentRef subdoc = docArray[numSubDocs-1];
			TestError((g_methods->getParentDocument(subdoc, &parentdocument) != emBrowserNoErr),"Method returned with error","EmBrowserGetParentDocumentProc");

			delete [] docArray;
			delete [] locationArray;
		}

		// EmBrowserGetParentDocumentProc
		EmDocumentRef parentdocument;
		TestError((g_methods->getParentDocument(document, &parentdocument) != emBrowserNoErr) && parentdocument ,"Method returned with error","EmBrowserGetParentDocumentProc");

		// EmBrowserGetBrowserProc
		EmBrowserRef browser2;
		TestError((g_methods->getBrowser(document, &browser2) != emBrowserNoErr),"Method returned with error","EmBrowserGetBrowserProc");
	}

	ExitModule();
	return emBrowserNoErr;
}

/* ---------------------------------------------------------------------------- 
 Test suite part 2 test modules
 ---------------------------------------------------------------------------- */

//
//  FUNCTION: TestMiscellaneous2
//
//  PURPOSE:  Test search in document, get zoom value, set default encoding, forced encoding and enable/disable sounds
//

EmBrowserStatus TestMiscellaneous2()
{
    InitModule("Miscellaneous2");

	// Test method implementation
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->searchInActiveDocument, "EmBrowserSearchInActiveDocumentProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setZoom, "EmBrowserSetZoomProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getZoom, "EmBrowserGetZoomProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setFallbackEncoding, "EmBrowserSetFallbackEncodingProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->forceEncoding, "EmBrowserForceEncodingProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setSound, "EmBrowserSetSoundProc"));

	// EmBrowserSearchInActiveDocumentProc
#ifdef WIN32
	EmBrowserString pattern = L"opera";
#else
	EmBrowserChar pattern[] = {'o', 'p', 'e', 'r', 'a', 0};
#endif // WIN32
	EmOperaSearchOptions options = emOperaSearchOptionUseDefaults;
	TestError((g_methods->searchInActiveDocument(g_browser, pattern, options) != emBrowserNoErr),"Method returned with error","EmBrowserSearchInActiveDocumentProc");
	options = emOperaSearchOptionCaseSensitive;
	TestError((g_methods->searchInActiveDocument(g_browser, pattern, options) != emBrowserNoErr),"Method returned with error","EmBrowserSearchInActiveDocumentProc");
	options = emOperaSearchOptionEntireWord;
	TestError((g_methods->searchInActiveDocument(g_browser, pattern, options) != emBrowserNoErr),"Method returned with error","EmBrowserSearchInActiveDocumentProc");
	options = emOperaSearchOptionFindNext;
	TestError((g_methods->searchInActiveDocument(g_browser, pattern, options) != emBrowserNoErr),"Method returned with error","EmBrowserSearchInActiveDocumentProc");
	options = emOperaSearchOptionUpwards;
	TestError((g_methods->searchInActiveDocument(g_browser, pattern, options) != emBrowserNoErr),"Method returned with error","EmBrowserSearchInActiveDocumentProc");

	// EmBrowserGetZoomProc, EmBrowserSetZoomProc
	long zoom = 50;
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setZoom(g_browser, zoom), "EmBrowserSetZoomProc"));
	long ret_zoom = 0;
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getZoom(g_browser, &ret_zoom), "EmBrowserGetZoomProc"));
	TestError((ret_zoom != zoom),"Function set or retrieved wrong zoom", "EmBrowserG(S)etZoomProc");
	zoom = 100;
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setZoom(g_browser, zoom), "EmBrowserSetZoomProc"));
	ret_zoom = 0;
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getZoom(g_browser, &ret_zoom), "EmBrowserGetZoomProc"));
	TestError((ret_zoom != zoom),"Function set or retrieved wrong zoom", "EmBrowserG(S)etZoomProc");

	// EmBrowserSetFallbackEncodingProc
#ifdef WIN32
	EmBrowserString encoding = L"windows-1252";
#else
	EmBrowserChar encoding[] = {'w','i','n','d','o','w','s','-','1','2','5','2',0};
#endif // WIN32
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setFallbackEncoding(encoding), "EmBrowserSetFallbackEncodingProc"));

	// EmBrowserForceEncodingProc
//	encoding = L"windows-1252";
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->forceEncoding(g_browser, encoding), "EmBrowserForceEncodingProc"));

	// EmBrowserSetSoundProc
	long progSound = 0;
	long pageSound = 0;
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setSound(progSound, pageSound), "EmBrowserSetSoundProc"));
	progSound = 1;
	pageSound = 1;
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setSound(progSound, pageSound), "EmBrowserSetSoundProc"));

	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestCookies
//
//  PURPOSE:  Test cookie handling
//

EmBrowserStatus TestCookies()
{
    InitModule("Cookies");

	// Test method implementation
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getCookieCount, "EmBrowserGetCookieCountProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getCookieList, "EmBrowserGetCookieListProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getCookieName, "EmBrowserGetCookieNameProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getCookieValue, "EmBrowserGetCookieValueProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getCookieExpiry, "EmBrowserGetCookieExpiryProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getCookieDomain, "EmBrowserGetCookieDomainProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getCookiePath, "EmBrowserGetCookiePathProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getCookieReceivedDomain, "EmBrowserGetCookieReceivedDomainProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getCookieReceivedPath, "EmBrowserGetCookieReceivedPathProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getCookieSecurity, "EmBrowserGetCookieSecurityProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setCookieRequest, "EmBrowserSetCookieRequestProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setCookieValue, "EmBrowserSetCookieValueProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->removeCookies, "EmBrowserRemoveCookiesProc"));

	// EmBrowserGetCookieCountProc
	long size = 0;
	TestError(((size = g_methods->getCookieCount(NULL, NULL)) < 0), "Method returned error value 1", "EmBrowserGetCookieCountProc");

	// EmBrowserGetCookieListProc
    EmCookieRef* cookieArray = new EmCookieRef [size];
	RETURN_IF_OOM_ERROR(cookieArray);
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getCookieList(NULL, NULL, cookieArray), "EmBrowserGetCookieListProc"));

	// go through all existing cookies to check browser calls:
	// EmBrowserGetCookieNameProc, EmBrowserGetCookieValueProc, EmBrowserGetCookieExpiryProc, EmBrowserGetCookieDomainProc
	// EmBrowserGetCookiePathProc, EmBrowserGetCookieReceivedDomainProc, EmBrowserGetCookieReceivedPathProc, EmBrowserGetCookieSecurityProc
	unsigned short strdomain[128] = {0};
	unsigned short strpath[128] = {0};
	long domain_cookies_count = 0;

	for(int i=0;i<size;i++)
	{
		unsigned short str[128] = {0};
		long str_size, safety = 0;
		time_t expiry;

		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getCookieName(cookieArray[i], 128, str, &str_size), "EmBrowserGetCookieNameProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getCookieValue(cookieArray[i], 128, str, &str_size), "EmBrowserGetCookieValueProc"));
		TestError(((expiry = g_methods->getCookieExpiry(cookieArray[i])) < 0), "Method returned error value", "EmBrowserGetCookieExpiryProc");
#ifdef WIN32
		unsigned short* tmp = _wctime(&expiry);
#else
		char* tmp = ctime(&expiry);
#endif //WIN32
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getCookieReceivedDomain(cookieArray[i], 128, str, &str_size), "EmBrowserGetCookieReceivedDomainProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getCookieReceivedPath(cookieArray[i], 128, str, &str_size), "EmBrowserGetCookieReceivedPathProc"));
		TestError(((safety = g_methods->getCookieSecurity(cookieArray[i])) < 0), "Method returned error value", "EmBrowserGetCookieSecurityProc");

		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getCookieDomain(cookieArray[i], 128, strdomain, &str_size), "EmBrowserGetCookieDomainProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getCookiePath(cookieArray[i], 128, strpath, &str_size), "EmBrowserGetCookiePathProc"));
		TestError(((domain_cookies_count = g_methods->getCookieCount(strdomain, strpath)) <= 0), "Method returned error value 2", "EmBrowserGetCookieCountProc");
	}

	// Test setting cookie and cookie value, and removing cookie on current site to check browser calls:
	// EmBrowserSetCookieRequestProc, EmBrowserSetCookieValueProc, EmBrowserRemoveCookiesProc

	// remove specified domain cookie(s), if any
	if (domain_cookies_count)
	{
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->removeCookies(strdomain, strpath, NULL), "EmBrowserRemoveCookiesProc"));
		TestError(((domain_cookies_count = g_methods->getCookieCount(strdomain, strpath)) != 0), "Method returned error value 3", "EmBrowserGetCookieCountProc");
	}

	EmDocumentRef document;
	g_methods->getRootDoc(g_browser, &document);
	if (document)
	{
		EmBrowserChar url_str[128];
		EmBrowserString domain = NULL;
		EmBrowserString path = NULL;
        EmBrowserString endp   = NULL;
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getURL(document, 128, url_str, NULL), "EmBrowserGetURLProc"));
#ifdef WIN32
	EmBrowserString protocol = L"http://";
#else
		EmBrowserChar protocol[] = {'h','t','t','p',':','/','/',0};
#endif // WIN32
		if(EmBrowserStringCompare(url_str, protocol, 7) == 0)
		{
			domain = url_str + 7;  
			if(domain) 
				path = EmBrowserStrChr(domain, '/');
			if(path)
			{
				*path = 0;
				path += 1;
				endp = wcsrchr(path,'/');
			}
			if(endp) *endp = 0;
			if (path && domain)
			{
				TestError(((size = g_methods->getCookieCount(domain, path)) < 0), "Method returned error value 2", "EmBrowserGetCookieCountProc");
				if (size)
				{
					// Change the value of the last cookie
					unsigned short str[128] = {0};
					long str_size = 0;
#ifdef WIN32
					EmBrowserString test_value = L"Test Value";
#else
					EmBrowserChar test_value[] = {'T','e','s','t',' ','V','a','l','u','e',0};
#endif // WIN32
					RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieValue(domain, path, cookieArray[size-1], test_value), "EmBrowserSetCookieValueProc"));

					// Remove page cookies
					RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->removeCookies(domain, path, NULL), "EmBrowserRemoveCookiesProc"));
					long new_size = 0;
					TestError(((new_size = g_methods->getCookieCount(domain, path)) >= size), "Method returned error value 3", "EmBrowserGetCookieCountProc");
				}
				// Set cookie request
#ifdef WIN32
				EmBrowserString test = L"test";
#else
				EmBrowserChar test []= {'t','e','s','t',0};
#endif // WIN32
				RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(domain, path, test), "EmBrowserSetCookieRequestProc"));
			}
		}
	}

	// Remove all cookies
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->removeCookies(NULL, NULL, NULL), "EmBrowserRemoveCookiesProc"));
	TestError((g_methods->getCookieCount(NULL, NULL) > 0),"EmBrowserRemoveCookiesProc did not remove all cookies","EmBrowserGetCookieCountProc");

	delete [] cookieArray;

	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestCookiesRegrRerun
//
//  PURPOSE:  Test fixed cookie bugs based on rerun and no deletion of cookies
//

EmBrowserStatus TestCookiesRegrRerun()
{
    InitModule("CookiesRegrRerun");

	// regression test for bugs #141105, 140935, 170352 and 170022

#ifdef WIN32
	EmBrowserString testdomainlocal = L"localhost";
	EmBrowserString testpathlocal = L"";
	EmBrowserString testvaluelocal = L"name=test";

	EmBrowserString testdomain1 = L"www.foo.bar";
	EmBrowserString testpath1 = L"/b/";
	EmBrowserString testpath2 = L"b/";
	EmBrowserString testpath3 = L"/";
	EmBrowserString testvalue1 = L"name=b1";
	EmBrowserString testvalue2 = L"name=b2";
	EmBrowserString testvalue3 = L"name=b3";
	EmBrowserString testvalue4 = L"name=b4";
	EmBrowserString testvalue5 = L"name=b5";

	EmBrowserString testdomain10 = L"people.opera.com";
	EmBrowserString testdomain11 = L".opera.com";
	EmBrowserString testpath10 = L"/eiriks/";
	EmBrowserString testpath11 = L"eiriks/";
	EmBrowserString testvalue10 = L"name=c1";
	EmBrowserString testvalue11 = L"name=c2";
	EmBrowserString testvalue12 = L"name=c3";
	EmBrowserString testvalue13 = L"name=c4";

#else
	EmBrowserChar testdomainlocal []= {'l','o','c','a','l','h','o','s','t',0};
	EmBrowserChar testpathlocal []= {0};
	EmBrowserChar testvaluelocal []= {'n','a','m','e','=','t','e','s','t',0};

	EmBrowserChar testdomain1 []= {'w','w','w','.','f','o','o','.','b','a','r',0};
	EmBrowserChar testpath1 []= {'/','b','/',0};
	EmBrowserChar testpath2 []= {'b','/',0};
	EmBrowserChar testpath3 []= {'/',0};
	EmBrowserChar testvalue1 []= {'n','a','m','e','=','b','1',0};
	EmBrowserChar testvalue2 []= {'n','a','m','e','=','b','2',0};
	EmBrowserChar testvalue3 []= {'n','a','m','e','=','b','3',0};
	EmBrowserChar testvalue4 []= {'n','a','m','e','=','b','4',0};
	EmBrowserChar testvalue5 []= {'n','a','m','e','=','b','5',0};

	EmBrowserChar testdomain10 []= {'p','e','o','p','l','e','.','o','p','e','r','a','.','c','o','m',0};
	EmBrowserChar testdomain11 []= {'.','o','p','e','r','a','.','c','o','m',0};
	EmBrowserChar testpath10 []= {'/','e','i','r','i','k','s','/',0};
	EmBrowserChar testpath11 []= {'e','i','r','i','k','s','/',0};
	EmBrowserChar testvalue10 []= {'n','a','m','e','=','c','1',0};
	EmBrowserChar testvalue11 []= {'n','a','m','e','=','c','2',0};
	EmBrowserChar testvalue12 []= {'n','a','m','e','=','c','3',0};
	EmBrowserChar testvalue13 []= {'n','a','m','e','=','c','4',0};
#endif // WIN32

	unsigned short strdomain[128] = {0};
	unsigned short strpath[128] = {0};
	unsigned short strvalue[128] = {0};
	long size, str_size, domain_cookies_count = 0;

	TestError(((size = g_methods->getCookieCount(testdomainlocal, NULL)) < 0), "Method returned error value 1", "EmBrowserGetCookieCountProc");
	if (size == 0)
	{
		// localhost.local cookie should be created
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomainlocal, testpathlocal, testvaluelocal), "EmBrowserSetCookieRequestProc"));

		// only one cookie should be created: domain=www.foo.bar; path=b/; name=b3
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain1, testpath1, testvalue1), "EmBrowserSetCookieRequestProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain1, testpath1, testvalue2), "EmBrowserSetCookieRequestProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain1, testpath2, testvalue2), "EmBrowserSetCookieRequestProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain1, testpath2, testvalue3), "EmBrowserSetCookieRequestProc"));

		// one cookie should be created: domain=www.foo.bar; path=/; name=b4
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain1, testpath3, testvalue4), "EmBrowserSetCookieRequestProc"));

		// error: no cookie should be created: domain=NULL; path=/; name=b5
		TestError((g_methods->setCookieRequest(NULL, testpath3, testvalue5) != emBrowserParameterErr),"Method did not return error (NULL domain)","EmBrowserSetCookieRequestProc");

		// error: no cookie should be created: domain=www.foo.bar; path=NULL; name=b5
		TestError((g_methods->setCookieRequest(testdomain1, NULL, testvalue5) != emBrowserParameterErr),"Method did not return error (NULL path)","EmBrowserSetCookieRequestProc");

		// only one cookie should be created: domain=people.opera.com; path=eiriks/; name=c3
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain10, testpath10, testvalue10), "EmBrowserSetCookieRequestProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain10, testpath10, testvalue11), "EmBrowserSetCookieRequestProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain10, testpath11, testvalue11), "EmBrowserSetCookieRequestProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain10, testpath11, testvalue12), "EmBrowserSetCookieRequestProc"));

		// only one cookie should be created: domain=.opera.com; path=eiriks/; name=c4
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain11, testpath10, testvalue10), "EmBrowserSetCookieRequestProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain11, testpath10, testvalue11), "EmBrowserSetCookieRequestProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain11, testpath11, testvalue11), "EmBrowserSetCookieRequestProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain11, testpath11, testvalue12), "EmBrowserSetCookieRequestProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setCookieRequest(testdomain11, testpath11, testvalue13), "EmBrowserSetCookieRequestProc"));
	}
	else
	{ 	// go through existing cookies to check browser calls
		TestError(((size = g_methods->getCookieCount(NULL, NULL)) < 0), "Method returned error value 2", "EmBrowserGetCookieCountProc");
		EmCookieRef* cookieArray = new EmCookieRef [size];
		RETURN_IF_OOM_ERROR(cookieArray);
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getCookieList(NULL, NULL, cookieArray), "EmBrowserGetCookieListProc"));
		for(int i=0;i<size;i++)
		{
			RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getCookieReceivedDomain(cookieArray[i], 128, strdomain, &str_size), "EmBrowserGetCookieReceivedDomainProc"));
			RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getCookieReceivedPath(cookieArray[i], 128, strpath, &str_size), "EmBrowserGetCookieReceivedPathProc"));
			RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getCookieDomain(cookieArray[i], 128, strdomain, &str_size), "EmBrowserGetCookieDomainProc"));
			RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getCookiePath(cookieArray[i], 128, strpath, &str_size), "EmBrowserGetCookiePathProc"));
			RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getCookieValue(cookieArray[i], 128, strvalue, &str_size), "EmBrowserGetCookieValueProc"));
		}
		delete [] cookieArray;
		// remove localhost.local cookie
		TestError(((domain_cookies_count = g_methods->getCookieCount(testdomainlocal, NULL)) != 1), "Method returned value other than 1 (local test)", "EmBrowserGetCookieCountProc");
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->removeCookies(testdomainlocal, NULL, NULL), "EmBrowserRemoveCookiesProc"));
		TestError(((domain_cookies_count = g_methods->getCookieCount(testdomainlocal, NULL)) != 0), "Method returned value other than 0 (local test)", "EmBrowserGetCookieCountProc");
		// remove slash test cookie
		TestError(((domain_cookies_count = g_methods->getCookieCount(testdomain1, NULL)) != 2), "Method returned value other than 2 (slash test 1)", "EmBrowserGetCookieCountProc");
		TestError(((domain_cookies_count = g_methods->getCookieCount(testdomain1, testpath2)) != 2), "Method returned value other than 2 (slash test 2)", "EmBrowserGetCookieCountProc");
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->removeCookies(testdomain1, NULL, NULL), "EmBrowserRemoveCookiesProc"));
		TestError(((domain_cookies_count = g_methods->getCookieCount(testdomain1, NULL)) != 0), "Method returned value other than 0 (slash test)", "EmBrowserGetCookieCountProc");
		// remove slash test cookie for people.opera.com
		TestError(((domain_cookies_count = g_methods->getCookieCount(testdomain10, NULL)) != 2), "Method returned value other than 2 (people.opera.com)", "EmBrowserGetCookieCountProc");
		TestError(((domain_cookies_count = g_methods->getCookieCount(testdomain10, testpath11)) != 2), "Method returned value other than 2 (people.opera.com/eiriks)", "EmBrowserGetCookieCountProc");
		TestError(((domain_cookies_count = g_methods->getCookieCount(testdomain11, NULL)) != 2), "Method returned value other than 2 (.opera.com)", "EmBrowserGetCookieCountProc");
		TestError(((domain_cookies_count = g_methods->getCookieCount(testdomain11, testpath11)) != 2), "Method returned value other than 2 (.opera.com/eiriks)", "EmBrowserGetCookieCountProc");
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->removeCookies(testdomain10, NULL, NULL), "EmBrowserRemoveCookiesProc"));
		TestError(((domain_cookies_count = g_methods->getCookieCount(testdomain10, NULL)) != 1), "Method returned value other than 1 (removed people.opera.com cookies)", "EmBrowserGetCookieCountProc");
		// remove slash test cookie for .opera.com
		TestError(((domain_cookies_count = g_methods->getCookieCount(testdomain11, NULL)) != 1), "Method returned value other than 1 (.opera.com)", "EmBrowserGetCookieCountProc");
		TestError(((domain_cookies_count = g_methods->getCookieCount(testdomain11, testpath11)) != 1), "Method returned value other than 1 (.opera.com/eiriks)", "EmBrowserGetCookieCountProc");
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->removeCookies(testdomain11, NULL, NULL), "EmBrowserRemoveCookiesProc"));
		TestError(((domain_cookies_count = g_methods->getCookieCount(testdomain11, NULL)) != 0), "Method returned value other than 0 (removed .opera.com cookies)", "EmBrowserGetCookieCountProc");
	}
	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestHistory
//
//  PURPOSE:  Test history handling
//

EmBrowserStatus TestHistory()
{
    InitModule("History");

	// Test method implementation
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getHistoryList, "EmBrowserGetHistoryListProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getHistorySize, "EmBrowserGetHistorySizeProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getHistoryIndex, "EmBrowserGetHistoryIndexProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setHistoryIndex, "EmBrowserSetHistoryIndexProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->removeHistoryItem, "EmBrowserRemoveHistoryItemProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->insertHistoryItem, "EmBrowserInsertHistoryItemProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->appendHistoryItem, "EmBrowserAppendHistoryItemProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getHistoryItemTitle, "EmBrowserGetHistoryItemTitleProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getHistoryItemURL, "EmBrowserGetHistoryItemURLProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->removeAllHistoryItems, "EmBrowserRemoveAllHistoryItemsProc"));

	// EmBrowserGetHistorySizeProc, EmBrowserGetHistoryIndexProc

	long size = 0;
	TestError(((size = g_methods->getHistorySize(g_browser)) < 0), "Method returned error value", "EmBrowserGetHistorySizeProc");
	long index = g_methods->getHistoryIndex(g_browser);
	TestError((index >= size), "Method returned index out of bounds", "EmBrowserGetHistoryIndexProc");

	// the following tests require an existing history item
	if (size && (index >= 0) && (index < size))
	{
		// EmBrowserGetHistoryListProc
		EmHistoryRef * historyArray = new EmHistoryRef [size];
		RETURN_IF_OOM_ERROR(historyArray);
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getHistoryList(g_browser, historyArray), "EmBrowserGetHistoryListProc"));

		// EmBrowserSetHistoryIndexProc: set the current history item to the last item of the history list, then reset it
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setHistoryIndex(g_browser, size-1), "EmBrowserSetHistoryListProc"));
		TestError((g_methods->getHistoryIndex(g_browser) != (size-1)), "Method returned error value", "EmBrowserG(S)etHistoryIndexProc");
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setHistoryIndex(g_browser, index), "EmBrowserSetHistoryListProc"));
		TestError((g_methods->getHistoryIndex(g_browser) != index), "Method returned error value", "EmBrowserG(S)etHistoryIndexProc");

		// EmBrowserRemoveHistoryItemProc: remove current item
		EmHistoryRef currentItem = historyArray[index];
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->removeHistoryItem(g_browser, index), "EmBrowserRemoveHistoryItemProc"));
		TestError(((g_methods->getHistorySize(g_browser)) != (size-1)), "EmBrowserGetHistorySizeProc returned unexpected value after history item removal", "EmBrowserRemoveHistoryItemProc");

		// EmBrowserInsertHistoryItemProc: insert removed item
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->insertHistoryItem(g_browser, index, currentItem), "EmBrowserInsertHistoryItemProc"));
		TestError(((g_methods->getHistorySize(g_browser)) != size), "EmBrowserGetHistorySizeProc returned unexpected value after history item insertion", "EmBrowserInsertHistoryItemProc");

		// EmBrowserAppendHistoryItemProc: remove and append copy of current item
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->removeHistoryItem(g_browser, index), "EmBrowserRemoveHistoryItemProc"));
		TestError(((g_methods->getHistorySize(g_browser)) != (size-1)), "EmBrowserGetHistorySizeProc returned unexpected value after history item removal", "EmBrowserRemoveHistoryItemProc");
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->appendHistoryItem(g_browser, currentItem), "EmBrowserAppendHistoryItemProc"));
		TestError(((g_methods->getHistorySize(g_browser)) != size), "EmBrowserGetHistorySizeProc returned unexpected value after history item appendation", "EmBrowserInsertHistoryItemProc");

		// EmBrowserGetHistoryItemTitleProc
        unsigned short title[128];
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getHistoryItemTitle(historyArray[index], 128, title, NULL), "EmBrowserGetHistoryItemTitleProc"));

        unsigned short url[128];
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getHistoryItemURL(historyArray[index], 128, url, NULL), "EmBrowserGetHistoryItemURLProc"));

		// EmBrowserGetHistoryItemURLProc implementation is missing
		delete [] historyArray;
	}

	// EmBrowserRemoveAllHistoryItemsProc
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->removeAllHistoryItems(g_browser), "EmBrowserRemoveAllHistoryItemsProc"));
	TestError(((size = g_methods->getHistorySize(g_browser)) != 0), "Method returned error value", "EmBrowserRemoveAllHistoryItemsProc");

	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestJavascript
//
//  PURPOSE:  Test Javascript handling
//

EmBrowserStatus TestJavaScript()
{
    InitModule("JavaScript");

	// Test method implementation
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->jsEvaluateCode, "EmBrowserJSEvaluateCodeProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->jsGetProperty, "EmBrowserJSGetPropertyProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->jsSetProperty, "EmBrowserJSSetPropertyProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->jsCallMethod, "EmBrowserJSCallMethodProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->jsProtectObject, "EmBrowserJSProtectObjectProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->jsUnprotectObject, "EmBrowserJSUnprotectObjectProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->jsCreateMethod, "EmBrowserJSCreateMethodProc"));
	
	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestDocument
//
//  PURPOSE:  Test Document
//

EmBrowserStatus TestDocument()
{
    InitModule("Document");

	// Test method implementation
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setBrowserStylesheet, "EmBrowserSetBrowserStylesheetProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getAltStylesheetCount, "EmBrowserGetAltStylesheetCountProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getAltStylesheetTitle, "EmBrowserGetAltStylesheetTitleProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->enableAltStylesheet, "EmBrowserEnableAltStylesheetProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getContentSize, "EmBrowserGetContentSizeProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getScrollPos, "EmBrowserGetScrollPosition"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setScrollPos, "EmBrowserSetScrollPosition"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getDocumentTitle, "EmBrowserGetDocumentTitleProc"));

	// EmBrowserSetBrowserStylesheetProc
    char file[MAX_PATH];
	unsigned short fileW[MAX_PATH];
	char * tmp = strdup(g_logfname);
    char * pos = strrchr(tmp, PATHSEP_CHAR);
    if(pos) 
		*pos = 0;

    sprintf(file,"%s%cemtestlog2.css",tmp,PATHSEP_CHAR);
	FILE * cssfile = fopen(file,"w"); // create a css file for test purpose
	if (cssfile)
	{
		fprintf(cssfile,"th{color:maroon;}\n");
		fclose(cssfile);

		MakeEmBrowserString(fileW, MAX_PATH, file);
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setBrowserStylesheet((EmBrowserString)fileW), "EmBrowserSetBrowserStylesheetProc"));
	}

    sprintf(file,"%s%cemtestlog3.css",tmp,PATHSEP_CHAR);
	cssfile = fopen(file,"w"); // create another css file for test purpose
	if (cssfile)
	{
		fprintf(cssfile,"th{color:blue;}\n");
		fclose(cssfile);

		MakeEmBrowserString(fileW, MAX_PATH, file);
	}

	// EmBrowserGetAltStylesheetCountProc
	long ac = 0;
	TestError(((ac = g_methods->getAltStylesheetCount(g_browser)) < 0), "Method returned error value", "EmBrowserGetAltStylesheetCountProc");

	if (ac > 0)
	{
		// EmBrowserGetAltStylesheetTitleProc
		unsigned short title[128];
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getAltStylesheetTitle(g_browser, ac-1, 128, title, NULL), "EmBrowserGetAltStylesheetTitleProc"));

		//EmBrowserEnableAltStylesheetProc
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->enableAltStylesheet(g_browser, title), "EmBrowserEnableAltStylesheetProc"));
	}

	// EmBrowserGetContentSizeProc
	long width = 0;
	long height = 0;
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getContentSize(g_browser, &width, &height), "EmBrowserGetContentSizeProc"));

	// EmBrowserGetScrollPositionProc, EmBrowserSetScrollPositionProc
	long x = 0;
	long y = 0;
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getScrollPos(g_browser, &x, &y), "EmBrowserGetScrollPositionProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setScrollPos(g_browser, 200, 200), "EmBrowserSetScrollPositionProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getScrollPos(g_browser, &x, &y), "EmBrowserGetScrollPositionProc"));

	// EmBrowserGetDocumentTitleProc
	EmDocumentRef document;
	g_methods->getRootDoc(g_browser, &document);
	if (document)
	{
		EmBrowserChar title[128];
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getDocumentTitle(document, 128, title, NULL), "EmBrowserGetDocumentTitleProc"));
	}

	delete [] tmp;
	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestThumbnail
//
//  PURPOSE:  Test Thumbnail handling
//

EmBrowserStatus TestThumbnail()
{
    InitModule("Thumbnail");

	// Test method implementation
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->thumbnailRequest, "EmBrowserThumbnailRequestProc"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->thumbnailKill, "EmBrowserThumbnailKillProc"));

	// EmBrowserThumbnailRequestProc, EmBrowserThumbnailKillProc
	EmDocumentRef document;
	g_methods->getRootDoc(g_browser, &document);
	if (document)
	{
		EmThumbnailRef ref;
		unsigned short url_str[128];
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getURL(document, 128, url_str, NULL), "EmBrowserGetURLProc"));
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->thumbnailRequest(url_str, 1000, 800, 20, thumbnailCallback, &ref), "EmBrowserThumbnailRequestProc"))
		RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->thumbnailKill(ref), "EmBrowserThumbnailKillProc"));
	}
	
	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestPlugin
//
//  PURPOSE:  Test Usage of new plugin path in addition to already existing paths
//

EmBrowserStatus TestPlugin()
{
#ifdef WIN32
	EmBrowserString new_plugin_path = L"D:/Program Files/Opera73/Program/Plugins";
#else
	EmBrowserChar new_plugin_path[] = {'D',':','/','P','r','o','g','r','a','m',' ','F','i','l','e','s','/','O','p','e','r','a','7','3','/','P','r','o','g','r','a','m','/','P','l','u','g','i','n','s',0};
#endif // WIN32

    InitModule("Plugin");

    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->usePluginPath, "EmBrowserUsePluginPathProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->usePluginPath(g_browser, new_plugin_path), "EmBrowserUsePluginPathProc")); 
	
	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestProperties
//
//  PURPOSE:  Test Get upload progress numbers
//

EmBrowserStatus TestProperties2()
{
    InitModule("Properties2");

    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->uploadProgress, "EmBrowserGetULProgressProc"));

	long loadedBytes = 0;
	long totalAmount = 0;
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->uploadProgress(g_browser, &loadedBytes, &totalAmount), "EmBrowserGetULProgressProc")); 
	
	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestImageDisplay
//
//  PURPOSE:  Test display of images
//

EmBrowserStatus TestImageDisplay()
{
    InitModule("Image Display");

	// Test method implementation
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setImageDisplayMode, "EmBrowserSetImageDisplay"));
    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->getImageDisplayMode, "EmBrowserGetImageDisplay"));

	// EmBrowserSetImageDisplay, EmBrowserGetImageDisplay
	EmBrowserImageDisplayMode orig_mode;
	EmBrowserImageDisplayMode new_mode;

	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getImageDisplayMode(g_browser, &orig_mode), "EmBrowserGetImageDisplay"));

	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setImageDisplayMode(g_browser, emBrowserImageDisplayNone), "EmBrowserSetImageDisplay")); 
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getImageDisplayMode(g_browser, &new_mode), "EmBrowserGetImageDisplay"));
	TestError((new_mode != emBrowserImageDisplayNone), "Function set or retrieved wrong mode emBrowserImageDisplayNone", "EmBrowserG(S)etImageDisplay");

	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setImageDisplayMode(g_browser, emBrowserImageDisplayAll), "EmBrowserSetImageDisplay")); 
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getImageDisplayMode(g_browser, &new_mode), "EmBrowserGetImageDisplay"));
	TestError((new_mode != emBrowserImageDisplayAll), "Function set or retrieved wrong mode emBrowserImageDisplayAll", "EmBrowserG(S)etImageDisplay");

	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setImageDisplayMode(g_browser, emBrowserImageDisplayLoaded), "EmBrowserSetImageDisplay")); 
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getImageDisplayMode(g_browser, &new_mode), "EmBrowserGetImageDisplay"));
	TestError((new_mode != emBrowserImageDisplayLoaded), "Function set or retrieved wrong mode emBrowserImageDisplayLoaded", "EmBrowserG(S)etImageDisplay");
	
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setImageDisplayMode(g_browser, orig_mode), "EmBrowserSetImageDisplay")); 
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->getImageDisplayMode(g_browser, &new_mode), "EmBrowserGetImageDisplay"));
	TestError((new_mode != orig_mode), "Function set or retrieved wrong original mode", "EmBrowserG(S)etImageDisplay");

	ExitModule();
	return emBrowserNoErr;
}

//
//  FUNCTION: TestVisibleLinks
//
//  PURPOSE:  Test Changing visibility of visited links
//

EmBrowserStatus TestVisibleLinks()
{
    InitModule("Visible Links");

    RETURN_OK_IF_EM_ERROR(TestBrowserImplementation((void *)g_methods->setVisLinks, "EmBrowserSetVisibilityProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setVisLinks(0), "EmBrowserSetVisibilityProc"));
	RETURN_OK_IF_EM_ERROR(TestBrowserCall(g_methods->setVisLinks(1), "EmBrowserSetVisibilityProc"));

	ExitModule();
	return emBrowserNoErr;
}

/* ---------------------------------------------------------------------------- 
 Test suite parts
 ---------------------------------------------------------------------------- */

//
//  FUNCTION: TestSuitePart
//
//  PURPOSE:  Run a subset of the API functions. Check result. Report anomalies.
//

EmBrowserStatus TestSuitePart(int part)
{
    RETURN_IF_EM_ERROR(TestBrowserState(InitTable(part),"InitTable"));

	switch (part)
	{
	case 1:
		{ 
			RETURN_IF_EM_ERROR(TestBrowserState(TestScreenPort(),"TestScreenPort"));
			RETURN_IF_EM_ERROR(TestBrowserState(TestVisibility(),"TestVisibility"));
			RETURN_IF_EM_ERROR(TestBrowserState(TestSelection(),"TestSelection")); 
			RETURN_IF_EM_ERROR(TestBrowserState(TestProperties(),"TestProperties"));
			RETURN_IF_EM_ERROR(TestBrowserState(TestCommands(),"TestCommands")); 
			RETURN_IF_EM_ERROR(TestBrowserState(TestMiscellaneous(),"TestMiscellaneous"));
			RETURN_IF_EM_ERROR(TestBrowserState(TestDocumentHandling(),"TestDocumentHandling"));
			RETURN_IF_EM_ERROR(TestBrowserState(TestBasicNavigation(),"TestBasicNavigation"));
			break;
		}
	case 2:
		{ 
			RETURN_IF_EM_ERROR(TestBrowserState(TestMiscellaneous2(),"TestMiscellaneous2")); 
			BOOL regr_test_need_rerun = FALSE; // change this value to true in order to run cookie regression tests
			if (regr_test_need_rerun)
			{
				RETURN_IF_EM_ERROR(TestBrowserState(TestCookiesRegrRerun(),"TestCookiesRegrRerun"));
			}
			else
			{
				RETURN_IF_EM_ERROR(TestBrowserState(TestCookies(),"TestCookies"));
			}
			RETURN_IF_EM_ERROR(TestBrowserState(TestJavaScript(),"TestJavaScript"));
			RETURN_IF_EM_ERROR(TestBrowserState(TestDocument(),"TestDocument"));
			RETURN_IF_EM_ERROR(TestBrowserState(TestThumbnail(),"TestThumbnail"));
			RETURN_IF_EM_ERROR(TestBrowserState(TestHistory(),"TestHistory")); 
			RETURN_IF_EM_ERROR(TestBrowserState(TestPlugin(),"TestPlugin"));
			RETURN_IF_EM_ERROR(TestBrowserState(TestProperties2(),"TestProperties2"));
			RETURN_IF_EM_ERROR(TestBrowserState(TestImageDisplay(),"TestImageDisplay"));
			RETURN_IF_EM_ERROR(TestBrowserState(TestVisibleLinks(),"TestVisibleLinks")); 
			break;
		}
	default:
			return emBrowserGenericErr;
	}
	RETURN_IF_EM_ERROR(TestBrowserState(ExitTable(part),"ExitTable"));

	return emBrowserNoErr;
}
