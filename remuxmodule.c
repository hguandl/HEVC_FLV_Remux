#define PY_SSIZE_T_CLEAN

#include <Python.h>

#include "remux.h"

static PyObject *remux_remux(PyObject *self, PyObject *args) {
    int ret;
    char *in_filename, *out_filename, *http_headers;

    ret = PyArg_ParseTuple(args, "sss", &in_filename, &out_filename, &http_headers);

    printf("%s\n", in_filename);
    if (!ret) {
        return Py_None;
    }

    ret = remux(in_filename, out_filename, http_headers);

    return PyLong_FromLong(ret);
}

static PyObject *remux_version(PyObject *self, PyObject *args) {
    return PyBytes_FromString("0.1");
}

static PyMethodDef RemuxMethods[] = {
    {"remux", remux_remux, METH_VARARGS,
     "Remux a media from in_filename to out_filename."},

    {"version", remux_version, METH_VARARGS,
     "Return the version of remux."},

    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef remuxmodule = {
    PyModuleDef_HEAD_INIT,
    "remux",
    NULL,
    -1,
    RemuxMethods
};

PyMODINIT_FUNC PyInit_remux(void) {
    return PyModule_Create(&remuxmodule);
}
