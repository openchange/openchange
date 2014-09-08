#include <Python.h>
#include "pyopenchange/mapistore/pymapistore.h"
#include "gen_ndr/exchange.h"

PyObject *pymapistore_python_dict_from_properties(enum MAPITAGS *aulPropTag, struct mapistore_property_data *prop_data, uint32_t count)
{
	PyObject 		*py_ret;
	int			ret, i;
	unsigned		key;
	const char		*skey;
	PyObject		*pykey;
	PyObject		*pyval;
	PyObject		*item;
	struct Binary_r		*bin;
	NTTIME			nt;
	struct FILETIME		*ft;
	struct timeval		t;
	struct StringArrayW_r	*MVszW;
	struct StringArray_r	*MVszA;

	/* Sanity checks */
	if (!aulPropTag) return NULL;
	if (!prop_data) return NULL;

	/* Create the dictionary */
	py_ret = PyDict_New();
	if (py_ret == NULL) {
		DEBUG(0, ("[ERR][%s]: Unable to initialize Python Dictionary\n", __location__));
		return NULL;
	}

	/* Build the dictionary */
	for (i = 0; i < count; i++) {
		/* Set the key of the dictionary entry */
		key = aulPropTag[i];
		if (prop_data[i].error) {
			key = key | PT_ERROR;
		}
		skey = openchangedb_property_get_attribute(key);
		if (skey == NULL) {
			pykey = PyString_FromFormat("0x%x", key);
		} else {
			pykey = PyString_FromString(skey);
		}

		/* Set the value of the dictionary entry */
		pyval = NULL;

		switch (key & 0xFFFF) {
		case PT_I2:
			DEBUG(5, ("[WARN][%s]: PT_I2 case not implemented\n", __location__));
			break;
		case PT_LONG:
			pyval = PyLong_FromLong(*((uint32_t *)prop_data[i].data));
			break;
		case PT_DOUBLE:
			DEBUG(5, ("[WARN][%s]: PT_DOUBLE case not implemented\n", __location__));
			break;
		case PT_BOOLEAN:
			pyval = PyBool_FromLong(*((uint32_t *)prop_data[i].data));
			break;
		case PT_I8:
			pyval = PyLong_FromUnsignedLongLong(*((uint64_t *)prop_data[i].data));
			break;
		case PT_STRING8:
		case PT_UNICODE:
			pyval = PyString_FromString((const char *)prop_data[i].data);
			break;
		case PT_SYSTIME:
			ft = (struct FILETIME *) prop_data[i].data;
			nt = ft->dwHighDateTime;
			nt = nt << 32;
			nt |= ft->dwLowDateTime;
			nttime_to_timeval(&t, nt);
			pyval = PyFloat_FromString(PyString_FromFormat("%ld.%ld", t.tv_sec, t.tv_usec), NULL);
			break;
		case PT_CLSID:
			DEBUG(5, ("[WARN][%s]: PT_CLSID case not implemented\n", __location__));
			break;
		case PT_SVREID:
		case PT_BINARY:
			bin = (struct Binary_r *)prop_data[i].data;
			pyval = PyByteArray_FromStringAndSize((const char *)bin->lpb, bin->cb);
			break;
		case PT_MV_SHORT:
			DEBUG(5, ("[WARN][%s]: PT_MV_I2 case not implemented\n", __location__));
			break;
		case PT_MV_LONG:
			DEBUG(5, ("[WARN][%s]: PT_MV_LONG case not implemented\n", __location__));
			break;
		case PT_MV_I8:
			DEBUG(5, ("[WARN][%s]: PT_MV_I8 case not implemented\n", __location__));
			break;
		case PT_MV_STRING8:
			MVszA = (struct StringArray_r *)prop_data[i].data;
			pyval = PyList_New(MVszA->cValues);
			if (pyval == NULL) {
				DEBUG(0, ("[ERR][%s]: Unable to initialized Python List\n",
					  __location__));
				return NULL;
			}
			for (i = 0; i < MVszA->cValues; i++) {
				item = PyString_FromString(MVszA->lppszA[i]);
				if (PyList_SetItem(pyval, i, item) == -1) {
					DEBUG(0, ("[ERR][%s]: Unable to append entry to Python list\n",
						  __location__));
					return NULL;
				}
			}
			break;
		case PT_MV_UNICODE:
			MVszW = (struct StringArrayW_r *)prop_data[i].data;
			pyval = PyList_New(MVszW->cValues);
			if (pyval == NULL) {
				DEBUG(0, ("[ERR][%s]: Unable to initialized Python List\n",
					  __location__));
				return NULL;
			}

			for (i = 0; i < MVszW->cValues; i++) {
				item = PyString_FromString(MVszW->lppszW[i]);
				if (PyList_SetItem(pyval, i, item) == -1) {
					DEBUG(0, ("[ERR][%s]: Unable to append entry to Python list\n",
						  __location__));
					return NULL;
				}
			}
			break;
		case PT_MV_SYSTIME:
			DEBUG(5, ("[WARN][%s]: PT_MV_SYSTIME case not implemented\n", __location__));
			break;
		case PT_MV_CLSID:
			DEBUG(5, ("[WARN][%s]: PT_MV_CLSID case not implemented\n", __location__));
			break;
		case PT_MV_BINARY:
			DEBUG(5, ("[WARN][%s]: PT_MV_BINARY case not implemented\n", __location__));
			break;
		case PT_NULL:
			DEBUG(5, ("[WARN][%s]: PT_NULL case not implemented\n", __location__));
			break;
		case PT_OBJECT:
			DEBUG(5, ("[WARN][%s]: PT_OBJECT case not implemented\n", __location__));
			break;
		default:
			DEBUG(5, ("[WARN][%s]: 0x%x case not implemented\n", __location__,
				  (key & 0xFFFF)));
			break;
		}

		/* Set the dictionary entry */
		if (pyval) {
			ret = PyDict_SetItem(py_ret, pykey, pyval);
			if (ret != 0) {
				DEBUG(0, ("[ERR][%s]: Unable to add entry to Python dictionary\n",
					  __location__));
				return NULL;
			}
		}
	}

	return py_ret;
}

