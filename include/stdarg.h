/**
 * stdarg.h
 */

#ifndef _STDARG_H
# define _STDARG_H

# define __va_list 			__builtin_va_list

typedef __va_list 			va_list;

# define va_start(ap, arg)	__builtin_va_start((ap), (arg))
# define va_arg				__builtin_va_arg
# define va_copy			__builtin_va_copy
# define va_end(ap)			__builtin_va_end((ap))

#endif // _STDARG_H
