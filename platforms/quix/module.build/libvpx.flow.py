
import os
import os.path

@goal('libvpx', 'Build the libvpx library.')
@flow
def libvpx(self):
    tempDir = config.buildrootLibVPX
    self['target'] = tempDir + '/libvpx.a'
    top = 'platforms/media_backends/libvpx'
    deps = [top]
    deps += util.listDirRecursive(top)
    if self < deps:
        util.makeDirs(self, tempDir)
        platform = config.hostPlatform
        yield command([platform.gmake] + platform.gmakeFlags +
                      ['-C', 'platforms/media_backends/libvpx/build/opera/unix',
                       'VPX_DIR=' + os.path.abspath('platforms/media_backends/libvpx'),
                       'VPX_TMP_DIR=' + os.path.abspath(tempDir)])
        self['result'] = "Produced " + self['target']
