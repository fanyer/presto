
@option('--pgo', 'Enable profile-guided optimization.')
def PGO():
    return False

autobenchTestList = 'platforms/quix/autobench/SPARTAN.list'
autobenchTimeout = 300

def autobenchJobs():
    return config.processQuota

def autobenchFlags():
    flags = ['-O', config(pgo='generate').outputDevel + '/opera',
             '-l', config.buildrootPGO + '/pgo.log',
             '--timeout', str(autobenchTimeout),
             '--headless']
    jobs = config.autobenchJobs
    if jobs > 1:
        flags += ['-j', str(jobs)]
    return flags
