from distutils.core import setup, Extension

module1 = Extension('signer',
                    define_macros = [('MAJOR_VERSION', '1'),
                                     ('MINOR_VERSION', '0')],
                    libraries = ['crypto'],
                     sources = ['signer.cpp'])

setup (name = 'signer',
       version = '1.0',
       description = 'This is signature generator for textfiles',
       author = 'Yngve N. Petterse, Opera Software ASA',
       author_email = 'yngve@opera.com',
       url = 'http://www.opera.com',
       long_description = 'This is signature generator for textfiles',
       ext_modules = [module1])
