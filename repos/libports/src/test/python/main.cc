/*
 * \brief  Test for using Python on Genode
 * \author Sebastian Sumpf
 * \date   2010-02-17
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Python includes */
#include <python2.6/Python.h>

/* libc includes */
#include <stdio.h>
#include <fcntl.h>


extern "C"  int __sread(void *, char *, int);


int main(int argc, char const ** args)
{
	if (argc < 1) {
		fprintf(stderr, "Error: need <scriptname>.py as argument!\n");
		return -1;
	}

	char * name = const_cast<char*>(args[0]);
	FILE * fp = fopen(name, "r");
	//fp._flags = __SRD;
	Py_SetProgramName(name);
	//don't need the 'site' module
	Py_NoSiteFlag = 1;
	//don't support interactive mode, yet
	Py_InteractiveFlag = 0;
	Py_Initialize();

	fprintf(stderr, "Starting python ...\n");
	PyRun_SimpleFile(fp, name);

	return 0;
}
