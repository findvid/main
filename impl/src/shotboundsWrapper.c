#include <Python.h>
#include "shotbounds.h"
#include "thumbnails.h"

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

PyObject * getFeaturesWrapper(PyObject *self, PyObject *args) {
	const char * filename;
	char defaultpath[64] = "/video/videosearch/findvid/thumbnails";
	const char * path = defaultpath;

	int vidThumb = 0;

	PyObject * scenesArg;

	if (!PyArg_ParseTuple(args, "(siO|s)", &filename, &vidThumb, &scenesArg, &path)) {
		//ParseTupel should raise Py_TypeError exception on it's own
		//PyErr_SetString(PyExc_TypeError, "Argument mismatch");
		return NULL;
	}

	if (!PyList_Check(sceneList)) {
		PyErr_SetString(PyExc_TypeError, "Second parameter must be a list of numbers!");
		return NULL;	
	}

	//Convert from python list of scene numbers to c array
	PyListObject * scenesList = (PyListObject *)scenesArg;

	Py_ssize_t size = PyList_Size(scenesList);
	uint32_t * scenes = malloc(sizeof(uint32_t) * size);

	for (int i = 0; i < size; i++)
		if (!PyArg_ParseTuple(PyList_GetItem(scenesList,i), "i", &scenes[i])) {
			PyErr_SetString(PyExc_TypeError, "Mismatched scene parameter!");
			return NULL;
		}
	
	FeatureTupel * results = getFeatures(filename, path, vidThumb, scenes,(int)size);

	//Convert struct of features to actual tuple
	PyListObject * pyList = PyList_New(size);
	PyObject * item;
	for (int i = 0; i < size; i++) {

#if PYVERSION == 3
		item = PyLong_FromLong((long)data[i]);
#endif
#if PYVERSION == 2
		item = PyInt_FromLong((long)data[i]);
#endif

		PyList_SET_ITEM(pyList, i, item);
	}

	
}

PyMethodDef FindVidMethods[] = {
	{"getCuts", getCutsWrapper, METH_VARARGS, "Passing a filename of a videofile, retrieve a list of cuts(framenumber)"},
	{"getFeatures", getFeaturesWrapper, METH_VARARGS, "Extract features and thumbnails in a video"}
};

#if PYVERSION == 3
PyModuleDef mod = {PyModuleDef_HEAD_INIT, "CutDetect", "CutDetect-Wrapper", 0, CutDetectMethods, NULL, NULL, NULL, NULL};
#endif

#if PYVERSION == 3
PyMODINIT_FUNC PyInit_FindVid() {
	(void) PyModule_Create(&mod);
}
#endif
#if PYVERSION == 2
PyMODINIT_FUNC initFindVid() {
	(void) Py_InitModule("FindVid", FindVidMethods);
}
#endif
