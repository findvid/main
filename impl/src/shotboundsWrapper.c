#include <Python.h>
#include "shotbounds.h"


#define PYVERSION 2

PyObject *getCutsWrapper(PyObject *self, PyObject *args) {
	const char * filename;
	PyObject *pyList;
	PyObject *item;

	if (!PyArg_ParseTuple(args, "s", &filename)) {
		return NULL;
	}

	uint32_t *data;
	int size = processVideo(filename, &data);

	pyList = PyList_New(size);
	for (int i = 0; i < size; i++) {
#if PYVERSION == 3
		item = PyLong_FromLong((long)data[i]);
#endif
#if PYVERSION == 2
		item = PyInt_FromLong((long)data[i]);
#endif
		PyList_SET_ITEM(pyList, i, item);
	}

	return pyList;
}

PyMethodDef CutDetectMethods[] = {{"getCutsWrapper", getCutsWrapper, METH_VARARGS, "Passing a filename of a videofile, retrieve a list of cuts(framenumber)"}};

#if PYVERSION == 3
PyModuleDef mod = {PyModuleDef_HEAD_INIT, "CutDetect", "CutDetect-Wrapper", 0, CutDetectMethods, NULL, NULL, NULL, NULL};
#endif

#if PYVERSION == 3
PyMODINIT_FUNC PyInit_CutDetect() {
	(void) PyModule_Create(&mod);
}
#endif
#if PYVERSION == 2
PyMODINIT_FUNC initCutDetect() {
	(void) Py_InitModule("CutDetect", CutDetectMethods);
}
#endif
