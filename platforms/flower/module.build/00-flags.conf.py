
@option('--debug-symbols', 'Produce debug information.')
def debugSymbols():
    return True

@option('--debug-code', 'Compile debugging code such as assertions.')
def debugCode():
    return False

@option('--frame-pointers', 'Compile with frame pointers.')
def framePointers():
    return not config.optimize

@option('--optimize', 'Produce optimized code.')
def optimize():
    return True

@option('--optimize-size', 'Optimize the code for size (only when optimizations are enabled).', value=True)
@option('--optimize-speed', 'Optimize the code for speed (only when optimizations are enabled).', value=False)
def optimizeSize():
    return False

@option('--lto', 'Optimize the whole program at link time (only when optimizations are enabled).')
def LTO():
    return False

@option('--warnings', 'Enable an extended set of compiler warnings.')
def warnings():
    return False

@option('--pike', 'Pike interpreter to use (for selftests).', metavar='PATH')
def pike():
    return 'pike'

@option('--selftests', 'Enable selftests.')
def selftests():
    return False

@option('--jumbo', 'Compile groups of source files into large compilation units to speed up the build process.')
def jumbo():
    return True

@option('--lib-per-module', 'Compile one shared library per module for faster linking.')
def libPerModule():
    return False

@option('--mapfiles', 'Produce map files when linking (slow).')
def buildMapfiles():
    return False
