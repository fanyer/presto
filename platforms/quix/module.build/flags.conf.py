
@option('--release', 'Use defaults for production builds.', value=True)
@option('--debug', 'Use defaults for development builds.', value=False)
def release():
    return False

def debugCode():
    return not config.release

def framePointers():
    return not config.release

def optimize():
    return config.release

def buildMapfiles():
    return config.release
