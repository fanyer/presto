
def defines():
    defines = default.defines
    defines['STASHED_DEFINES'] = None # see operasetup.py --generate_stashed_defines
    defines['_LARGEFILE_SOURCE'] = None
    defines['WCHAR_IS_UNICHAR'] = None
    defines['_REENTRANT'] = None
    if config.debugCode:
        defines['DEBUG'] = 1
        defines['_DEBUG'] = None
    else:
        defines['NDEBUG'] = None
    return defines

def buildSharedLibrary(binary, mainBinary=False):
    return binary.endswith('.so') or (binary == 'opera' and config.libPerModule and not mainBinary)

def binaryTarget(binary, module=None):
    if module:
        return "{bin}/{name}-{m}.so".format(bin=config.buildrootBinaries, name=binary, m=module.replace('/', '-'))
    else:
        return "{bin}/{name}".format(bin=config.buildrootBinaries, name=binary)

def binaryMapFile():
    return config.binaryTarget + '.map'
