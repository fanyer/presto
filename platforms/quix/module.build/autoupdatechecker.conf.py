
@option('--with-autoupdatechecker', 'Compile standalone auto update checker.')
def withAutoUpdateChecker():
    # By default, True on Linux, False on FreeBSD
    return config.targetPlatform.isLinux

def libraries(binary='opera'):
    libs = default.libraries
    if binary == 'opera_autoupdatechecker':
        # Use system libs
        libs.append(util.PkgConfig('sqlite3'))
        libs.append(util.PkgConfig('tinyxml', static=True))
        libs.append(util.PkgConfig('libcurl'))
    return libs

def defines(binary='opera'):
    defines = default.defines
    if binary == 'opera_autoupdatechecker':
        defines['OAUC_PLATFORM_INCLUDES'] = '"platforms/posix/autoupdate_checker/includes/platform.h"'
        defines['TIXML_USE_STL'] = 'YES'
        defines['POSIX_AUTOUPDATECHECKER_IMPLEMENTATION'] = 'YES'
        defines['OAUC_STANDALONE'] = 'YES'
        defines['NO_OPENSSL'] = 'YES'
    return defines

class GCC(default.GCC):
    def codeFlags(self, binary=None):
        flags = super(GCC, self).codeFlags
        if binary == 'opera_autoupdatechecker':
            flags.append("-fexceptions")
        return flags
