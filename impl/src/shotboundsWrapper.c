#include <Python.h>
#include "shotbounds.h"


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
		item = PyLong_FromLong((long)data[i]);
		PyList_SET_ITEM(pyList, i, item);
	}

	return pyList;
}

PyMethodDef CutDetectMethods[] = {{"getCutsWrapper", getCutsWrapper, METH_VARARGS, "Passing a filename of a videofile, retrieve a list of cuts(framenumber)"}};

PyModuleDef mod = {PyModuleDef_HEAD_INIT, "CutDetect", "CutDetect-Wrapper", 0, &CutDetectMethods, NULL, NULL, NULL, NULL};

PyMODINIT_FUNC PyInit_CutDetect() {
	printf("Test\n");
	(void) PyModule_Create(&mod);
}
