
@option('--with-plugins', 'Compile operapluginwrapper.')
def withPlugins():
    return True

@option('--with-dual-plugin-wrapper', 'Compile operapluginwrapper separately for ia32-linux and the native platform.')
def withDualPluginWrapper():
    platform = config.targetPlatform
    return platform.isLinux and platform.wordsize != 32

def defines(binary='opera'):
    defines = default.defines
    if binary.startswith('operapluginwrapper-'):
        defines['PLUGIN_WRAPPER'] = None
    return defines

def libraries(binary='opera'):
    libs = default.libraries
    if binary.startswith('operapluginwrapper-'):
        libs += map(util.PkgConfig, ['x11'])
        libs += [util.PkgConfig('gtk+-2.0', link=False)]
        libs += map(util.StandardLibrary, ['pthread', 'rt'])
        if config.targetPlatform.isLinux: # dlopen/dlsym/dlclose are in libc on FreeBSD
            libs += [util.StandardLibrary('dl')]
    return libs

class GCC(default.GCC):

    def codeFlags(self, binary=None):
        flags = super(GCC, self).codeFlags
        if binary == 'operapluginwrapper-ia32-linux':
            flags.append('-m32')
        return flags
