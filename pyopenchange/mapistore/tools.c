#include <Python.h>
#include "pyopenchange/mapistore/pymapistore.h"
#include "gen_ndr/exchange.h"

enum mapistore_error pymapistore_get_uri(struct mapistore_context *mstore_ctx, const char *username, uint64_t fmid, PyObject **ppy_uri)
{
	TALLOC_CTX			*mem_ctx;
	enum mapistore_error 		retval;
	char				*uri;
	bool				soft_deleted;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		return MAPISTORE_ERR_NO_MEMORY;
	}
	/* Retrieve the URI from the indexing */
	retval = mapistore_indexing_record_get_uri(mstore_ctx, username, mem_ctx, fmid, &uri, &soft_deleted);

	if (retval != MAPISTORE_SUCCESS) {
		goto end;
	}

	if (soft_deleted == true) {
		DEBUG(0, ("[ERR][%s]: Soft-deleted message\n", __location__));
		retval = MAPISTORE_ERR_INVALID_DATA;
		goto end;
	}

	*ppy_uri = PyString_FromString(uri);
	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
end:
	talloc_free(mem_ctx);
	return retval;
}

enum mapistore_error pymapistore_get_properties(PyObject *list, struct mapistore_context *mstore_ctx, uint32_t context_id, void *object, PyObject **ppy_dict)
{
	TALLOC_CTX			*mem_ctx;
	PyObject			*py_key, *py_ret = NULL;
	struct SPropTagArray		*properties;
	struct mapistore_property_data  *prop_data;
	const char			*sproptag;
	enum mapistore_error		retval;
	enum MAPISTATUS			ret;
	enum MAPITAGS			proptag;
	Py_ssize_t			i, count;

	/* Get the available properties */
	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		return MAPISTORE_ERR_NO_MEMORY ;
	}

	properties = talloc_zero(mem_ctx, struct SPropTagArray);
	if (properties == NULL) {
		retval = MAPISTORE_ERR_NO_MEMORY;
		goto end;
	}

	properties->aulPropTag = talloc_zero(properties, void);
	if (properties->aulPropTag == NULL) {
		retval = MAPISTORE_ERR_NO_MEMORY;
		goto end;
	}

	if (list == NULL) {
		/* If no list of needed properties is provided, return all */
		retval = mapistore_properties_get_available_properties(mstore_ctx, context_id,
				object, mem_ctx, &properties);
		if (retval != MAPISTORE_SUCCESS) {
			goto end;
		}
	} else {
		/* Check the input argument */
		if (PyList_Check(list) == false) {
			DEBUG(0, ("[ERR][%s]: Input argument must be a list\n", __location__));
			retval = MAPISTORE_ERR_INVALID_PARAMETER;
			goto end;
		}

		/* Build the SPropTagArray structure */
		count = PyList_Size(list);

		for (i = 0; i < count; i++) {
			py_key = PyList_GetItem(list, i);

			sproptag = PyString_AsString(py_key);
			proptag = openchangedb_named_properties_get_tag((char *)sproptag);
			if (proptag == 0xFFFFFFFF) {
				proptag = openchangedb_property_get_tag((char *)sproptag);
				if (proptag == 0xFFFFFFFF) {
					proptag = strtol(sproptag, NULL, 16);
				}
			}

			ret = SPropTagArray_add(mem_ctx, properties, proptag);
			if (ret != MAPI_E_SUCCESS) {
				retval = MAPISTORE_ERROR;
				goto end;
			}
		}
	}

	/* Get the available values */
	prop_data = talloc_zero_array(mem_ctx, struct mapistore_property_data, properties->cValues);
	if (prop_data == NULL) {
		retval = MAPISTORE_ERR_NO_MEMORY;
		goto end;
	}

	retval = mapistore_properties_get_properties(mstore_ctx, context_id, object, mem_ctx,
						     properties->cValues, properties->aulPropTag, prop_data);
	if (retval != MAPISTORE_SUCCESS) {
		goto end;
	}

	/* Build a Python dictionary object with the tags and the property values */
	py_ret = pymapistore_python_dict_from_properties(properties->aulPropTag, prop_data, properties->cValues);
	if (py_ret == NULL) {
		DEBUG(0, ("[ERR][%s]: Error building the dictionary\n", __location__));
		retval = MAPISTORE_ERROR;
		goto end;
	}

	*ppy_dict = py_ret;
	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;

