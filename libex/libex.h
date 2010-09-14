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
 * # what to do for cases of possible duplicate error values? If they have the same value
 *   clients will get compile-time errors about duplicate cases. If they don't have the same
 *   value, we cannot distinguish between the cases. This only matters if errno could ever
 *   possibly take on either value after a single function call.
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
	ENoError = INT_MIN,
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
#define TRY(X) _EXCOPEN(X) CATCH(ENoError)
#define _EXCOPEN(X) do { int __ex = (X); switch (__ex) {
#define LET(X) _EXCOPEN(X) default:
#define ENSURE(X) LET(X)
#define CATCH(e) break; case e:
#define OTHERWISE default:
#define FINALLY break; } } while(0);

/*
 * Translate an errno expression into an exception.
 * X: an expression that sets errno.
 * C: conditional that determines whether errno was set.
 */
#define TRYE(X,C) (X); TRY((C) ? ENoError : errno)

#endif /*__LIBEX__*/
