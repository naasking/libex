/*
 * Simple, local exception handling with binding forms.
 *
 * LICENSE: LGPL
 * Copyright 2010: Sandro Magi
 *
 * Example:
 *
 * TRY(char *foo) {
 *     MAYBE(foo = (char*)malloc(123), errno)
 * } IN {
 *   ... do something with foo
 * } HANDLE CATCH (EOutOfMemory) {
 *   ... handle error for foo
 * } FINALLY {
 *   ... finalize any state that has already been allocated
 * }
 *
 * The thrown exception can be any sort of error code of integer type, including
 * errno, and Windows HRESULT.
 *
 * NOTES:
 * # The full TRY(), IN, HANDLE, and FINALLY are all *mandatory*. Everything else is optional.
 * # FINALLY *must* always come last, and must terminate every exception block.
 * # You *cannot* use any control-flow operators, ie. goto, break, continue,
 *   return, that will *escape* an exception block. Any control-flow that stays
 *   within the same exception block is fine, with the following exception:
 * # You cannot execute a THROW or RETHROW within a loop or a switch. Use a
 *   flag to exit the loop, then throw the desired exception.
 */

#ifndef __LIBEX__
#define __LIBEX__

/*
 * FUTURE WORK:
 * 1. Compile-time conditional to log the current file, function, and line # on exception.
 *
 * FIXME:
 * # What to do for cases of possible duplicate error values? If they have the same value
 *   clients will get compile-time errors about duplicate cases. If they don't have the same
 *   value, we cannot distinguish between the cases. This only matters if errno could ever
 *   possibly take on either value after a single function call.
 * # Optionally intercept and handle signals? ie. catch div-by-zero and set errno. Register the
 *   signal handler at init time, and just use the ERROR() macro.
 * # Better integration with C libs. There are 4 general cases, 2 specific:
 *   1. expression returns error code directly.        => TRY (E_exc_type) => switch(E_exc_type) ...
 *   2. expression returns success/fail == 0/-1 or !0. => ENSURE (E_int)   => switch(E_int == 0 ? ENoError : errno) ...
 *   3. expression returns success/fail == !NULL/NULL. => LET (E_void*)    => switch(E_void* ? ENoError : errno) ...
 *   4. set errno = 0, call function, check errno.     => CHECK (E_void)   => errno=0; (E_void); switch(errno) ...
 *   5. Windows-only: check HRESULT:				   => WTRY(HRESULT)    => switch(SUCCESS(HRESULT) ? ENoError : HRESULT_CODE(HRESULT)) ...
 *   NOTE: Windows system error codes can just use TRY: http://msdn.microsoft.com/en-us/library/ms681382.aspx
 * # Prevent user from forgetting the CATCHANY branch?
 * # Microsoft has it's own type of return codes (maybe a HTRY for Windows only?):
 *   http://en.wikipedia.org/wiki/HRESULT
 *   http://msdn.microsoft.com/en-us/library/ms691242.aspx
 *   The HRESULT is definitely interesting, as it conveys significantly more information
 *   than simple return codes.
 * # OpenSSL allegedly has a sophisticated error system:
 *   http://landheer-cieslak.com/wordpress/error-handling-in-c/
 *   It uses a compile-time allocated error queue that stores various information.
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
 * FINALLY terminates the exception handling block and must always appear, even
 * if no finalization code is necessary.
 */

/* THROWS(...) declares which exceptions may be thrown. It's purely for
 * documentation purposes, and declares a hidden local to store the current
 * exception, and a function-scope loop used for exception propagation. */
#define THROWS(...) exc_type THROWS = ENoError; do {

/* DONE designates the end of a function block, where the exception is returned */
#define DONE } while(0); return THROWS == EEarlyReturn ? ENoError : THROWS

/* RETURN throws an EEarlyReturn exception which propagates up to the caller */
#define RETURN THROW(EEarlyReturn)

/* A RETRY operation restarts the entire block again. It would *not* run the
 * finalizers, and would in fact re-run the TRY block thus re-allocating
 * resources. Probably not a good idea. */
/*#define RETRY continue*/

/* THROW raises an exception to an enclosing CATCH */
/* FIXME: This is the ugliest part of the package, because I either force every
 * use of THROW to be wrapped in {}, or I forbid users from terminating with
 * a semi-colon ; contrary to typical C style. I can't wrap in do-while because
 * "break" must break out of the *outer* loop.
#define THROW(E) { THROWS = (exc_type)(E); break; }

/* RETHROW re-raises the current exception in the parent scope */
#define RETHROW break
#define EXC_CASE(E) THROWS = ENoError; break; E:
#define __CUR_EXC__ THROWS

/* CATCH and CATCHANY specify handlers for various error types */
#define CATCH(e) EXC_CASE(case e)
#define CATCHANY EXC_CASE(default)

/* FINALLY closes the scope of the exception handling block and designates
 * the start of code that finalizes any allocated resources */
#define FINALLY break; } } while(0); }

/* MAYBE raises the exception R if E evaluates to NULL */
#define MAYBE(E, R) if (NULL == (E)) THROW(R);

/* ERROR raises the exception E if E evaluates to something other than ENoError */
#define ERROR(E) THROWS = (E); if (THROWS != ENoError) RETHROW;

/* ERRORE raises the exception R if E evaluates to non-zero */
#define ERRORE(E, R) if ((E)) THROW(R)

/* TRY begins the exception handling scope, and accepts a list of declarations D.
 * IN designates the scope which executes if no errors were raised in the
 * TRY scope.
 * HANDLE designates the start of the error handlers themselves. */

#ifdef _DEBUG

#define TRY(D) { if (THROWS != ENoError) RETHROW; do { { D; do
#define IN while(0); if (THROWS == ENoError)
#define HANDLE } switch(THROWS) { case ENoError: case EEarlyReturn: break;
/* optionally deprecate HANDLE by requiring CATCHANY after IN */
//#define CATCHANY } switch(THROWS) { case ENoError: case EEarlyReturn: break; EXC_CASE(default)

#else

/* The only real difference between the DEBUG and RELEASE implementations
 * is that the latter has one less if-test and one less lexical scope. So
 * unlike DEBUG mode, all CATCH clauses can see the bindings introduced in
 * the TRY block. */

#define TRY(D) { if (THROWS != ENoError) RETHROW; { D; do 
#define IN while (0); switch (THROWS) { case ENoError: 
#define HANDLE break; case EEarlyReturn: break;
/* optionally deprecate HANDLE by requiring CATCHANY after IN */
//#define CATCHANY break; case EEarlyReturn: break; EXC_CASE(default)

#endif /*_DEBUG*/

#endif /*__LIBEX__*/
