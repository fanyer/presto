
import sys

def hardcoreFlags(step):
    flags = [
        '--outroot=' + config.buildrootSources,
        '--make',
        '--timestamp', config.hardcoreTimestamp,
        '--with-linenumbers',
        '-DOPERASETUP'] + config.preprocessorFlags
    if step == 'pch':
        configFile = config.platformProductConfig
        if configFile:
            flags += ['--platform_product_config=' + configFile]
    return flags

def hardcoreTimestamp(step):
    return "{src}/hardcore.{step}".format(src=config.buildrootSources, step=step)

def operaVersion():
    return util.runOnce([sys.executable, 'modules/hardcore/scripts/opera_coreversion.py', '--version=opera'])
