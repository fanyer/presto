
@option('--package', 'Build packages of the specified type (tar, deb, rpm).', metavar='TYPE', replace=True)
def packageTypes():
    types = []
    if config.release:
        for t in config.targetPlatform.packageTypes:
            types.append(t)
            if config.packageOpera:
                types.append(t + ':opera')
            if config.labsName:
                types.append(t + ':opera-labs')
    return types

@option('--package-devel', 'Build a development pseudo-package.')
def packageDevel():
    return not config.release

@option('--package-opera', 'Build Opera packages in addition to Opera Next (with --release).')
def packageOpera():
    return False

@option('--output-packages', 'Directory for generated packages.', metavar='PATH')
def outputPackages():
    return 'packages'

@option('--output-devel', 'Directory for developer pseudo-package.', metavar='PATH')
def outputDevel(pgo=None):
    if pgo == 'generate':
        return config.buildrootPGO + '/run'
    else:
        return 'run'

@option('--labs-name', 'Opera Labs product name, such as "Hardware Acceleration".', metavar='NAME')
def labsName():
    return '' # Must be a string and not None so that the option engine knows the type

@option('--debugger', 'Override the command used to run Opera in the developer wrapper script.', metavar='COMMAND', default='exec gdb -ex run --args')
def develDebugger():
    return '' # Must be a string and not None so that the option engine knows the type

@option('--repack-bundle', 'Produce a repack bundle for rebuilding of packages.')
def repackBundle():
    return config.release

def cleanPatterns():
    return default.cleanPatterns + [config.outputPackages, config.outputDevel]

def packageManifestFiles():
    files = ['adjunct/resources/package.xml', 'platforms/paxage/manifest.xml']
    if config.libPerModule:
        files.append(config.packageLibPerModuleManifest)
    return files

def packageLibPerModuleManifest():
    return config.buildrootSources + '/platforms/paxage/lib-per-module.xml'

def packageFlags(target, pgo=None):
    platform = config.targetPlatform
    flags = [
        '--os', platform.packageOS,
        '--arch', platform.packageArch,
        '--version', config.operaVersion,
        '--build', str(config.operaBuild),
        '--timestamp', target]
    if pgo == 'generate':
        flags += ['--buildroot', config.buildrootPGO]
    flags += ['--buildroot', config.buildroot]
    for f in config.packageManifestFiles:
        flags += ['--manifest', f]
    for t in config.packageTargets:
        flags += ['--target', t]
    labsName = config.labsName
    if labsName:
        flags += ['--labs-name', labsName]
    develDebugger = config.develDebugger
    if develDebugger and pgo != 'generate':
        flags += ['--debugger', develDebugger]
    return flags

def packageTargets():
    targets = []
    if config.withPlugins:
        targets.append('plugins')
        if config.withDualPluginWrapper:
            targets.append('plugins-dual')
    if config.withGTK2:
        targets.append('liboperagtk2')
    if config.withGTK3:
        targets.append('liboperagtk3')
    if config.withKDE4:
        targets.append('liboperakde4')
    if config.withGStreamer:
        targets.append('gstreamer')
    if config.libPerModule:
        targets.append('lib-per-module')
    if config.withAutoUpdateChecker:
        targets.append('opera_autoupdatechecker')
    return targets

class AutoPlatform(default.AutoPlatform):

    def packageOS(self):
        if self.isLinux:
            return 'Linux'
        if self.isFreeBSD:
            return 'FreeBSD'
        assert False, 'Unsupported OS'

    def packageArch(self):
        return 'x86_64' if self.wordsize == 64 else 'i386'

    def packageTypes(self):
        if self.isLinux:
            return ['deb', 'rpm', 'tar']
        if self.isFreeBSD:
            return ['tar']
        assert False, 'Unsupported OS'
