/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#include "common\common.h"
#include "autoupdatechecker.h"

#ifdef DEBUG
namespace
{
  void test_result_callback(const char* name, bool passed)
  {
    MessageBoxA(NULLPTR, passed ? "PASSED" : "FAILED!", name, MB_OK);
  }
}
#endif // DEBUG

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
  opera_update_checker::OperaAutoupdateChecker checker;
#ifdef DEBUG
  if (strcmp(lpCmdLine, "-test") == 0)
  {
    // Pretent the command line was parsed successfully.
    bool tmp_validated_pipe_id = checker.validated_pipe_id;
    checker.validated_pipe_id = true;
    checker.TestCommandLineParsing(0, NULLPTR, test_result_callback);
    checker.TestGlobalStorage(test_result_callback);
    checker.TestProtocol(test_result_callback);
    checker.TestNetwork(test_result_callback);
    checker.validated_pipe_id = tmp_validated_pipe_id;
  }
  else if (strcmp(lpCmdLine, "-testipcserver") == 0 || strcmp(lpCmdLine, "-testipcclient") == 0)
  {
    bool tmp_validated_pipe_id = checker.validated_pipe_id;
    checker.validated_pipe_id = true;
    checker.TestIPC(test_result_callback, strcmp(lpCmdLine, "-testipcserver") == 0);
    checker.validated_pipe_id = tmp_validated_pipe_id;
  }
  else
#endif // DEBUG
    checker.Execute(__argc, __argv);
}
