
@flow(1, binary='opera_autoupdatechecker')
def precompiledHeader(self, stem, binary, lang):
    self['target'] = ""
    # Disabling precompiled header in this target
