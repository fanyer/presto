
@option('--with-gstreamer', 'Compile GStreamer plugins.')
def withGStreamer():
    return True

def buildrootGStreamer():
    return config.buildroot + '/gstreamer'

gstreamerPlugins = ['libgstoperamatroska.so', 'libgstoperavp8.so']
