#include <stdio.h>
#include <stdlib.h>
#include "instrument.h"

/* gcc CFLAGS="-g -finstrument-functions"*/

/* Output trace file pointer */
static FILE *fp;

void main_constructor( void )
{
	fp = fopen( "trace.txt", "w" );
	if (fp == NULL) 
		exit(-1);
}
void main_deconstructor( void )
{
	fclose( fp );
}



void __cyg_profile_func_enter( void *this, void *callsite )
{
	/* Function Entry Address */
	fprintf(fp, "E%p\n", (int *)this);
}
void __cyg_profile_func_exit( void *this, void *callsite )
{
	/* Function Exit Address */
	fprintf(fp, "X%p\n", (int *)this);
}