end:
	talloc_free(mem_ctx);
	return retval;
}

enum mapistore_error pymapistore_set_properties(PyObject *dict, struct mapistore_context *mstore_ctx, uint32_t context_id, void *object)
{
	TALLOC_CTX		*mem_ctx;
	PyObject		*py_key, *py_value;
	struct SRow		*aRow;
	struct SPropValue	newValue;
	void			*data;
	enum MAPITAGS		tag;
	enum mapistore_error	retval;
	enum MAPISTATUS		ret;
	Py_ssize_t		pos = 0;

	/* Check the input argument */
	if (PyDict_Check(dict) == false) {
		DEBUG(0, ("[ERR][%s]: Input argument must be a dictionary\n", __location__));
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		return MAPISTORE_ERR_NO_MEMORY ;
	}

	aRow = talloc_zero(mem_ctx, struct SRow);
	if (aRow == NULL) {
		retval = MAPISTORE_ERR_NO_MEMORY;
		goto end;
	}

	while (PyDict_Next(dict, &pos, &py_key, &py_value)) {
		/* Transform the key into a property tag */
		if (PyString_Check(py_key)) {
			tag = get_proptag_value(PyString_AsString(py_key));

			if (tag == 0xFFFFFFFF) {
				DEBUG(0, ("[ERR][%s]: Unsupported property tag '%s'\n",
						__location__, PyString_AsString(py_key)));
				retval = MAPISTORE_ERR_INVALID_DATA;
				goto end;
			}
		} else if (PyInt_Check(py_key)) {
			tag = PyInt_AsUnsignedLongMask(py_key);
		} else {
			DEBUG(0, ("[ERR][%s]: Invalid property type: only strings and integers accepted\n", __location__));
			retval = MAPISTORE_ERR_INVALID_PARAMETER;
			goto end;
		}

		/* Transform the input value into proper C type */
		retval = pymapistore_data_from_pyobject(mem_ctx,tag, py_value, &data);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("[WARN][%s]: Unsupported value for property '%s'\n",
					__location__, PyString_AsString(py_key)));
			continue;
		}

		/* Update aRow */
		if (set_SPropValue_proptag(&newValue, tag, data) == false) {
			DEBUG(0,("[ERR][%s]: Can't set property 0x%x\n", __location__, tag));
			retval = MAPISTORE_ERROR;
			goto end;
		}

		ret = SRow_addprop(aRow, newValue);
		if (ret != MAPI_E_SUCCESS) {
			retval = MAPISTORE_ERROR;
			goto end;
		}
	}

	/* Set the properties from aRow */
	retval = mapistore_properties_set_properties(mstore_ctx, context_id, object, aRow);
	if (retval != MAPISTORE_SUCCESS) {
		goto end;
	}

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
end:
	talloc_free(mem_ctx);
	return retval;
}

