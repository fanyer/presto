
import os.path
import shutil

@goal('pgo', 'Compile and run an instrumented build for profile-guided optimization.')
@flow
def PGOData(self):
    self['target'] = config.buildrootPGO + '/data.timestamp'
    pack = packageDevel(pgo='generate')
    yield pack
    script = 'platforms/quix/autobench/autobench.py'
    tests = config.autobenchTestList
    if self < [pack, script, tests]:
        util.makeDirs(self)
        yield command([script] + config.autobenchFlags + ['@' + tests])
        util.touch(self)
