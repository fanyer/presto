/**
 * @mainpage Locale module
 *
 * This is an auto-generated documentation of the encodings module.
 * For more information about this module, please refer to the
 * <a href="http://wiki.oslo.opera.com/developerwiki/index.php/Locale_module">Wiki</a>
 * page.
 *
 * @section api API
 *
 * The API as exposed to the core consists of these classes:
 *
 * @li OpLanguageManager
 * @li Str::LocaleString
 *
 * No core code may use any of the numerical equivalents of the
 * Str::LocaleString constants.
 *
 * @section implementation Platform implementations
 *
 * There are three default implementations of the OpLanguageManager
 * interface:
 *
 * - OpPrefsFileLanguageManager is the standard language manager used on
 *   desktop and others, and is using a text-based language file.
 * - OpBinaryFileLanguageManager uses a pre-processed binary file that
 *   is faster to load, but much more difficult to edit manually.
 * - OpDummyLanguageManager is used for rapid prototyping or builds
 *   where no language strings are used.
 *
 * The API can be re-implemented by platforms with specific needs.
 * 
 * @section initdeinit Initialization and de-initialization
 *
 * The global OpLanguageManager instance will be initialized by the
 * locale module if using any of the implementations known by the module.
 * If you are using a platform-specific non-language file implementation of
 * the interface, you will need to create the instance yourself before
 * running Opera::InitL().
 * 
 * @section requirements Requirements
 *
 * The locale module should take care of importing its necessary
 * dependencies using the API system.
 *
 * @author Peter Krefting <peter@opera.com>
 */
