"""Check portable camera-preview plugins without opening a physical camera."""
import ctypes
import pathlib
import sys

stage = pathlib.Path(sys.argv[1]).resolve()
gst = ctypes.CDLL(str(stage / 'usr/lib/libgstreamer-1.0.so.0'), mode=ctypes.RTLD_GLOBAL)
gst.gst_init.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
gst.gst_init.restype = None
gst.gst_init(None, None)
gst.gst_element_factory_make.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
gst.gst_element_factory_make.restype = ctypes.c_void_p
gst.gst_object_unref.argtypes = [ctypes.c_void_p]
gst.gst_object_unref.restype = None
for name in ('v4l2src', 'videotestsrc', 'camerabin', 'wrappercamerabinsrc', 'jpegdec', 'jpegenc',
             'decodebin', 'videoconvert', 'videorate', 'appsink', 'encodebin', 'capsfilter'):
    element = gst.gst_element_factory_make(name.encode(), None)
    if not element:
        raise RuntimeError('Missing portable video element: ' + name)
    gst.gst_object_unref(element)
print('PASS: bundled GStreamer capture, MJPEG, conversion and camera-preview factories')
