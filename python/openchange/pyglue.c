#include <Python.h>
#include <libmapi/version.h>

void init_glue(void);

static PyMethodDef py_misc_methods[] = {
	{ NULL }
};

void init_glue(void)
{
	PyObject *m;

	m = Py_InitModule3("_glue", py_misc_methods,
			   "Python bindings for miscellaneous OpenChange functions.");
	if (m == NULL)
		return;

	PyModule_AddObject(m, "version",
			   PyString_FromString(OPENCHANGE_VERSION_STRING));
}
