# Exception handling and RAII for C #
This library provides a small syntactic C abstraction for purely **local** exception handling and RAII. It provides a TRY binding form, with CATCH and FINALLY blocks in a single .h file. All constructs can be arbitrarily nested.

The exception handling form is slightly different than what you'll find in most languages or libraries which generally provide a try-catch-finally structure. Instead, libex exception handling consists of a try-in-catch-finally form, first suggested by Nick Benton and Andrew Kennedy of Microsoft Research in their paper titled [Exceptional Syntax](http://lambda-the-ultimate.org/node/1193).

This structure was then generalized by Gordon Plotkin and Matija Pretnar in their paper, [Handlers for Algebraic Effects](http://lambda-the-ultimate.org/node/4006). The try-in-catch-finally structure has nicer algebraic properties than the more common try-catch-finally. It also lends itself to particularly efficient compilation.

# Why? #

In truth, the well-known [RAII pattern in C](http://vilimpoc.org/research/raii-in-c/) can be clearer than this pattern for deeply nested try-finally hierarchies. However, any functions whose try-finally blocks are so deeply nested as to make the program difficult to understand should be factored out into a number of smaller functions as a matter of good design.

Furthermore, these macros ensure that you cannot accidentally goto the **wrong** finalizer, and so programs will be more robust in the presence of refactoring.

Finally, this exception form makes a clear distinction between normal program logic, error handling logic, and finalization logic, which makes it distinctly easier to understand the intent of code at a glance.

# Status #

The design seems to have converged on something satisfactory, and [some tests](http://code.google.com/p/libex/source/browse/libex/tests.c) are available. Future work also includes some Windows-specific convenience macros. Until the tests are more comprehensive, use at your own risk!

# Exception Form #

Here is the basic TRY-IN-CATCH-FINALLY form:
```
TRY(char *foo) {
    if (NULL == (foo = (char*)malloc(123)) THROW(errno)
} IN {
    // ... no exception was raised, so compute something with foo
} HANDLE CATCH (EOutOfMemory) {
    // ... handle error for foo
} CATCHANY {
    // ... other errors?
} FINALLY {
   // ... finalize any state that has already been allocated
}
```
The TRY() syntax allows you to specify the bindings that are to be initialized before before executing the IN block. These bindings are only visible in the TRY and IN blocks, and not in the CATCH clauses.

Execution proceeds to IN only if no exceptions were raised in the TRY block. Any exceptions raised in the IN block propagate up to the enclosing scope.

HANDLE is specified only on the **first** CATCH block, directly after the IN block. CATCHANY is optional.

CATCH blocks must specify any integer-compatible values for error codes. For example, you can match on any symbols defined in `errno.h`, or on on HRESULT codes for Windows. libex also provides an optional enum providing human-readable names for the symbols defined in `errno.h`. The compiler will warn you if there is any overlap in error code values.

Automatically propagating the exceptions up to the function's caller requires a minimally stylized program structure. Here is a function declaration:
```
/* return an exception type */
exc_type test(int i) {
    THROWS(EArgumentInvalid)

    if (i < 0) THROW(EArgumentInvalid)

    /* ... */

    DONE
}
```
Every function that uses exception handling returns an `exc_type`, and begins with a THROWS declaration, via which you may optionally declare the list of exceptions that may be thrown. The function then terminates with a DONE statement before the closing curly brace, and that's it.

Any exceptions thrown between these two statements propagate up to the caller via the return value. Assuming the caller is using this same exception handling structure, the exception will then propagate to a matching CATCH or CATCHANY block, or if nothing matches, propagate up to its caller.

# Convenience Macros #

Instead of performing the full NULL check as above, there are some convenience macros which provide the same semantics. The NULL check is performed by the MAYBE macro, so the above:
```
    if (NULL == (foo = (char*)malloc(123)) THROW(errno)
```
could be replaced with:
```
    MAYBE(foo = (char*)malloc(123), errno)
```
Similarly, the ERROR macro checks an expression's value against 0/ENoError, and throws it if it's non-zero:
```
    ERROR(ENoError) /* will not throw */
    ERROR(EOverlow) /* will throw EOverflow */
    ERROR(errno)    /* will throw errno if errno != 0 */

    ERRORE(errno, EUnrecoverable) /* throws EUnrecoverable if errno != 0 */
```
Some Windows-specific macros are in the works, but you can still process the standard HRESULT values now. Perhaps something like:
```
TRY() {
    ERROR(HRESULT_CODE( SomeFunction() ))
} IN {
...
```

# Efficiency #

These macros compile to a simple switch and/or direct branches, so error handling and finalization are as efficient as they can possibly be. There is no use of setjmp/longjmp, and it introduces no thread-safety issues since all state is kept in locals.

There may be a slight efficiency difference between the `_DEBUG` and `RELEASE` builds. The `_DEBUG` build has safer defaults which the `RELEASE` build elides, but this only eliminates roughly a single if-test, so the difference is probably negligible.

# Conditions #

  1. TRY(), IN, HANDLE, and FINALLY are all **mandatory** to use exceptions. Every other macro is optional.
  1. FINALLY **must** always come last, and must terminate every exception block. It can be empty though.
  1. You **cannot** use any control-flow operators, ie. goto, break, continue, return, that will **escape** an exception block. Any control-flow that stays within the same exception block is fine, with the following exception:
  1. You cannot execute a THROW or RETHROW within a loop or a switch. Use a flag to exit the loop, then throw the desired exception.
  1. If using exception handling in a function, you should not have code that executes outside of an exception handling block. All such code will execute unconditionally, as if it were in the FINALLY block. You may optionally specify ENDTRY after the FINALLY block if you wish to avoid this behaviour.


# Limitations #

  1. Does not provide stack traces (future work, opt-in).
  1. Provide an exception propagation form where the exception is propagated via errno, rather than returned directly (future work, opt-in). This allows returning values directly. The only change will likely be the DONE terminator.

# License #

My default license is LGPL, but I'm not sure how that interacts with the fact that the "library" consists solely of a header file, and thus has no runtime replaceable binary. I like the LGPL's conditions otherwise, so I'm open to other license suggestions that provide similar freedoms.

# Related Work #
  * [RAII in C](http://vilimpoc.org/research/raii-in-c/): demonstration that C can support RAII with a simple pattern. The only problem is that it's never clear at a glance that RAII is being used; you must interpret the context. libex simply codifies this pattern in a recognizable syntax to make the RAII pattern obvious.
  * [cexcept](http://www.nicemice.net/cexcept/) and [libexcept](http://libexcept.sourceforge.net/) libraries: use setjmp/longjmp to propagate exceptions. These are less efficient than the purely local exception handling used here, but they more easily support stack unwinding, at the expense of extra overhead to ensure thread-safety.
  * [exceptions4c](http://code.google.com/p/exceptions4c/): an extensive exception handling solution based on setjmp/longjmp, which supports exception subtyping, aka exception hierarchies, thread-safety, and signal handling.