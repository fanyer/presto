
import os
import os.path

@goal('gstreamer-plugins', 'Build GStreamer plugins.')
@flow
def gstreamerPlugins(self):
    binaryDir = config.buildrootBinaries + '/gstreamer'
    tempDir = config.buildrootGStreamer
    self['target'] = tempDir + '/timestamp'
    deps = []
    for top in ('platforms/media_backends/gst/gst-opera', 'platforms/media_backends/gst/include'):
        deps.append(top)
        deps += util.listDirRecursive(top)
    if self < deps:
        plugins = ["{0}/{1}".format(binaryDir, name) for name in config.gstreamerPlugins]
        util.makeDirs(self, binaryDir, tempDir)
        platform = config.hostPlatform
        yield command([platform.gmake] + platform.gmakeFlags +
                      ['-C', 'platforms/media_backends/gst/gst-opera/unix',
                       'GStreamerSourceDir=' + os.path.abspath('platforms/media_backends/gst/gst-opera'),
                       'GStreamerIncludeDir=' + os.path.abspath('platforms/media_backends/gst/include'),
                       'GStreamerTopIncludeDir=' + os.path.abspath('.'),
                       'GStreamerOutputDir=' + os.path.abspath(binaryDir),
                       'GStreamerTempDir=' + os.path.abspath(tempDir),
                       'OPERA_BUILD_DEFINES=' + ' '.join(["{0}={1}".format(key, value) if value else key for key, value in config.defines.iteritems()])] +
                      [os.path.abspath(p) for p in plugins])
        util.touch(self)
        self['result'] = "Produced GStreamer plugins " + ' '.join(plugins)
