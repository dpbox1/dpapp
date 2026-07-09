/** @file dpret.h
 *  @ingroup dpapp_pret
 *  @brief 统一错误码与返回值约定。
 *
 *  约定：负值为错误；非负为成功或其他非错误语义（如读写字节数）。
 *
 *  区间：
 *  - [0,150)：系统 errno（来自系统头文件）
 *  - [150,190)：dpapp 专用错误（可能扩展）
 *  - [190,200)：HTTP 1xx 映射区（避免与 errno 冲突）
 *  - [200,600)：标准 HTTP 状态码映射
 *
 *  `dperr_detail()` 返回已定义错误码的描述；
 *  `dperr_http_detail()` 将 HTTP 风格状态（含 \>0 与 \<-190）映射为可读文本。
 *
 *  注意：`DPE_CONTINUE`（-190）同时用于 HTTP 100 Continue 与异步 I/O
 *  「需继续等待/部分完成」语义（如 `dpaio_read_until` 未找到分隔符）。 */
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

/** @brief 有符号状态码：负值为 `DPE_*` 错误，非负为成功或字节数等语义。 */
typedef int dpret_t;

/** @name 系统 errno 映射 */
/**@{*/
#define DPE_OK     0          /* OK */
#define DPE_PERM   (-EPERM)   /* Operation not permitted */
#define DPE_NOENT  (-ENOENT)  /* No such file or directory */
#define DPE_SRCH   (-ESRCH)   /* No such process */
#define DPE_INTR   (-EINTR)   /* Interrupted system call */
#define DPE_IO     (-EIO)     /* I/O error */
#define DPE_NXIO   (-ENXIO)   /* No such device or address */
#define DPE_2BIG   (-E2BIG)   /* Argument list too long */
#define DPE_NOEXEC (-ENOEXEC) /* Exec format error */
#define DPE_BADF   (-EBADF)   /* Bad file number */
#define DPE_CHILD  (-ECHILD)  /* No child processes */
#define DPE_AGAIN  (-EAGAIN)  /* Try again */
#define DPE_NOMEM  (-ENOMEM)  /* Out of memory */
#define DPE_ACCES  (-EACCES)  /* Permission denied */
#define DPE_FAULT  (-EFAULT)  /* Bad address */
#define DPE_NOTBLK (-ENOTBLK) /* Block device required */
#define DPE_BUSY   (-EBUSY)   /* Device or resource busy */
#define DPE_EXIST  (-EEXIST)  /* File exists */
#define DPE_XDEV   (-EXDEV)   /* Cross-device link */
#define DPE_NODEV  (-ENODEV)  /* No such device */
#define DPE_NOTDIR (-ENOTDIR) /* Not a directory */
#define DPE_ISDIR  (-EISDIR)  /* Is a directory */
#define DPE_INVAL  (-EINVAL)  /* Invalid argument */
#define DPE_NFILE  (-ENFILE)  /* File table overflow */
#define DPE_MFILE  (-EMFILE)  /* Too many open files */
#define DPE_NOTTY  (-ENOTTY)  /* Not a typewriter */
#define DPE_TXTBSY (-ETXTBSY) /* Text file busy */
#define DPE_FBIG   (-EFBIG)   /* File too large */
#define DPE_NOSPC  (-ENOSPC)  /* No space left on device */
#define DPE_SPIPE  (-ESPIPE)  /* Illegal seek */
#define DPE_ROFS   (-EROFS)   /* Read-only file system */
#define DPE_MLINK  (-EMLINK)  /* Too many links */
#define DPE_PIPE   (-EPIPE)   /* Broken pipe */
#define DPE_DOM    (-EDOM)    /* Math argument out of domain of func */
#define DPE_RANGE  (-ERANGE)  /* Math result not representable */

#define DPE_DEADLK      (-EDEADLK)      /* Resource deadlock would occur */
#define DPE_NAMETOOLONG (-ENAMETOOLONG) /* File name too long */
#define DPE_NOLCK       (-ENOLCK)       /* No record locks available */

