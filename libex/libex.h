/*
 * Simple, local exception handling with binding forms.
 *
 * LICENSE: LGPL
 *
 * Examples:
 *
 * char *foo;
 * LET (foo = (char*)malloc(123)) {
 *   ... do something with foo
 * } CATCH (NullRefExc) {
 *   ... handle error for foo
 * } FINALLY {
 *   ... finalize any state that has already been allocated
 * }
 *
 * TRY (returnExceptionType()) { ... }
 * CATCH (ReadExc) { ... }
 * CATCH (WriteExc) { ... }
 * FINALLY { ... }
 *
 * NOTES:
 * -If you use TRY, you SHOULD specify an OTHERWISE branch.
 * -If you use LET or ENSURE, you MUST NOT specify an OTHERWISE branch.
 */

#ifndef __LIBEX__
#define __LIBEX__

/*
 * FUTURE WORK:
 * 1. Compile-time conditional to log the current file, function, and line # on exception.
 * 2. A function definition form that implicitly returns exceptions, and provides
 *    exception propagation/stack unwinding.
 */

#include <limits.h>

typedef enum exc_type {
	NullRefExc,
	EnsureViolatedExc = NullRefExc,
	ReadExc,
	WriteExc,
	/* ... */
	NoExc = INT_MAX,
} exc_type;

#define TRY(X) _EXCOPEN(X) CATCH(NoExc)
#define _EXCOPEN(X) do { switch ((int)(X)) {
#define LET(X) _EXCOPEN(X) default:
#define ENSURE(X) LET(X)
#define CATCH(e) break; case e:
#define OTHERWISE default:
#define FINALLY break; } } while(0);

#endif /*__LIBEX__*/
