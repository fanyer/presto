/**
 * @mainpage Encodings module
 *
 * This is an auto-generated documentation of the encodings module.
 * For more information about this module, please refer to the
 * <a href="http://wiki.oslo.opera.com/developerwiki/index.php/Encodings_module">Wiki</a>
 * page.
 *
 * @section api API
 * @subsection convert Character conversion
 *
 * The most important task of the encodings module is to provide support
 * for converting from and to legacy character encodings.
 *
 * @li InputConverter::CreateCharConverter()
 * @li OutputConverter::CreateCharConverter()
 * @li CharConverter (see also InputConverter and OutputConverter)
 *
 * @subsection detect Encoding detection
 *
 * There is support, to a certain degree, for automatically detecting the
 * encoding with which a document is written.
 *
 * @li CharsetDetector
 *
 * @subsection tables Loading binary data tables
 *
 * The encodings module provides an interface for loading binary data tables
 * from an outside source, normally a file called encoding.bin.
 *
 * @li OpTableManager (the API and abstract interface)
 * @li FileTableManager (the default implementation)
 * @li RomTableManager (implementation using hardcoded tables)
 *
 * @subsection charset Encoding tags
 *
 * IANA has standardized "charset" labels that names the encodings used on
 * the Internet. There are several compatibility aliases for this, so the
 * module provides an interface to convert these into the preferred form,
 * which is used by the other APIs. There is also support for MIB enums.
 *
 * @li CharsetManager
 *
 * @section requirements Requirements
 *
 * If you are using the table-driven conversion support, you need to create
 * a <var>encoding.bin</var> (or similar) for you platform to use. The
 * <a href="http://wiki.oslo.opera.com/developerwiki/index.php/Data/i18ndata">i18ndata</a>
 * module contains the necessary source files for this.
 *
 * @author Peter Karlsson <peter@opera.com>
 */
