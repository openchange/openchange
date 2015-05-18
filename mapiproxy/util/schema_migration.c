/*
   Schema migration util procedures calling Python code

   OpenChange Project

   Copyright (C) Enrique J. Hernández 2015

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "schema_migration.h"
#include "libmapi/libmapi.h"

#include <stdbool.h>
#include <talloc.h>
#include <util/data_blob.h>
#include <Python.h>


static int migrate_schema(const char *connection_string, const char *schema_backend_class_name)
{
        PyObject *mailbox_mod;
        PyObject *py_ret_value = NULL;
        PyObject *mailbox_obj = NULL;
        int      another_init, retval = 0;

        another_init = Py_IsInitialized();
        Py_Initialize();

        mailbox_mod = PyImport_ImportModule("openchange.mailbox");

        if (mailbox_mod == NULL) {
                PyErr_Print();
                OC_DEBUG(0, "Unable to import mailbox Python module.");
                Py_Finalize();
                return -1;
        }

        mailbox_obj = PyObject_CallMethod(mailbox_mod, (char *) schema_backend_class_name,
                                          "(s)", connection_string);
        if (mailbox_obj == NULL) {
                PyErr_Print();
                OC_DEBUG(0, "Call to %s constructor failed", schema_backend_class_name);
                retval = 1;
                goto end;
        }

        py_ret_value = PyObject_CallMethod(mailbox_obj, "migrate", NULL);
        if (py_ret_value == NULL) {
                PyErr_Print();
                OC_DEBUG(0, "Call to %s.migrate failed", schema_backend_class_name);
                retval = 1;
        } else {
                Py_DECREF(py_ret_value);
                OC_DEBUG(5, "Call to %s.migrate succeeded", schema_backend_class_name);
        }

end:
        Py_XDECREF(mailbox_obj);
        Py_DECREF(mailbox_mod);
        if (!another_init) {
                Py_Finalize();
        }
        return retval;
}

/**
   \details Migrate OpenChangeDB MySQL backend schema using
            a connection string with this format:
		mysql://user[:pass]@host[:port]/database

   \param mem_ctx pointer to the memory context
   \param connection_string pointer to connection string

   \return 0 on success, 1 on failing called to Python code, -1 on Out-Of-Memory errors
 */
int migrate_openchangedb_schema(const char *connection_string)
{
        return migrate_schema(connection_string, "OpenChangeDBWithMysqlBackend");
}

/**
   \details Migrate Indexing MAPIStore MySQL backend schema using
            a connection string with this format:
		mysql://user[:pass]@host[:port]/database

   \param mem_ctx pointer to the memory context
   \param connection_string pointer to connection string

   \return 0 on success, 1 on failing called to Python code, -1 on Out-Of-Memory errors
 */
int migrate_indexing_schema(const char *connection_string)
{
        return migrate_schema(connection_string, "IndexingWithMysqlBackend");
}

/**
   \details Migrate Named Properties MAPIStore MySQL backend schema using
            a connection string with this format:
		mysql://user[:pass]@host[:port]/database

   \param mem_ctx pointer to the memory context
   \param connection_string pointer to connection string

   \return 0 on success, 1 on failing called to Python code, -1 on Out-Of-Memory errors
 */
int migrate_named_properties_schema(const char *connection_string)
{
        return migrate_schema(connection_string, "NamedPropertiesWithMysqlBackend");
}
