"""Single-Byte Character Set tables
"""

# Copyright Opera software ASA, 2003-2012.  Written by Eddy.
from tableutils import unhex, NON_UNICODE, byteTable, hexTable

class sbcsTable (byteTable, hexTable):
    __upinit = byteTable.__init__
    def __init__(self, name, emojis):
        self.__upinit(name, emojis)
        self.depend('sbcs.py') # ? __file__ ? __module__ ?
        try: src = { 'macintosh': 'mac-roman',
                     'ibm866': 'cp866' }[name]
        except KeyError:
            if name[:9] == 'iso-8859-':
                if name[9:11] == '11': src = 'cp874'
                else: src = name[4:]
            elif name[:8] == 'windows-': src = 'cp' + name[8:]
            elif name[:6] == 'x-mac-': src = name[2:]
            else: src = name

        self.source(src)

    def writemap(self, out, fwd):
        # over-ridden by viscii
        apply(out.emit, ('128H',) + tuple(fwd[128:256]))

    def build(self, endian, dir, force, want):
        outs, date = self.prepare(dir, want, force,
                                  self.name, self.name + '-rev')
        if date is None:
            # CORE-40886, CORE-43418: By default, map C0, G0 and C1 to
            # [U+0000,U+001F], [U+0020,U+007F] and [U+0080,U+009F].
            fwd = self.load(range(0, 160))

            # Write forward table:
            try: out = outs[self.name]
            except KeyError: pass
            else:
                out.open(endian)
                try: self.writemap(out, fwd)
                finally: out.close()

            try: out = outs[self.name + '-rev']
            except KeyError: pass
            else:
                # Compute reverse table:
                rev, i = [], 0
                for x in fwd:
                    if NON_UNICODE != x >= 128:
                        rev.append((x, i))
                    i = 1 + i

                del fwd

                # Write reverse table:
                self.writepairs(out, endian, rev)

        return outs

del byteTable, hexTable

class visciiTable (sbcsTable):
    def writemap(self, out, fwd):
        apply(out.emit, ('256H',) + tuple(fwd))
    def parse(self, row): return int(row[0]), unhex(row[2])
    __load = sbcsTable.load
    def load(self, fwd=None): return self.__load(range(128))
