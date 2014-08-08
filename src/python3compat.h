#ifndef PYTHONCOMPAT_H
#define PYTHONCOMPAT_H

#if (PY_VERSION_HEX < 0x03030000)
#define PyUnicode_AsUTF8(x) PyBytes_AsString(PyUnicode_AsUTF8String(x))
#endif

#endif  //PYTHONCOMPAT_H
