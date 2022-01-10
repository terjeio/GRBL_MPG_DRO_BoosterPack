/*
 * quickselect.h - header file for median algorithm
 *
 * v1.00 / 2016-02-12 / Terje Io
 *
 */

#include <stdint.h>

#define elem_type uint32_t

elem_type quick_select(elem_type arr[], int n);
