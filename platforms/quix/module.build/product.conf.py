
helpDescription = 'Build Opera Desktop.'

goal = 'all'

platformProductConfig = 'platforms/unix/product/product_config.h'

versionHeader = 'adjunct/quick/quick-version.h'

def operaVersion():
    version = config.targetPlatform.preprocessor.expandMacro(config.versionHeader, 'VER_NUM_STR')
    assert len(version) > 2 and version[0] == '"' and version[-1] == '"'
    return version[1:-1]

@option('--buildno', 'Product build number.')
def operaBuild():
    return 9999

def defines(binary='opera'):
    defines = default.defines
    defines['UNIX'] = None
    defines['FEATURE_SELFTEST'] = 'YES' if config.selftests else 'NO'
    defines['BROWSER_BUILD_NUMBER'] = '"' + str(config.operaBuild) + '"'
    defines['BROWSER_BUILD_NUMBER_INT'] = str(config.operaBuild)
    return defines

def libraries(binary='opera'):
    libs = default.libraries
    if binary == 'opera':
        libs += map(util.PkgConfig, ['freetype2', 'fontconfig', 'sm', 'ice', 'xext', 'xrender'])
        libs += map(util.StandardLibrary, ['pthread', 'rt'])
        if config.targetPlatform.isLinux: # dlopen/dlsym/dlclose are in libc on FreeBSD
            libs += [util.StandardLibrary('dl')]
    return libs
