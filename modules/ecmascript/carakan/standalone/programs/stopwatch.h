/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * @author Daniel Spang
 */

#ifndef STOPWATCH_H
#define STOPWATCH_H

#ifdef MSWIN
typedef LARGE_INTEGER TimeVal;
#elif UNIX
typedef struct timeval TimeVal;
#endif // UNIX

class StopWatch
{
public:
    void Start();
    void Stop();
    double GetElapsedTime();
private:
    TimeVal start, stop;
    void GetTime(TimeVal *t);
    double GetTimeDiff(TimeVal t1, TimeVal t2);
};

#endif // STOPWATCH_H
