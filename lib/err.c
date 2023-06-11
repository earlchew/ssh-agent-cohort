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

#include "err.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/******************************************************************************/
int debug_;

const char DebugEnable[1];
const char DebugDisable[1];

/******************************************************************************/
static void
alert_(const char *aFmt, const char *aLevel, int errCode, va_list argp)
{
    fprintf(stderr, "%s: [%d] ", ARGV0, getpid());
    if (aLevel)
        fprintf(stderr, "%s - ", aLevel);
    vfprintf(stderr, aFmt, argp);

    if (errCode) {
        const char *errText = strerror(errCode);

        fprintf(stderr, " [%d - %s]", errCode, errText);
    }

    fprintf(stderr, "\n");
}

/*----------------------------------------------------------------------------*/
static void
errorv_(const char *aFmt, va_list argp)
{
    int errCode = errno;

    alert_(aFmt, "ERROR", errCode, argp);

    errno = errCode;
}

/******************************************************************************/
void
warn(const char *aFmt, ...)
{
    int errCode = errno;

    va_list argp;

    va_start(argp, aFmt);
    alert_(aFmt, "WARN", errCode, argp);
    va_end(argp);

    errno = errCode;
}

/******************************************************************************/
void
error(const char *aFmt, ...)
{
    va_list argp;

    va_start(argp, aFmt);
    errorv_(aFmt, argp);
    va_end(argp);
}

/******************************************************************************/
void
fatal(const char *aFmt, ...)
{
    va_list argp;

    va_start(argp, aFmt);
    errorv_(aFmt, argp);
    va_end(argp);
    exit(EXIT_FAILURE);
}

/******************************************************************************/
void
debug(const char *aFmt, ...)
{
    va_list argp;

    va_start(argp, aFmt);

    do {
        if (!strcmp(aFmt, "%s")) {

            const char *action = va_arg(argp, const char*);

            if (DebugEnable == action) {
                debug_ = 1;
                break;
            }
            else if (DebugDisable == action) {
                debug_ = 0;
                break;
            }

            va_end(argp);
            va_start(argp, aFmt);
        }

        alert_(aFmt, "DEBUG", 0, argp);

    } while (0);

    va_end(argp);
}

/******************************************************************************/
void
die(const char *aFmt, ...)
{
    va_list argp;

    va_start(argp, aFmt);
    alert_(aFmt, 0, 0, argp);
    va_end(argp);

    exit(EXIT_FAILURE);
}

/******************************************************************************/
void
help(const char *aUsage, int aSummary)
{
    if (aSummary)
        fprintf(stderr, "usage: %s %s\n", ARGV0, aUsage);
    else {
        const char *usageLine = strchr(aUsage, '\n');

        if (!usageLine)
            usageLine = strchr(aUsage, 0);

        size_t helpLen = usageLine - aUsage;

        char helpText[helpLen+1];

        memcpy(helpText, aUsage, helpLen);
        helpText[helpLen] = 0;

        fprintf(stderr, "usage: %s %s\n", ARGV0, helpText);
    }

    exit(EXIT_FAILURE);
}

/******************************************************************************/
