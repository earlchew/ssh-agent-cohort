#ifndef ERR_H_
#define ERR_H_
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

#include "macros.h"

extern int debug_;

extern const char DebugEnable[];
extern const char DebugDisable[];

PRINTF_FORMAT(1, 2)
void debug(const char *aFmt, ...);

PRINTF_FORMAT(1, 2)
void warn(const char *aFmt, ...);

PRINTF_FORMAT(1, 2)
void error(const char *aFmt, ...);

PRINTF_FORMAT(1, 2)
void fatal(const char *aFmt, ...);

PRINTF_FORMAT(1, 2)
void die(const char *aFmt, ...);

void help(const char *aText, int aSummary);

#endif
