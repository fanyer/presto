/** @mainpage Preferences module

This is an auto-generated documentation of the preferences module.
For more information about this module, please refer to the
<a href="http://wiki.oslo.opera.com/developerwiki/index.php/Prefs_module">Wiki</a>
page.

@section api API

@subsection PrefsManager

The main job of the PrefsManager is to have the over-all responsibility for
ensuring that the collection objects (see below) are constructed and
destructed correctly.

PrefsManager also has a method for committing all unsaved changes to disk,
PrefsManager::CommitL().

@subsection PrefsNotifier

The PrefsNotifier is used when code either inside or outside of the core
code wants to notify the preferneces framework that there have been
changes to the system default settings in a specific area.

@subsection opprefscollection OpPrefsCollection and the collections

The collections, which are all subclasses of the OpPrefsCollection class,
are the objects bearing the data.
They will read, interpret and check for validity the data from the
preferences reader object and store it in a more efficient internal storage.
Lookup is fast thanks to almost all preferences being stored in arrays to
which direct lookups are performed, accessed via global pointers to the
collections.

Each collection has its own defined scope, most often referring to a single
code module, a set of related code modules, or a feature definition.
The collections are either implemented in the preferences module, or in
other code modules.

@subsection util Various utilities

The class OperaConfig is used to generate a document for the opera:config
URL.

The class PrefsUpgrade has a single method called PrefsUpgrade::Upgrade(),
which is used to import settings from a previous version of Opera.

@section initdeinit Initialization and de-initalization

To instantiate the PrefsManager and the associated collections, you will
need to first create an PrefsFile instance and send it as a parameter to
the constructor. This object is taken over by PrefsManager.

Constructing the PrefsManager, PrefsManager::ConstructL(), will also create
all the relevant preference collection objects. Similarly, destructing the
PrefsManager will also destruct all the collections.

@author Peter Karlsson <peter@opera.com>
*/
