
@goal('all', 'Build everything (compile, link and package the product).')
@flow
def desktop(self):
    p = package(pgo='use' if config.PGO else None)
    yield p
    if 'result' in p:
        self['result'] = p['result']

@flow
def productSetup(self):
    yield hardcodedSearches()
