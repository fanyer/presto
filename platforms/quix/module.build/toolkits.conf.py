
@option('--with-gtk2', 'Compile GTK2 toolkit integration library.')
def withGTK2():
    return True

@option('--with-gtk3', 'Compile GTK3 toolkit integration library.')
def withGTK3():
    return config.targetPlatform.isLinux

@option('--with-kde4', 'Compile KDE4 toolkit integration library.')
def withKDE4():
    return True

GTK3PkgConfig = 'pkg-config' # Workaround: the pkg-config tool to use for gtk+-3.0

def libraries(binary='opera'):
    libs = default.libraries
    if binary == 'liboperagtk2.so':
        libs += map(util.PkgConfig, ['gtk+-unix-print-2.0', 'gtk+-2.0'])
    elif binary == 'liboperagtk3.so':
        GTK3PkgConfig = config.GTK3PkgConfig
        libs += map(lambda lib: util.PkgConfig(lib, pkgConfig=GTK3PkgConfig), ['gtk+-unix-print-3.0', 'gtk+-3.0'])
    elif binary == 'liboperakde4.so':
        libs += [config.KDE4()]
        libs += map(util.PkgConfig, ['QtGui', 'QtCore'])
    return libs

class KDE4(util.Library):

    def __init__(self):
        configTool = self.configTool
        self._includePaths = util.runOnce([configTool, '--path', 'include']).strip().split(':')
        self._libraryPaths = util.runOnce([configTool, '--path', 'lib']).strip().split(':')

    def configTool(self):
        return '/usr/local/kde4/bin/kde4-config' if config.targetPlatform.isFreeBSD else 'kde4-config'

    def includePaths(self):
        return self._includePaths

    def libraryPaths(self):
        return self._libraryPaths

    libraryNames = ['kdeui', 'kio', 'kdecore']
