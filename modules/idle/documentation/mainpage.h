/**

@mainpage Idle Module

The <strong>idle module</strong> implements "idle detection" in Core. Via
probe-like objects placed around in the code (OpActivity and OpAutoActivity), it
keeps track of various important acitivies (like reflow, paint and others as
listed in ::OpActivityType), and notifies listeners when we go from a state of
one or more live activities, to a state of no live activities (or vice-versa).

@section api API

Components interested in knowing when we are idle (or when we leave an idle
state) must implement the OpActivityListener interface, and subscribe for the
events using OpIdleDetector::AddListener. The OpIdleDetector singleton is
globally available under the alias \c g_idle_detector.

*/
