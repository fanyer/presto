/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#ifdef DEBUG
#include <stdio.h>
#endif //DEBUG

#include "adjunct/autoupdate/autoupdate_checker/autoupdatechecker.h"

using namespace opera_update_checker;
using namespace opera_update_checker::ipc;

#include <ApplicationServices/ApplicationServices.h>

#ifdef DEBUG
namespace
{
  void test_result_callback(const char* name, bool passed)
  {
    printf("TEST: %s, RESULT: %s\n", name, passed == 0 ? "failed" : "passed");
  }
}
#endif //DEBUG

int main(int argc, char* argv[])
{
  opera_update_checker::OperaAutoupdateChecker checker;
#ifdef DEBUG
  if (argc > 1 && argv[1] != NULL && strcmp(argv[1], "-test") == 0)
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
  else if (argc > 1 && argv[1] != NULL && (strcmp(argv[1], "-testipcserver") == 0 || strcmp(argv[1], "-testipcclient") == 0))
  {
    bool tmp_validated_pipe_id = checker.validated_pipe_id;
    checker.validated_pipe_id = true;
    checker.TestIPC(test_result_callback, strcmp(argv[1], "-testipcserver") == 0);
    checker.validated_pipe_id = tmp_validated_pipe_id;
  }
  else
#endif //DEBUG
    checker.Execute(argc, argv);

  return 0;
}
