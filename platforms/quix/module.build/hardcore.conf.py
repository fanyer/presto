
class GCC(default.GCC):

    def dumpSystemDefinesCommand(self, target, source):
        return [self.name, '-E', '-dM', '-P', '-o', target, source]
