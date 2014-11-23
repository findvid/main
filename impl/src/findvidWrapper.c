#include <Python.h>
#include "shotbounds.h"
#include "feature_extraction.h"

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

	if (!PyArg_ParseTuple(args, "siO|s", &filename, &vidThumb, &scenesArg, &path)) {
		//ParseTupel should raise Py_TypeError exception on it's own
		//PyErr_SetString(PyExc_TypeError, "Argument mismatch");
		return NULL;
	}

	if (!PyList_Check(scenesArg)) {
		PyErr_SetString(PyExc_TypeError, "Second parameter must be a list of numbers!");
		return NULL;	
	}

	//Convert from python list of scene numbers to c array
	//PyListObject * scenesList = (PyListObject *)scenesArg;

	Py_ssize_t size = PyList_Size(scenesArg);
	uint32_t * scenes = malloc(sizeof(uint32_t) * size);

	for (int i = 0; i < size; i++) {
		PyObject * itemI = PyList_GetItem(scenesArg, i);
		if (!PyArg_Parse(itemI, "i", &scenes[i])) {
			char err[128];
			sprintf(err, "Mismatched scene parameter #%d!", i);
			PyErr_SetString(PyExc_TypeError, err);
			return NULL;
		}
	}
	
	FeatureTuple * results = getFeatures(filename, path, vidThumb, scenes,(int)size);
	printf("Building tuples from results...\n");
	//Convert struct of features to actual tuple
	PyObject * pyList = PyList_New(size);
	PyObject * item;
	//For each scene, build a tuple of all feature vectors
	for (int i = 0; i < results->feature_count; i++) {
		printf("Iteration for tuple#%d\n", i);
		//Build a list for each feature vector at index i
		PyObject * f1 = PyList_New((Py_ssize_t)results->feature_length[0]);
		for(int j = 0; j < results->feature_length[0]; j++) {
		#if PYVERSION == 3
			item = PyLong_FromLong((long)results->feature_list[0][i][j]);
		#endif
		#if PYVERSION == 2
			item = PyInt_FromLong((long)results->feature_list[0][i][j]);
		#endif
			PyList_SetItem(f1, j, item); 
		}

		PyObject * f2 = PyList_New((Py_ssize_t)results->feature_length[1]);
		for(int j = 0; j < results->feature_length[1]; j++) {
		#if PYVERSION == 3
			item = PyLong_FromLong((long)results->feature_list[1][i][j]);
		#endif
		#if PYVERSION == 2
			item = PyInt_FromLong((long)results->feature_list[1][i][j]);
		#endif
			PyList_SetItem(f2, j, item); 
		}
			
		PyObject * f3 = PyList_New((Py_ssize_t)results->feature_length[2]);
		for(int j = 0; j < results->feature_length[2]; j++) {
		#if PYVERSION == 3
			item = PyLong_FromLong((long)results->feature_list[2][i][j]);
		#endif
		#if PYVERSION == 2
			item = PyInt_FromLong((long)results->feature_list[2][i][j]);
		#endif
			PyList_SetItem(f3, j, item); 
		}
			
		PyObject * f4 = PyList_New((Py_ssize_t)results->feature_length[3]);
		for(int j = 0; j < results->feature_length[3]; j++) {
		#if PYVERSION == 3
			item = PyLong_FromLong((long)results->feature_list[3][i][j]);
		#endif
		#if PYVERSION == 2
			item = PyInt_FromLong((long)results->feature_list[3][i][j]);
		#endif
			PyList_SetItem(f4, j, item); 
		}

		item = Py_BuildValue("(OOOO)", f1, f2, f3, f4);	
		PyList_SET_ITEM(pyList, i, item);
	}
	printf("Destroying features...\n");
	destroyFeatures(results);
	return pyList;
}

PyMethodDef FindVidMethods[] = {
	{"getCuts", getCutsWrapper, METH_VARARGS, "Passing a filename of a videofile, retrieve a list of cuts(framenumber)"},
	{"getFeatures", getFeaturesWrapper, METH_VARARGS, "Extract features and thumbnails in a video"}
};

#if PYVERSION == 3
PyModuleDef mod = {PyModuleDef_HEAD_INIT, "FindVid", "FindVid-Backendwrapper", -1, FindVidMethods, NULL, NULL, NULL, NULL};
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
