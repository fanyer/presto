/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** @mainpage applicationcache module
 *
 * Handles offline application caches (html 5 specification).
 *
 * To access, use the global g_application_cache_manager.
 *
 * This API should mainly be accessed from document code or network code.
 * The public parts of the api are in WindowCommander, and
 * WindowCommanderManager.
 *
 * @section Public API Basic overview:
 *
 * This is a very simplified overview. Read
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/offline.html#offline
 * for a more accurate description.
 *
 * Offline application support makes it possible to run
 * web applications when not connected to the Internet. All resources
 * for a web application is downloaded into cache, and a reload
 * of those resources will only load from cache.
 *
 * The caches are maintained in a data structure consisting of
 * application cache groups, and application caches.
 *
 * A application cache group is identified by a manifest url. The manifest url
 * is given by the manifest attribute in the html tag in the document that
 * contains the application using the cache. The document with an manifest
 * attribute is called a "master document", and the manifest identifies
 * which cache the application is using.
 *
 * The master document can be opened in several tabs/frames at the same time,
 * and each tab is identified by the "cache host", which is a pointer to the
 * DOM_Environment for that frame.
 *
 * The application cache group has a list of application caches, which
 * are different versions of the cache. Only the most recent version
 * of the cache is persistent between opera sessions.
 *
 * @section The update algorithm:
 * When the parser code sees the manifest attribute, the HandleCacheManifest
 * function is called. This triggers an update algorithm that downloads the
 * manifest,  creates a new cache, and downloads the urls listed in the
 * manifest into the cache. If another master having the same manifest attribute
 * is opened while the update algorithm is running, the master is added as a
 * pending master. Thus the update algorithm is not re-triggered.
 *
 * When all the urls are downloaded, the cache is set to be complete, and
 * the cache hosts for the pending masters are associated with the cache.
 * From now on, all urls (except for urls specified in the network section of
 * the manifest) will be loaded from cache only. If a url is not listed in
 * the cache, a loading error will happen.
 *
 * The only way to update a application cache is to make changes to the manifest.
 *
 * When reloading the master document from cache, then update algorithm will
 * download the manifest. If the manifest has changed, a new cache will
 * be created and added as a new version of the cache in the cache group.
 * The urls in the manifest will then be downloaded into the new cache, or
 * copied from the previous cache if it has not changed on server.
 *
 * The old cache will still be used until the application calls
 * applicationCache.swapCache(), or the user reloads the master document.
 *
 *
 * @section Persistent storage and data structures
 *
 * The urls are stored persistently in the cache. When stored they are never
 * reloaded from network.
 *
 * The application caches are stored in the
 * [OPFILE_HOME_FOLDER]/applicationcache/ directory as
 * url contexts.
 *
 * The cache structure and list of which manifests and masters
 * belonging to each cache is saved in the XML file
 * cache_groups.xml
 *
 * ApplicationCacheManager owns and controls the ApplicationCacheGroup objects,
 * which again controls the different versions of each application cache.
 *
 * ApplicationCacheManager maintains several hash tables for quick lookup of
 * caches and cache groups, using master url, manifest urls, cache hosts or
 * context IDs.
 */