PyObject *pymapistore_python_dict_from_properties(enum MAPITAGS *aulPropTag, struct mapistore_property_data *prop_data, uint32_t count)
{
	PyObject 		*py_ret, *py_key, *py_val, *item;
	struct Binary_r		*bin;
	struct FILETIME		*ft;
	struct timeval		t;
	struct StringArrayW_r	*MVszW;
	struct StringArray_r	*MVszA;
	NTTIME			nt;
	const char		*skey;
	unsigned		key;
	int			ret, i;

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
		key = aulPropTag[i];
		/* Set the value of the dictionary entry */
		if (prop_data[i].error) {
			key = (key & 0xFFFF0000) | PT_ERROR;
			py_val = PyString_FromString(mapistore_errstr(prop_data[i].error));
		} else {
			/* Sanity check */
			if (prop_data[i].data == 0x0){
				DEBUG(0, ("[ERR][%s]: Invalid data pointer for tag 0x%x (and no associated error)\n",
					  __location__, key));
				Py_DECREF(py_ret);
				return NULL;
			}
			py_val = NULL;

			switch (key & 0xFFFF) {
			case PT_I2:
				DEBUG(5, ("[WARN][%s]: PT_I2 case not implemented\n", __location__));
				break;
			case PT_LONG:
				py_val = PyLong_FromLong(*((uint32_t *)prop_data[i].data));
				break;
			case PT_DOUBLE:
				DEBUG(5, ("[WARN][%s]: PT_DOUBLE case not implemented\n", __location__));
				break;
			case PT_BOOLEAN:
				py_val = PyBool_FromLong(*((uint32_t *)prop_data[i].data));
				break;
			case PT_I8:
				py_val = PyLong_FromUnsignedLongLong(*((uint64_t *)prop_data[i].data));
				break;
			case PT_STRING8:
			case PT_UNICODE:
				py_val = PyString_FromString((const char *)prop_data[i].data);
				break;
			case PT_SYSTIME:
				ft = (struct FILETIME *) prop_data[i].data;
				nt = ft->dwHighDateTime;
				nt = nt << 32;
				nt |= ft->dwLowDateTime;
				nttime_to_timeval(&t, nt);
				py_val = PyFloat_FromString(PyString_FromFormat("%ld.%ld", t.tv_sec, t.tv_usec), NULL);
				break;
			case PT_CLSID:
				DEBUG(5, ("[WARN][%s]: PT_CLSID case not implemented\n", __location__));
				break;
			case PT_SVREID:
			case PT_BINARY:
				bin = (struct Binary_r *)prop_data[i].data;
				py_val = PyByteArray_FromStringAndSize((const char *)bin->lpb, bin->cb);
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
				py_val = PyList_New(MVszA->cValues);
				if (py_val == NULL) {
					DEBUG(0, ("[ERR][%s]: Unable to initialized Python List\n",
						  __location__));
					Py_DECREF(py_ret);
					return NULL;
				}
				for (i = 0; i < MVszA->cValues; i++) {
					item = PyString_FromString(MVszA->lppszA[i]);
					if (PyList_SetItem(py_val, i, item) == -1) {
						DEBUG(0, ("[ERR][%s]: Unable to append entry to Python list\n",
							  __location__));
						Py_DECREF(py_val);
						Py_DECREF(py_ret);
						return NULL;
					}
				}
				break;
			case PT_MV_UNICODE:
				MVszW = (struct StringArrayW_r *)prop_data[i].data;
				py_val = PyList_New(MVszW->cValues);
				if (py_val == NULL) {
					DEBUG(0, ("[ERR][%s]: Unable to initialise Python List\n",
						  __location__));
					Py_DECREF(py_ret);
					return NULL;
				}

				for (i = 0; i < MVszW->cValues; i++) {
					item = PyString_FromString(MVszW->lppszW[i]);
					if (PyList_SetItem(py_val, i, item) == -1) {
						DEBUG(0, ("[ERR][%s]: Unable to append entry to Python list\n",
							  __location__));
						Py_DECREF(py_val);
						Py_DECREF(py_ret);
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

		}

		/* Set the key of the dictionary entry */
		skey = get_proptag_name(key);
		if (skey == NULL) {
			py_key = PyString_FromFormat("0x%x", key);
		} else {
			py_key = PyString_FromString(skey);
		}

		/* Set the dictionary entry */
		if (py_val) {
			ret = PyDict_SetItem(py_ret, py_key, py_val);
			if (ret != 0) {
				DEBUG(0, ("[ERR][%s]: Unable to add entry to Python dictionary\n",
					  __location__));
				Py_DECREF(py_key);
				Py_DECREF(py_val);
				Py_DECREF(py_ret);
				return NULL;
			}
			Py_DECREF(py_val);
		}
		Py_DECREF(py_key);
	}
	return py_ret;
}

enum mapistore_error pymapistore_data_from_pyobject(TALLOC_CTX *mem_ctx, uint32_t proptag, PyObject *value, void **data)
{
	PyObject		*item;
	struct Binary_r		*bin;
	struct StringArrayW_r	*MVszW;
	struct StringArray_r	*MVszA;
	struct FILETIME		*ft;
	NTTIME			nt;
	char			*str;
	enum mapistore_error	retval = MAPISTORE_ERR_NOT_FOUND;
	uint64_t		ll;
	uint32_t		count;
	int			l;
	bool			b;

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

void PyErr_SetMAPIStoreError(uint32_t retval)
{
	PyErr_SetObject(PyExc_RuntimeError,
			Py_BuildValue("(i, s)", retval, mapistore_errstr(retval)));
}

void PyErr_SetMAPISTATUSError(enum MAPISTATUS retval)
{
	PyErr_SetObject(PyExc_RuntimeError,
			Py_BuildValue("(i, s)", retval, mapi_get_errstr(retval)));
}
