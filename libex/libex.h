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
 * # FINALLY *must* always come last, and must must terminate every exception block.
 * # If you use TRY, you *should* specify an OTHERWISE branch.
 * # If you use LET or ENSURE, you *must not* specify an OTHERWISE branch.
 * # You *cannot* use any control-flow operators, ie. goto, break, continue,
 *   return, that will *escape* an exception block. Any control-flow that stays
 *   within the same exception block is fine.
 */

#ifndef __LIBEX__
#define __LIBEX__

/*
 * FUTURE WORK:
 * 1. Compile-time conditional to log the current file, function, and line # on exception.
 * 2. A function definition form that implicitly returns exceptions, and provides
 *    exception propagation/stack unwinding.
 */

#include <stdlib.h>
#include <limits.h>

typedef enum exc_type {
	NullRefExc = (int)NULL,
	EnsureViolatedExc = 0,	/* ENSURE(COND): COND is false means it's zero */
	ReadExc,
	WriteExc,
	/* ... */
	NoExc = INT_MAX,
} exc_type;

/*
 * An exception handling block expands into a simple switch statement, with
 * each exception becoming a case.
 *
 * TRY takes an expression returning an exc_type. Functions returning normally
 * must return the NoExc exception. OTHERWISE becomes the "default"
 * case of the switch.
 *
 * LET takes an expression returning a pointer type, and is generally used for
 * bindings. The body of the LET is the "default" case in the switch. The only
 * exception thrown by LET is NullRefExc. Since NULL and NullRefExc have the
 * same value, an assignment of NULL will branch to the NullRefExc case.
 * Otherwise, the default case executes becase the value is non-null.
 *
 * ENSURE takes an expression returning true/false, and is generally used for
 * checks where you want to specify finalization on failure. The body of
 * ENSURE is the "default" case of the switch. Expressions that evaluate to 0
 * are considered false in C, and EnsureViolatedExc == 0, thus any false
 * expressions will branch to that case.
 *
 * FINALLY terminates the exception handling block and must always appear, even
 * if no finalization code is necessary.
 */
#define TRY(X) _EXCOPEN(X) CATCH(NoExc)
#define _EXCOPEN(X) do { switch ((int)(X)) {
#define LET(X) _EXCOPEN(X) default:
#define ENSURE(X) LET(X)
#define CATCH(e) break; case e:
#define OTHERWISE default:
#define FINALLY break; } } while(0);

#endif /*__LIBEX__*/
