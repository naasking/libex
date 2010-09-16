/*
 * Simple, local exception handling with binding forms.
 *
 * LICENSE: LGPL
 * Copyright 2010: Sandro Magi
 *
 * Examples:
 *
 * char *foo;
 * LET (foo = (char*)malloc(123)) {
 *   ... do something with foo
 * } CATCH (ENullRef) {
 *   ... handle error for foo
 * } FINALLY {
 *   ... finalize any state that has already been allocated
 * }
 *
 * TRY (returnExceptionType()) { ... }
 * CATCH (EBadDescriptor) { ... }
 * CATCH (EFileExists) { ... }
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
 *
 * FIXME:
 * # What to do for cases of possible duplicate error values? If they have the same value
 *   clients will get compile-time errors about duplicate cases. If they don't have the same
 *   value, we cannot distinguish between the cases. This only matters if errno could ever
 *   possibly take on either value after a single function call.
 * # Optionally intercept and handle signals? ie. catch div-by-zero and set errno.
 * # Better integration with C libs. There are 4 general cases, 2 specific:
 *   1. expression returns error code directly.        => TRY (E_exc_type) => switch(E_exc_type) ...
 *   2. expression returns success/fail == 0/-1 or !0. => ENSURE (E_int)   => switch(E_int == 0 ? ENoError : errno) ...
 *   3. expression returns success/fail == !NULL/NULL. => LET (E_void*)    => switch(E_void* ? ENoError : errno) ...
 *   4. set errno = 0, call function, check errno.     => CHECK (E_void)   => errno=0; (E_void); switch(errno) ...
 *   5. Windows-only: check HRESULT:				   => WTRY(HRESULT)    => switch(SUCCESS(HRESULT) ? ENoError : HRESULT_CODE(HRESULT)) ...
 *   NOTE: Windows system error codes can just use TRY: http://msdn.microsoft.com/en-us/library/ms681382.aspx
 * # Prevent user from forgetting the OTHERWISE branch?
 * # Microsoft has it's own type of return codes (maybe a HTRY for Windows only?):
 *   http://en.wikipedia.org/wiki/HRESULT
 *   http://msdn.microsoft.com/en-us/library/ms691242.aspx
 *   The HRESULT is definitely interesting, as it conveys significantly more information
 *   than simple return codes.
 * # OpenSSL allegedly has a sophisticated error system:
 *   http://landheer-cieslak.com/wordpress/error-handling-in-c/
 *   It uses a compile-time allocated error queue that stores various information.
 * # This approach can support early exits with THROW = break, assuming there's a top-level
 *   exc_type local in the function. THROW sets the local, then executes a break, which exits
 *   the switch early, execs the finalizer, and proceeds to the next bloc, which is a
 *   finalizer or a RETURN(X). This means there can be no code after an exception handler,
 *   because this code will assume successful completion of the block with no errors, which
 *   isn't the case. Each CATCH block must set exc_type=ENoError. Each TRY is wrapped in an
 *   conditional that first checks that exc_type=ENoError. This supports pre-condition checks
 *   which run before any exception, and may throw exceptions, ie.
 *     if (check) THROW(ArgumentInvalid);
 *   This may require a new terminator ENDTRY to close the conditional. Or perhaps can encode
 *   whether we are in an exception block at the time THROW was invoked via a bit in exc_type.
 *   If the bit is not set, FINALLY can simply exec: if (BIT(exc_type)) break; This doesn't
 *   handle the case of two exception blocks one after the other.
 * # Alternate approach: try-in-catch, where TRY-IN is wrapped in a do-while, and THROW simply breaks
 *   out of that loop to the exception handling block. TRY no longer takes an expression form.
 *   TRY Foo x = MakeFoo(); { ...foo...bar... if (x) THROW(ArgumentInvalid); ... }
 *   IN { ... }
 *   CATCH(ArgumentInvalid) { ... }
 *   FINALLY
 *
 *   RETHROW(E) can appear in the OTHERWISE block. Above compiles down to.
 *
 * = Traditional Exception Handling w/ unwinding and THROW =
 * -Each function has an EXC_BEGIN(T) which declares an "exc_type", and a value for the return
 *  type T. Alternately, provide a function definition macro, ie. RETURNS(T) DEF(fname, .../args/)
 *  which declares this local.
 * -Each exception block sets the exc_type local, and is wrapped in an infinite loop around
 *  the switch on that local.
 * -Each CATCH "breaks" from the loop.
 * -FINALLY is right after the loop.
 * -THROW sets the exc_type local and executes a "continue", which retries the exception
 *  handling switch. If matched, exec that block. If no match, set flag, and exec finalizer
 *  for cleanup which contains a test that checks the flag and execs a "continue" in
 *  the outer exception handling loop.
 * -Non-exceptional exits from blocks sets exc_type to ENoError.
 * -If at top-level, then return exc_type, perhaps using a RETURN(x) macro. This will have
 *  to be overloaded based on the 4 error cases typically used in C.
 */

