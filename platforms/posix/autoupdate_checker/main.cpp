// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2012 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include <cstdio>
#include "adjunct/autoupdate/autoupdate_checker/common/common.h"
#include "adjunct/autoupdate/autoupdate_checker/autoupdatechecker.h"

namespace {
  void test_result_callback(const char* name, bool passed) {
    printf("%s %s\n", name, (passed ? "PASSED" : "FAILED!"));
  }

  bool has_flag(int argc, char* argv[], const char* flag) {
    for (int i = 0; i < argc; ++i) {
    if (strcmp(argv[i], flag) == 0)
        return true;
    }
    return false;
  }
}

using opera_update_checker::OperaAutoupdateChecker;

int main(int argc, char* argv[]) {
  OperaAutoupdateChecker checker;
#ifdef DEBUG
  if (has_flag(argc, argv, "-test")) {
    // Pretent the command line was parsed successfully.
    bool tmp_validated_pipe_id = checker.validated_pipe_id;
    checker.validated_pipe_id = true;
    checker.TestCommandLineParsing(0, NULLPTR, test_result_callback);
    checker.TestGlobalStorage(test_result_callback);
    checker.TestProtocol(test_result_callback);
    checker.TestNetwork(test_result_callback);
  } else if (has_flag(argc, argv, "-testipcserver") ||
             has_flag(argc, argv, "-testipcclient")) {
    bool tmp_validated_pipe_id = checker.validated_pipe_id;
    checker.validated_pipe_id = true;
    checker.TestIPC(test_result_callback,
                    has_flag(argc, argv, "-testipcserver"));
    checker.validated_pipe_id = tmp_validated_pipe_id;
  } else {
#endif  // DEBUG
  return checker.Execute(argc, argv);
#ifdef DEBUG
  }
#endif  // DEBUG
}