/*
 * This error code is special: arch syscall entry code will return
 * -ENOSYS if users try to call a syscall that doesn't exist.  To keep
 * failures of syscalls that really do exist distinguishable from
 * failures due to attempts to use a nonexistent syscall, syscall
 * implementations should refrain from returning -ENOSYS.
 */
#define DPE_NOSYS (-ENOSYS) /* Invalid system call number */

#define DPE_NOTEMPTY   (-ENOTEMPTY) /* Directory not empty */
#define DPE_LOOP       (-ELOOP)     /* Too many symbolic links encountered */
#define DPE_WOULDBLOCK (-EAGAIN)    /* Operation would block */
#define DPE_NOMSG      (-ENOMSG)    /* No message of desired type */
#define DPE_IDRM       (-EIDRM)     /* Identifier removed */
#define DPE_CHRNG      (-ECHRNG)    /* Channel number out of range */
#define DPE_L2NSYNC    (-EL2NSYNC)  /* Level 2 not synchronized */
#define DPE_L3HLT      (-EL3HLT)    /* Level 3 halted */
#define DPE_L3RST      (-EL3RST)    /* Level 3 reset */
#define DPE_LNRNG      (-ELNRNG)    /* Link number out of range */
#define DPE_UNATCH     (-EUNATCH)   /* Protocol driver not attached */
#define DPE_NOCSI      (-ENOCSI)    /* No CSI structure available */
#define DPE_L2HLT      (-EL2HLT)    /* Level 2 halted */
#define DPE_BADE       (-EBADE)     /* Invalid exchange */
#define DPE_BADR       (-EBADR)     /* Invalid request descriptor */
#define DPE_XFULL      (-EXFULL)    /* Exchange full */
#define DPE_NOANO      (-ENOANO)    /* No anode */
#define DPE_BADRQC     (-EBADRQC)   /* Invalid request code */
#define DPE_BADSLT     (-EBADSLT)   /* Invalid slot */

#define DPE_DEADLOCK (-EDEADLK)

#define DPE_BFONT          (-EBFONT)    /* Bad font file format */
#define DPE_NOSTR          (-ENOSTR)    /* Device not a stream */
#define DPE_NODATA         (-ENODATA)   /* No data available */
#define DPE_TIME           (-ETIME)     /* Timer expired */
#define DPE_NOSR           (-ENOSR)     /* Out of streams resources */
#define DPE_NONET          (-ENONET)    /* Machine is not on the network */
#define DPE_NOPKG          (-ENOPKG)    /* Package not installed */
#define DPE_REMOTE         (-EREMOTE)   /* Object is remote */
#define DPE_NOLINK         (-ENOLINK)   /* Link has been severed */
#define DPE_ADV            (-EADV)      /* Advertise error */
#define DPE_SRMNT          (-ESRMNT)    /* Srmount error */
#define DPE_COMM           (-ECOMM)     /* Communication error on send */
#define DPE_PROTO          (-EPROTO)    /* Protocol error */
#define DPE_MULTIHOP       (-EMULTIHOP) /* Multihop attempted */
#define DPE_DOTDOT         (-EDOTDOT)   /* RFS specific error */
#define DPE_BADMSG         (-EBADMSG)   /* Not a data message */
#define DPE_OVERFLOW       (-EOVERFLOW) /* Value too large for defined data type */
#define DPE_NOTUNIQ        (-ENOTUNIQ)  /* Name not unique on network */
#define DPE_BADFD          (-EBADFD)    /* File descriptor in bad state */
#define DPE_REMCHG         (-EREMCHG)   /* Remote address changed */
#define DPE_LIBACC         (-ELIBACC)   /* Can not access a needed shared library */
#define DPE_LIBBAD         (-ELIBBAD)   /* Accessing a corrupted shared library */
#define DPE_LIBSCN         (-ELIBSCN)   /* .lib section in a.out corrupted */
#define DPE_LIBMAX         (-ELIBMAX) /* Attempting to link in too many shared libraries */
#define DPE_LIBEXEC        (-ELIBEXEC) /* Cannot exec a shared library directly */
#define DPE_ILSEQ          (-EILSEQ)   /* Illegal byte sequence */
#define DPE_RESTART        (-ERESTART) /* Interrupted system call should be restarted */
#define DPE_STRPIPE        (-ESTRPIPE)        /* Streams pipe error */
#define DPE_USERS          (-EUSERS)          /* Too many users */
#define DPE_NOTSOCK        (-ENOTSOCK)        /* Socket operation on non-socket */
#define DPE_DESTADDRREQ    (-EDESTADDRREQ)    /* Destination address required */
#define DPE_MSGSIZE        (-EMSGSIZE)        /* Message too long */
#define DPE_PROTOTYPE      (-EPROTOTYPE)      /* Protocol wrong type for socket */
#define DPE_NOPROTOOPT     (-ENOPROTOOPT)     /* Protocol not available */
#define DPE_PROTONOSUPPORT (-EPROTONOSUPPORT) /* Protocol not supported */
#define DPE_SOCKTNOSUPPORT (-ESOCKTNOSUPPORT) /* Socket type not supported */
#define DPE_OPNOTSUPP                                                               \
    (-OPNOTSUPP) /* Operation not supported on transport endpoint */
