/**
 * @mainpage Console module
 *
 * This is an auto-generated documentation of the console module.
 * For more information about this module, please refer to the
 * <a href="http://wiki.oslo.opera.com/developerwiki/index.php/Modules/console">Wiki</a>
 * page.
 *
 * @section api API
 *
 * The API as exposed to the core consists of these classes:
 *
 * @li ConsoleModule is the module handler used by the initialization
 *                   system.
 * @li OpConsoleEngine is the central component that stores the actual
 *                     messages.
 * @li OpConsoleView listens to console messages and propagates them to the
 *                   UI console.
 * @li OpConsoleViewHandler is the API to be implemented by the UI console.
 * @li OpConsoleLogger allows for logging console messages to a file.
 * @li OpConsoleFilter describes a display filter for the view and logger.
 * @li OpConsoleListener is the API for classes that wish to listen to
 *                       console messages.
 *
 * @section internal Internal classes
 *
 * There are also some classes only used internally.
 *
 * @li OpConsolePrefsHelper sets up known logger objects and keeps track
 *                          of the associated preferences.
 *
 * @author Peter Karlsson <peter@opera.com>
 */
