#ifndef MACROS_H_
#define MACROS_H_
/**
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/******************************************************************************/
#define NUMBEROF(Array) ( sizeof((Array))/sizeof((Array)[0]) )

/******************************************************************************/
#define DEBUG(...) do { if (!debug_) { } else debug(__VA_ARGS__); } while (0)

/******************************************************************************/
#define FREE(Ptr) do { free((Ptr)); (Ptr) = 0; } while (0)

/******************************************************************************/
#include <errno.h>
#define FINALLY(...)                 \
    do {                             \
        int errno_ = errno;          \
        do { __VA_ARGS__ } while(0); \
        errno = errno_;              \
    } while (0)

/******************************************************************************/
#ifdef __GNUC__
#define PRINTF_FORMAT(Fmt, Args) \
    __attribute__((__format__ (__printf__, Fmt, Args)))
#else
#define PRINTF_FORMAT(Fmt, Args)
#endif

/******************************************************************************/
#include <limits.h>

#ifdef __GNU_LIBRARY__
#include <errno.h>
#define ARGV0 (program_invocation_short_name)
#else
#include <stdlib.h>
#define ARGV0 (getprogname())
#endif

/******************************************************************************/

#endif