#define DPE_PFNOSUPPORT (-EPFNOSUPPORT) /* Protocol family not supported */
#define DPE_AFNOSUPPORT                                                             \
    (-EAFNOSUPPORT)                    /* Address family not supported by protocol */
#define DPE_ADDRINUSE    (-EADDRINUSE) /* Address already in use */
#define DPE_ADDRNOTAVAIL (-EADDRNOTAVAIL) /* Cannot assign requested address */
#define DPE_NETDOWN      (-ENETDOWN)      /* Network is down */
#define DPE_NETUNREACH   (-ENETUNREACH)   /* Network is unreachable */
#define DPE_NETRESET     (-ENETRESET) /* Network dropped connection because of reset */
#define DPE_CONNABORTED  (-ECONNABORTED) /* Software caused connection abort */
#define DPE_CONNRESET    (-ECONNRESET)   /* Connection reset by peer */
#define DPE_NOBUFS       (-ENOBUFS)      /* No buffer space available */
#define DPE_ISCONN       (-EISCONN)  /* Transport endpoint is already connected */
#define DPE_NOTCONN      (-ENOTCONN) /* Transport endpoint is not connected */
#define DPE_SHUTDOWN     (-SHUTDOWN) /* Cannot send after transport endpoint shutdown*/
#define DPE_TOOMANYREFS  (-ETOOMANYREFS) /* Too many references: cannot splice */
#define DPE_TIMEDOUT     (-ETIMEDOUT)    /* Connection timed out */
#define DPE_CONNREFUSED  (-ECONNREFUSED) /* Connection refused */
#define DPE_HOSTDOWN     (-EHOSTDOWN)    /* Host is down */
#define DPE_HOSTUNREACH  (-EHOSTUNREACH) /* No route to host */
#define DPE_ALREADY      (-EALREADY)     /* Operation already in progress */
#define DPE_INPROGRESS   (-EINPROGRESS)  /* Operation now in progress */
#define DPE_STALE        (-ESTALE)       /* Stale file handle */
#define DPE_UCLEAN       (-EUCLEAN)      /* Structure needs cleaning */
#define DPE_NOTNAM       (-ENOTNAM)      /* Not a XENIX named type file */
#define DPE_NAVAIL       (-ENAVAIL)      /* No XENIX semaphores available */
#define DPE_ISNAM        (-EISNAM)       /* Is a named type file */
#define DPE_REMOTEIO     (-EREMOTEIO)    /* Remote I/O error */
#define DPE_DQUOT        (-EDQUOT)       /* Quota exceeded */

#define DPE_NOMEDIUM    (-ENOMEDIUM)    /* No medium found */
#define DPE_MEDIUMTYPE  (-EMEDIUMTYPE)  /* Wrong medium type */
#define DPE_CANCELED    (-ECANCELED)    /* Operation Canceled */
#define DPE_NOKEY       (-ENOKEY)       /* Required key not available */
#define DPE_KEYEXPIRED  (-EKEYEXPIRED)  /* Key has expired */
#define DPE_KEYREVOKED  (-EKEYREVOKED)  /* Key has been revoked */
#define DPE_KEYREJECTED (-EKEYREJECTED) /* Key was rejected by service */

