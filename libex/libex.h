/*
 * Simple, local exception handling with implicit binding forms.
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
 * Function that throws exceptions:
 * static RETURNS(int) foo() {
 *   ...
 *   if (X) THROW(int, ReadExc);
 *   RETURN(int, 8);
 * }
 *
 * If you use TRY, you SHOULD specify an OTHERWISE branch.
 * If you use LET or ENSURE, you MUST NOT specify an OTHERWISE branch.
 */

#ifndef __SWITCH_EXC__
#define __SWITCH_EXC__

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

#define RETURNS(T) struct /*__return_##X*/ { exc_type e; T value; }
#define _RAISE(T, X, E) { RETURNS(T) __x = { (E), (T)(X) }; return __x; }
#define RETURN(T, X) _RAISE(T, X, NoExc)
#define THROW(T, E) _RAISE(T, 0, E)

//#define TRY(T, X, F) do { RETURNS(T) __x = (F); T X = __x.value; _TRY(__x.e) } while(0);
#define TRY(X) _EXCOPEN(X) CATCH(NoExc)
#define _EXCOPEN(X) do { switch ((int)(X)) {
#define LET(X) _EXCOPEN(X) default:
#define ENSURE(X) LET(X)
#define CATCH(e) break; case e:
#define ONERROR CATCH(NullRefExc)
#define OTHERWISE default:
#define FINALLY break; } } while(0);

#endif /*__SWITCH_EXC__*/
