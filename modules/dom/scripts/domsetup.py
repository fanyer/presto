import basesetup
import atoms
import prototypes

class AtomsOperation(basesetup.SetupOperation):
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="DOM atoms")

    def __call__(self, sourceRoot, outputRoot=None, quiet="yes"):
        self.startTiming()
        return self.endTiming(atoms.generate(sourceRoot, outputRoot or sourceRoot), quiet=quiet)

class PrototypesOperation(basesetup.SetupOperation):
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="DOM prototypes")

    def __call__(self, sourceRoot, outputRoot=None, quiet="yes"):
        self.startTiming()
        return self.endTiming(prototypes.generate(sourceRoot, outputRoot or sourceRoot), quiet=quiet)
