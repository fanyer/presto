Documentation available at
https://wiki.oslo.opera.com/developerwiki/Core/projects/clisto

Important note when submitting patch bugs:
No patches will be accepted that regress selftests.
All selftests must pass without failures, leaks, crashes or timeouts.
If changes in the API are done, or the behavior of the API changes
then selftests must be updated to test for the new behavior.
Run the following selftests:
 -test-module=database
 -test=dom.storage*

Last but not least, if you get unexpected failures during
a selftest run, try to delete the web db and web storage
data in the profile (folder pstorage).

