/* This user.js file is used to stress test execution order of
   inline scripts.  It will block (a lot, since it calls alert()
   before and after every inline script.
   
   Combined with the inline script tests in 
   
     jstest2/scheduler-regression/index.html
     jstest2/HTML/HTMLScriptElement.html

   these provide a very good test of the inline script mechanism.
   The test is to simply run the above tests and make sure they
   report no errors even with this file (which will cause a lot
   of alerts to be displayed, of course). */

opera.addEventListener("BeforeExternalScript", function (ev) { alert("BeforeExternalScript"); }, false);
opera.addEventListener("BeforeScript", function (ev) { alert("BeforeScript"); }, false);
opera.addEventListener("AfterScript", function (ev) { alert("AfterScript"); }, false);