enum mapistore_error pymapistore_data_from_pyobject(TALLOC_CTX *mem_ctx,
							 uint32_t proptag,
							 PyObject *value,
							 void **data)
{
	enum mapistore_error	retval = MAPISTORE_ERR_NOT_FOUND;
	PyObject		*item;
	uint32_t		count;
	bool			b;
	int			l;
	uint64_t		ll;
	struct Binary_r		*bin;
	struct StringArray_r	*MVszA;
	struct StringArrayW_r	*MVszW;
	NTTIME			nt;
	struct FILETIME		*ft;
	char			*str;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!proptag, MAPISTORE_ERR_NOT_FOUND, NULL);
	MAPISTORE_RETVAL_IF(!value, MAPISTORE_ERR_NOT_FOUND, NULL);
	MAPISTORE_RETVAL_IF(!data, MAPISTORE_ERR_NOT_FOUND, NULL);

	switch (proptag & 0xFFFF) {
		case PT_I2:
			DEBUG(5, ("[WARN][%s]: PT_I2 case not implemented\n", __location__));
			break;
		case PT_LONG:
			l = PyLong_AsLong(value);
			MAPISTORE_RETVAL_IF(l == -1, MAPISTORE_ERR_NOT_FOUND, NULL);
			*data = talloc_memdup(mem_ctx, &l, sizeof(l));
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_DOUBLE:
			DEBUG(5, ("[WARN][%s]: PT_DOUBLE case not implemented\n", __location__));
			break;
		case PT_BOOLEAN:
			MAPISTORE_RETVAL_IF((PyBool_Check(value) == false), MAPISTORE_ERR_NOT_FOUND, NULL);
			b = (value == Py_True);
			*data = talloc_memdup(mem_ctx, &b, sizeof(b));
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_I8:
			if (PyInt_Check(value)) {
				ll = (unsigned long long)PyInt_AsLong(value);
			} else if (PyLong_Check(value)) {
				ll = PyLong_AsLongLong(value);
			} else {
				PyErr_SetString(PyExc_TypeError, "an integer is required");
				ll = -1;
			}
			MAPISTORE_RETVAL_IF(ll == -1, MAPISTORE_ERR_NOT_FOUND, NULL);
			*data = talloc_memdup(mem_ctx, &ll, sizeof(ll));
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_STRING8:
		case PT_UNICODE:
			MAPISTORE_RETVAL_IF(!PyString_Check(value) && !PyUnicode_Check(value),
					    MAPISTORE_ERR_NOT_FOUND, NULL);
			str = PyString_AsString(value);
			MAPISTORE_RETVAL_IF(!str, MAPISTORE_ERR_NOT_FOUND, NULL);

			*data = (void *)talloc_strdup(mem_ctx, str);
			MAPISTORE_RETVAL_IF(!*data, MAPISTORE_ERR_NOT_FOUND, NULL);
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_SYSTIME:
			MAPISTORE_RETVAL_IF(!PyFloat_Check(value), MAPISTORE_ERR_NOT_FOUND, NULL);
			unix_to_nt_time(&nt, PyFloat_AsDouble(value));
			ft = talloc_zero(mem_ctx, struct FILETIME);
			MAPISTORE_RETVAL_IF(!ft, MAPISTORE_ERR_NOT_FOUND, NULL);
			ft->dwLowDateTime = (nt << 32) >> 32;
			ft->dwHighDateTime = nt >> 32;
			*data = (void *) ft;
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_CLSID:
			DEBUG(5, ("[WARN][%s]: PT_CLSID case not implemented\n", __location__));
			break;
		case PT_SVREID:
		case PT_BINARY:
			MAPISTORE_RETVAL_IF(!PyByteArray_Check(value), MAPISTORE_ERR_NOT_FOUND, NULL);
			bin = talloc_zero(mem_ctx, struct Binary_r);
			MAPISTORE_RETVAL_IF(!bin, MAPISTORE_ERR_NOT_FOUND, NULL);
			bin->cb = PyByteArray_Size(value);
			bin->lpb = talloc_memdup(bin, PyByteArray_AsString(value), bin->cb);
			MAPISTORE_RETVAL_IF(!bin->lpb, MAPISTORE_ERR_NOT_FOUND, bin);
			*data = (void *) bin;
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_MV_SHORT:
			DEBUG(5, ("[WARN][%s]: PT_MV_I2 case not implemented\n", __location__));
			break;
		case PT_MV_LONG:
			DEBUG(5, ("[WARN][%s]: PT_MV_LONG case not implemented\n", __location__));
			break;
		case PT_MV_I8:
			DEBUG(5, ("[WARN][%s]: PT_MV_I8 case not implemented\n", __location__));
			break;
		case PT_MV_STRING8:
			MAPISTORE_RETVAL_IF(!PyList_Check(value), MAPISTORE_ERR_NOT_FOUND, NULL);
			MVszA = talloc_zero(mem_ctx, struct StringArray_r);
			MAPISTORE_RETVAL_IF(!MVszA, MAPISTORE_ERR_NOT_FOUND, NULL);
			MVszA->cValues = PyList_Size(value);
			MVszA->lppszA = (const char **) talloc_array(MVszA, char *, MVszA->cValues + 1);
			for (count = 0; count < MVszA->cValues; count++) {
				item = PyList_GetItem(value, count);
				MAPISTORE_RETVAL_IF(!item, MAPISTORE_ERR_INVALID_PARAMETER, MVszA);
				str = PyString_AsString(item);
				MAPISTORE_RETVAL_IF(!str, MAPISTORE_ERR_INVALID_PARAMETER, MVszA);
				MVszA->lppszA[count] = talloc_strdup(MVszA->lppszA, str);
			}
			*data = (void *) MVszA;
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_MV_UNICODE:
			MAPISTORE_RETVAL_IF(!PyList_Check(value), MAPISTORE_ERR_NOT_FOUND, NULL);
			MVszW = talloc_zero(mem_ctx, struct StringArrayW_r);
			MAPISTORE_RETVAL_IF(!MVszW, MAPISTORE_ERR_NOT_FOUND, NULL);
			MVszW->cValues = PyList_Size(value);
			MVszW->lppszW = (const char **) talloc_array(MVszW, char *, MVszW->cValues + 1);
			for (count = 0; count < MVszW->cValues; count++) {
				item = PyList_GetItem(value, count);
				MAPISTORE_RETVAL_IF(!item, MAPISTORE_ERR_INVALID_PARAMETER, MVszW);
				str = PyString_AsString(item);
				MAPISTORE_RETVAL_IF(!str, MAPISTORE_ERR_INVALID_PARAMETER, MVszW);
				MVszW->lppszW[count] = talloc_strdup(MVszW->lppszW, str);
			}
			*data = (void *) MVszW;
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_MV_SYSTIME:
			DEBUG(5, ("[WARN][%s]: PT_MV_SYSTIME case not implemented\n", __location__));
			break;
		case PT_MV_CLSID:
			DEBUG(5, ("[WARN][%s]: PT_MV_CLSID case not implemented\n", __location__));
			break;
		case PT_MV_BINARY:
			DEBUG(5, ("[WARN][%s]: PT_MV_BINARY case not implemented\n", __location__));
			break;
		case PT_NULL:
			DEBUG(5, ("[WARN][%s]: PT_NULL case not implemented\n", __location__));
			break;
		case PT_OBJECT:
			DEBUG(5, ("[WARN][%s]: PT_OBJECT case not implemented\n", __location__));
			break;
		default:
			DEBUG(5, ("[WARN][%s]: 0x%x case not implemented\n", __location__,
				  (proptag & 0xFFFF)));
			break;
		}

	return retval;
}