/* for robust mutexes */
#define DPE_OWNERDEAD      (-EOWNERDEAD)      /* Owner died */
#define DPE_NOTRECOVERABLE (-ENOTRECOVERABLE) /* State not recoverable */
#define DPE_RFKILL         (-ERFKILL)   /* Operation not possible due to RF-kill */
#define DPE_HWPOISON       (-EHWPOISON) /* Memory page has hardware error */

#define DPE_PARSE (-EPROTO) // Parse error
#define DPE_WAIT  (-EAGAIN) // Wait the next event
/** @} */

/** @name dpapp 专用错误码 */
/**@{*/

/* 15x Custom Errors */
#define DPE_UNKNOWN   (-150) // Unknown Error
#define DPE_REPEAT    (-151) // Repeat Operation
#define DPE_UNINIT    (-152) // Not initialized
#define DPE_OPEN      (-153) // Open Failed
#define DPE_CLOSED    (-154) // File or source closed
#define DPE_SETATTR   (-155) // Set Attribute Failed
#define DPE_EOF       (-156) // End of source or file
#define DPE_NOTEXISTS (-157) // Not exists
#define DPE_INITED    (-158) // Already initialized
#define DPE_BEINITED  (-159) // Initialization due to Invalid parameter
#define DPE_PARAMTYPE (-160) // The parameter type mismatch
#define DPE_PARAMMISS (-166) // Missing parameter
#define DPE_PREPARE   (-167) // Prepareing or bind parameter error
#define DPE_NOTAUTH   (-168) // Need authentication
#define DPE_NOTOPEN   (-169) // Not open source
#define DPE_INVCMD    (-170) // Invalid command
#define DPE_AUTHED    (-171) // Already authenticated
#define DPE_DATATYPE  (-172) // Invalid data type
#define DPE_NOEVENT   (-173) // Listen no events
#define DPE_AUTHERR   (-174) // Authentication error
#define DPE_UNSUPPORT (-175) // Not support
#define DPE_NOSOURCE  (-176) // No source
#define DPE_DELETED   (-177) // Object or source deleted
#define DPE_NOTENOUGH (-178) // Not enough data or buffer space
#define DPE_PARTIALOK (-179) // Partially OK
#define DPE_IGNORE    (-180) // Ignore the operation

/** @} */

/** @name HTTP 状态码映射 */
/**@{*/

// Add http error code extenstion
// https://developer.mozilla.org/en-US/docs/Web/HTTP/Status#information_responses

/* 1xx Informational (adjusted to 19x range) */
#define DPE_CONTINUE            (-190) // Continue
#define DPE_SWITCHING_PROTOCOLS (-191) // Switching Protocols
#define DPE_PROCESSING          (-192) // Processing
#define DPE_EARLY_HINTS         (-193) // Early Hints

/* 2xx Success */
#define DPE_RESOK             (-200) // OK
#define DPE_CREATED           (-201) // Created
#define DPE_ACCEPTED          (-202) // Accepted
#define DPE_NON_AUTHORITATIVE (-203) // Non-Authoritative Information
#define DPE_NO_CONTENT        (-204) // No Content
#define DPE_RESET_CONTENT     (-205) // Reset Content
#define DPE_PARTIAL_CONTENT   (-206) // Partial Content
#define DPE_MULTI_STATUS      (-207) // Multi-Status
#define DPE_ALREADY_REPORTED  (-208) // Already Reported
#define DPE_IM_USED           (-226) // IM Used

/* 3xx Redirection */
#define DPE_MULTIPLE_CHOICES   (-300) // Multiple Choices
#define DPE_MOVED_PERMANENTLY  (-301) // Moved Permanently
#define DPE_FOUND              (-302) // Found
#define DPE_SEE_OTHER          (-303) // See Other
#define DPE_NOT_MODIFIED       (-304) // Not Modified
#define DPE_USE_PROXY          (-305) // Use Proxy
#define DPE_TEMPORARY_REDIRECT (-307) // Temporary Redirect
#define DPE_PERMANENT_REDIRECT (-308) // Permanent Redirect

