#!/usr/bin/env python2.7

import os
from os.path import dirname
import sys

def parse_equality(line, last_value):
    if line[-1] == ",":
        line = line[:-1]

    error_name = None
    error_value = -1

    equ_idx = line.find("=")
    if equ_idx > -1:
        error_name = line[0:equ_idx].strip()
        error_value = int(line[equ_idx+1:].strip())
    else:
        error_name = line.strip()
        error_value = last_value + 1

    return (error_name, error_value)

def read_errors(errors_file):
    errors = {}

    in_enum = False
    done = False
    error_value = -1

    while not done:
        line = errors_file.readline().strip()
        if in_enum:
            if line.find("}") > -1:
                done = True
            elif line.find("MAPISTORE_") > -1:
                (error_name, error_value) \
                    = parse_equality(line, error_value)
                if error_name is not None:
                    errors[error_name] = error_value
        elif line.find("enum mapistore_error") > -1:
            in_enum = True

    return errors

def output_errors(output_file, errors):
    output_file.write("""/* mapistore_errors.c -- auto-generated */

#include <Python.h>
#include "pymapistore.h"

void initmapistore_errors(PyObject *parent_module)
{
	PyObject	*errors_module;

	errors_module = Py_InitModule3("errors", NULL,
				       "Error codes of the mapistore operations");
	if (errors_module == NULL) {
		return;
	}
	PyModule_AddObject(parent_module, "errors", errors_module);

""")

    for error_name, error_value in errors.iteritems():
        error_value = errors[error_name]
        output_file.write("  PyModule_AddObject(errors_module, \"%s\"," \
                              " PyInt_FromLong(%d));\n"
                          % (error_name, error_value))
    output_file.write("}\n")

if __name__ == "__main__":
    if len(sys.argv) > 2:
        errors_file = open(sys.argv[1])
        errors = read_errors(errors_file)
        if sys.argv[2] == "-":
            output_file = sys.stdout
            close_out = False
        else:
            output_file = open(sys.argv[2], "w+")
        output_errors(output_file, errors)
    else:
        sys.stderr.write("2 arguments required: mapistore_error.h and output"
                         " filename (or \"-\")\n")
        sys.exit(-1)
