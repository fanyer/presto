
import os
import atexit

@goal('run', 'Compile and run the product.')
@flow
def run(self):
    yield packageDevel(pgo='use' if config.PGO else None)
    cmd = config.outputDevel + '/opera'
    @atexit.register
    def chain():
        sys.stdout.flush()
        sys.stderr.flush()
        os.execl(cmd, cmd)

@flow
def package(self, pgo):
    # These cannot be run in parallel because some packaging tools
    # like tar cannot handle it
    result = []
    if config.packageDevel:
        p = packageDevel(pgo=pgo)
        yield p
        if 'result' in p:
            result.append(p['result'])
    if config.packageTypes:
        p = packageOS(pgo=pgo)
        yield p
        if 'result' in p:
            result.append(p['result'])
    if result:
        self['result'] = '; '.join(result)

@flow
def packageOS(self, pgo):
    packageTypes = sorted(config.packageTypes)
    self['target'] = config.outputPackages + '/timestamp'
    yield packageSetup(pgo=pgo)
    if self < util.readDepFile(self['target'] + '.d'):
        flags = config.packageFlags + ['--output', config.outputPackages]
        if config.repackBundle:
            flags += ['--repack-bundle']
        yield command(['platforms/paxage/package'] + flags + packageTypes)
        self['result'] = "Produced OS packages in {0}/".format(config.outputPackages)

@flow
def packageDevel(self, pgo):
    self['target'] = config.outputDevel + '/timestamp'
    yield packageSetup(pgo=pgo)
    if self < util.readDepFile(self['target'] + '.d'):
        flags = config.packageFlags + ['--output-devel', config.outputDevel]
        yield command(['platforms/paxage/package'] + flags + ['devel'])
        self['result'] = "Produced development pseudo-package in {0}/".format(config.outputDevel)

@flow
def packageSetup(self, pgo):
    binaries = ['opera']
    if config.withGTK2:
        binaries.append('liboperagtk2.so')
    if config.withGTK3:
        binaries.append('liboperagtk3.so')
    if config.withKDE4:
        binaries.append('liboperakde4.so')
    if config.withPlugins:
        binaries.append('operapluginwrapper-native')
        if config.withDualPluginWrapper:
            binaries += ['operapluginwrapper-ia32-linux']
    if config.withAutoUpdateChecker:
        binaries.append('opera_autoupdatechecker')
    deps = [linkBinary(binary=b, pgo=pgo) for b in binaries] + [languageFile(language=language) for language in config.languages] + [encodingBin()]
    if config.withGStreamer:
        deps.append(gstreamerPlugins())
    if config.libPerModule:
        deps.append(packageLibPerModuleManifest())
    yield deps

@flow
def packageLibPerModuleManifest(self):
    self['target'] = config.packageLibPerModuleManifest
    scan = scanSources(binary='opera')
    yield scan
    util.writeIfModified(self['target'],
                         '<?xml version="1.1" encoding="UTF-8"?>\n<xml><opfolder id="UNIX_LIB"><source path=".">\n'
                         + '\n'.join(['<file name="opera-{module}.so" from="bin/opera-{module}.so" mode="so" scandeps="true" />'.format(
                             module=m.replace('/', '-')) for m in scan['byModule']])
                         + '</source></opfolder></xml>\n')
