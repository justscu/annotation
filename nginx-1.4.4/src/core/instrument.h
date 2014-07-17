#pragma once

/* Constructor and Destructor Prototypes */
void main_constructor( void ) __attribute__ ((no_instrument_function, constructor));
void main_destructor( void )  __attribute__ ((no_instrument_function, destructor));


void __cyg_profile_func_enter( void *func_address, void *call_site )__attribute__ ((no_instrument_function));
void __cyg_profile_func_exit ( void *func_address, void *call_site )__attribute__ ((no_instrument_function));