/* 4xx Client Errors */
#define DPE_BAD_REQUEST         (-400) // Bad Request
#define DPE_UNAUTHORIZED        (-401) // Unauthorized
#define DPE_PAYMENT_REQUIRED    (-402) // Payment Required
#define DPE_FORBIDDEN           (-403) // Forbidden
#define DPE_NOT_FOUND           (-404) // Not Found
#define DPE_METHOD_NOT_ALLOWED  (-405) // Method Not Allowed
#define DPE_NOT_ACCEPTABLE      (-406) // Not Acceptable
#define DPE_PROXY_AUTH_REQUIRED (-407) // Proxy Authentication Required
#define DPE_REQUEST_TIMEOUT     (-408) // Request Timeout
#define DPE_CONFLICT            (-409) // Conflict
#define DPE_GONE                (-410) // Gone
#define DPE_LENGTH_REQUIRED     (-411) // Length Required
#define DPE_PRECONDITION_FAILED (-412) // Precondition Failed
#define DPE_POST_TOO_LARGE      (-413) // Payload Too Large
#define DPE_URI_TOO_LONG        (-414) // URI Too Long
#define DPE_UNSUPPORTED_MEDIA   (-415) // Unsupported Media Type
#define DPE_RANGE_INVALID       (-416) // Range Not Satisfiable
#define DPE_EXPECTATION_FAILED  (-417) // Expectation Failed
#define DPE_TEAPOT              (-418) // I'm a teapot
#define DPE_UNPROCESSABLE       (-422) // Unprocessable Entity
#define DPE_TOO_EARLY           (-425) // Too Early
#define DPE_UPGRADE_REQUIRED    (-426) // Upgrade Required
#define DPE_PRECONDITION_REQ    (-428) // Precondition Required
#define DPE_TOO_MANY_REQUESTS   (-429) // Too Many Requests
#define DPE_HEADER_TOO_LARGE    (-431) // Request Header Fields Too Large
#define DPE_LEGAL_UNAVAILABLE   (-451) // Unavailable For Legal Reasons

/* 5xx Server Errors */
#define DPE_INTERNAL_ERROR       (-500) // Internal Server Error
#define DPE_NOT_IMPLEMENTED      (-501) // Not Implemented
#define DPE_BAD_GATEWAY          (-502) // Bad Gateway
#define DPE_SERVICE_UNAVAILABLE  (-503) // Service Unavailable
#define DPE_GATEWAY_TIMEOUT      (-504) // Gateway Timeout
#define DPE_HTTP_NOT_SUPPORTED   (-505) // HTTP Version Not Supported
#define DPE_VARIANT_NEGOTIATES   (-506) // Variant Also Negotiates
#define DPE_INSUFFICIENT_STORAGE (-507) // Insufficient Storage
#define DPE_LOOP_DETECTED        (-508) // Loop Detected
#define DPE_NOT_EXTENDED         (-510) // Not Extended
#define DPE_NETWORK_AUTH_REQ     (-511) // Network Authentication Required

/** @} */

/** @brief 返回错误码 `err` 的人类可读描述。 */
const char* dperr_detail(int err);

/** @brief 将 HTTP 状态（含 1xx 的 19x 映射区）转为可读描述字符串。 */
const char* dperr_http_detail(int http_status);

/** @brief 判断 `err` 非负（成功或非错误语义，如读写字节数）。 */
#define dpret_isok(err) ((err) >= 0)

/** @brief 判断 `err` 为负（错误）。 */
#define dpret_iserr(err) ((err) < 0)

/** @brief 判断成功或流结束（`err` 非负或等于 `DPE_EOF`）。 */
#define dpret_isok_eof(err) ((err) >= 0 || (err) == DPE_EOF)

/** @brief 布尔式成功返回值（与返回 0/1 的 API 配合）。 */
#define DPE_TRUE 1

/** @brief 布尔式失败返回值。 */
#define DPE_FALSE 0

#ifdef __cplusplus
}
#endif