#include <stdlib.h>
#include <limits.h>
#include <errno.h>

/*
 * Define meaningful names for all exception types based on standard POSIX:
 * http://www.opengroup.org/onlinepubs/9699919799/basedefs/errno.h.html
 */
typedef enum exc_type {
	ENullRef = (int)NULL,
	EEnsureViolated = 0,	/* ENSURE(B): B == zero when false */
	ETooManyArgs = E2BIG,
	EPermissionDenied = EACCES,
	EAddressInUse = EADDRINUSE,
	EAddressUnavailable = EADDRNOTAVAIL,
	EAddressFamilyUnsupported = EAFNOSUPPORT,
	EResourceUnavailable = EAGAIN, /* may == EWOULDBLOCK */
	EConnectionInProgress = EALREADY,
	EBadDescriptor = EBADF,
	EBadMessage = EBADMSG,
	EResourceBusy = EBUSY,
	ECanceled = ECANCELED,
	ENoChildProcesses = ECHILD,
	EConnectionAborted = ECONNABORTED,
	EConnectionRefused = ECONNREFUSED,
	EConnectionReset = ECONNRESET,
	EDeadlock = EDEADLK,
	EAddressRequired = EDESTADDRREQ,
	EOutOfRange = EDOM,
	/* Reserved: EDQUOT */
	EFileExists = EEXIST,
	EBadAddress = EFAULT,
	EFileTooBig = EFBIG,
	EUnreachable = EHOSTUNREACH,
	EIdentifierRemoved = EIDRM,
	EIllegalByteSequence = EILSEQ,
	EInProgress = EINPROGRESS,
	EInterrupted = EINTR,
	EArgumentInvalid = EINVAL,
	EIOError = EIO,
	EDisconnected = EISCONN,
	EIsDirectory = EISDIR,
	ETooManyLevels = ELOOP,
	EDescriptorTooBig = EMFILE,
	ETooManyLinks = EMLINK,
	EMessageTooBig = EMSGSIZE,
	/* Reserved: EMULTIHOP */
	ENameTooLong = ENAMETOOLONG,
	ENetworkDown = ENETDOWN,
	ENetworkAborted = ENETRESET,
	ENetworkUnreachable = ENETUNREACH,
	ETooManyOpenFiles = ENFILE,
	EBufferUnavailable = ENOBUFS,
#ifdef ENODATA
	ENoData = ENODATA,
#endif
	EDeviceNotFound = ENODEV,
	EPathNotFound = ENOENT,
	EInvalidExecutable = ENOEXEC,
	ENoLocks = ENOLCK,
	/* Reserved: ENOLINK */
	EOutOfMemory = ENOMEM,
	EMessageNotFound = ENOMSG,
	EProtocolUnavailable = ENOPROTOOPT,
	ENoSpaceOnDevice = ENOSPC,
#ifdef ENOSR
	ENoStreamResources = ENOSR,
#endif
#ifdef ENOSTR
	EInvalidStream = ENOSTR,
#endif
	EFunctionUnsupported = ENOSYS,
	ESocketNotConnected = ENOTCONN,
	EInvalidDirectory = ENOTDIR,
	EDirectoryNotEmpty = ENOTEMPTY,
	EUnrecoverable = ENOTRECOVERABLE,
	EInvalidSocket = ENOTSOCK,
	EUnsupported = ENOTSUP, /* may == EOPNOTSUPP */
	EInvalidIOControl = ENOTTY,
	EInvalidDeviceOrAddress = ENXIO,
	EInvalidSocketOp = EOPNOTSUPP, /* may == ENOTSUPP */
	EOverflow = EOVERFLOW,
	EOwnerUnvailable = EOWNERDEAD,
	EInvalidOp = EPERM,
	EBrokenPipe = EPIPE,
	EProtocolError = EPROTO,
	EProtocolUnsupported = EPROTONOSUPPORT,
	EProtocolInvalid = EPROTOTYPE,
	EResultTooBig = ERANGE,
	EReadOnly = EROFS,
	EInvalidSeek = ESPIPE,
	EProcessNotFound = ESRCH,
	/* Reserved: ESTALE */
#ifdef ETIME
	EStreamTimeout = ETIME,
#endif
	ETimedout = ETIMEDOUT,
	EFileBusy = ETXTBSY,
	EWouldBlock = EWOULDBLOCK, /* may == EAGAIN */
	ECrossDeviceLink = EXDEV,

	/* errno must be a positive value */
	EEarlyReturn= -1,
	ENoError = 0,
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
 * exception thrown by LET is ENullRef. Since NULL and ENullRef have the
 * same value, an assignment of NULL will branch to the ENullRef case.
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

/* THROWS(...) declares which exceptions may be thrown, and declares a local to
 * store the current exception. */
#define THROWS(...) exc_type THROWS;
//#define TRY(D) do { THROWS = ENoError; { D; do {
#define TRY(D) do { THROWS = ENoError; { D; do
#define IN while(0); if (THROWS == ENoError)
#define HANDLE } switch(THROWS) { case ENoError: case EEarlyReturn: break;
#define CATCH(e) EXC_CASE(case e)
#define OTHERWISE EXC_CASE(default)
#define EXC_CASE(E) THROWS = ENoError; break; E:
#define FINALLY break; } } while(0);
#define THROW(E) { THROWS = (exc_type)(E); break; }

#define RETURN THROW(EEarlyReturn)
#define EXIT return THROWS;
//#define ERRNO_RETURN(X) do { if (THROWS != ENoError) { errno = THROWS; return -1; } else return 0; } while(0)

//#define DO THROWS =
//#define _ ; if (THROWS != ENoError) RETHROW;
//#define DO(E) if ((THROWS = (E)) != ENoError) RETHROW;
#define MAYBE(E, R) if (NULL == (E)) THROW(R);

//TRY DO (callFoo())
//	if (B) THROW(E)
//	DO (callBar())
//IN {
//
//} HANDLE
//  CATCH (E) {
//}

// old, simple technique
//#define TRY(X) _EXCOPEN(X) CATCH(ENoError)
//#define _EXCOPEN(X) do { int __ex = (X); switch (__ex) {
//#define LET(X) _EXCOPEN(X) default:
//#define ENSURE(X) LET(X)
//#define CATCH(e) break; case e:
//#define OTHERWISE default:
//#define FINALLY break; } } while(0);

/* this must appear at the beginning of any function that uses exceptions */
//#define USE_EXC unsigned USE_EXC

/*
 * Translate an errno expression into an exception.
 * X: an expression that sets errno.
 * C: conditional that determines whether errno was set.
 */
#define TRYE(X,C) (X); TRY((C) ? ENoError : errno)

#endif /*__LIBEX__*/
