#include "core/pch.h"
#include "modules/ecmascript/oppseudothread/oppseudothread.h"

#include <stdio.h>

class MyThread
  : public OpPseudoThread
{
public:
  static void Run(OpPseudoThread *thread)
  {
    for (unsigned i = 0; i < 0x1000000; ++i)
      if ((i & 0xfffff) == 0xfffff)
        {
          static_cast<MyThread *>(thread)->Say("Yielding (%u)...\n", thread->StackSpaceRemaining());
          thread->Yield();
          static_cast<MyThread *>(thread)->Say("Resuming (%u)...\n", thread->StackSpaceRemaining());
        }
  }

private:
  void Say(const char *what, unsigned value)
  {
    this->what = what;
    this->value = value;
    Suspend(DoSay);
  }

  static void DoSay(OpPseudoThread *thread)
  {
    printf(static_cast<MyThread *>(thread)->what, static_cast<MyThread *>(thread)->value);
  }

  const char *what;
  unsigned value;
};

/* This is an external function so that it doesn't get inlined.  And it's a
   function to (try to) make sure that OpPseudoThread::Start() and
   OpPseudoThread::Resume() are called with different stack pointers.  This will
   be the case in practice, and it not being the case in this simple test
   program just hides implementation bugs in OpPseudoThread. */
extern BOOL ResumeThread(OpPseudoThread *);

int main(int argc, char **argv)
{
  printf("Starting...\n");

  MyThread *mt = new MyThread;
  mt->Initialize(1024);
  if (!mt->Start(MyThread::Run))
    do
      printf("Thread yielded...\n");
    while (!ResumeThread(mt));
  printf("Done!\n");

  return 0;
}
