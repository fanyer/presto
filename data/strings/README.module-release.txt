See also https://wiki.oslo.opera.com/developerwiki/Data/strings

Git:

0) (First time only) Make sure your user account on the Git server can access the CVS server:
             ssh <username>@git.oslo.osa "CVSROOT=:pserver:<username>@cvs.oslo.osa/var/cvs/cvsroot cvs login"
                                    (note the _two_ instances of <username> in the above command)

1) Status check:    git status             (make sure that all your changes are committed before pulling)

2) Pull:            git pull [origin]      (pull = fetch + merge; always start out from a up-to-date repository)

3) From scripts folder, run makelang.pl -P to check that there are no syntax errors in english.db

4) Edit english.db and module.about, and update the version number

5) Staging:         git add english.db module.about

6) Commit:          git commit             (commits local changes, use commit -a to stage AND commit)

7) Annotated tag:   git tag -a TAG_NAME

8) Push             git push [origin master]

9) Push the tag:    git push origin tag TAG_NAME


Make a changelog in git:

git log --pretty='format:%h (%an) %s'

If you have defined the alias: git config --global alias.changelog="log --pretty='format:%h (%an) %s'"

then it is sufficient to type:

git changelog

git changelog > changelog.txt  (dumps the changelog to a file called changelog.txt)

The output since the previous module release should be picked up and inserted in a message to

module-versions@opera.com

together with the tag name.

Tag name:

Changelog:


Module information in BTS
-------------------------

In BTS, create a core patch, assigned to strings module owner, and CC translang, with title

"Strings and translations db1022" (or whatever current version is)

Mark "strings" and "translations" as modules

Within this BTS issue, you should make note of all strings required for CORE, linking to them from the menu item "Link this issue to another issue".

- This issue "resolves" a core strings patch. The core strings patch "is resolved" by the module issue.
- This issue "blocks" a patch for core code.  The core code "depends" on the module issue.

When the strings module is released, change the assignee to "Localization team". When translang is finished with the issue, they should reassign to "core-integrators".

For desktop, wait until you have strings and translations, then create a desktop task, and assign it to "desktop-integrator"

Comment by alexr:

The main problem here is that probably only the managers and a few
ex-integrators know about this because we do this rotation based integration.
So when a developer gets the integrator job, he doesn't necessarily know what
to do or that he has to do it. We get the strings and translations when doing
core upgrades, but that is often not enough.

What I suggest that we do is to make a dummy task, eg  "Upgrade strings and
translations to db1042" and make sure that it appears in the list of stuff to
do that the desktop integrator uses:
https://bugs.opera.com/secure/Dashboard.jspa?selectPageId=11181

Anybody can make this task, and we can easily follow the progress.

To get into the integrator queue, 3 things are required:

1) The bug must have priority P1
2) The bug must be resolved
3) The bug must be assigned to desktop-integrator


Obsolete: releasing modules through spartan
-------------------------------------------

http://spartan/modules.php?command=moduleRelease

* Existing module:  strings
  Choose both Core-1 and Core-2
* Version tag:  new-tag-name

The changelog since the previous module release should be picked up and inserted in the changelog field

Check the box marked "Send E-mail notification to modules list"

Check the box marked "Send E-mail notification to specified address:"
and send to the integrator, "Stein Kulseth" <steink@opera.com>

Press button "Add New Release"
