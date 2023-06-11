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

#include "fd.h"

#include "err.h"

#include "macros.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

/******************************************************************************/
int
fd_cloexec(int aFd)
{
    int rc = -1;

    int arg;

    arg = fcntl(aFd, F_GETFD);
    if (-1 == arg)
        goto Finally;

    arg |= FD_CLOEXEC;

    if (-1 == fcntl(aFd, F_SETFD, arg))
        goto Finally;

    rc = 0;

Finally:

    return rc;
}

/*----------------------------------------------------------------------------*/
int
fd_nonblock(int aFd)
{
    int rc = -1;

    int arg;

    arg = fcntl(aFd, F_GETFL);
    if (-1 == arg)
        goto Finally;

    arg |= O_NONBLOCK;

    if (-1 == fcntl(aFd, F_SETFL, arg))
        goto Finally;

    rc = 0;

Finally:

    return rc;
}

/*----------------------------------------------------------------------------*/
int
fd_close(int aFd)
{
    if (-1 != aFd)
        close(aFd);

    return -1;
}

/*----------------------------------------------------------------------------*/
ssize_t
fd_write(int aFd, const char *aBuf, ssize_t aLen)
{
    int rc = -1;

    const char *bufPtr = aBuf;
    ssize_t     bufLen = aLen;

    while (bufLen) {
        ssize_t writeLen = write(aFd, bufPtr, bufLen);
        if (-1 == writeLen) {
            if (EINTR == errno)
                continue;
            if (bufPtr != aBuf)
                break;
            goto Finally;
        }

        if (0 == writeLen)
            break;

        if (writeLen > bufLen)
            die("File descriptor %d write %lu overrunning buffer length %lu",
                aFd, (unsigned long) writeLen, (unsigned long) bufLen);

        bufPtr += writeLen;
        bufLen -= writeLen;
    }

    rc = 0;

Finally:

    return rc ? -1 : bufPtr - aBuf;
}

/*----------------------------------------------------------------------------*/
ssize_t
fd_read(int aFd, char *aBuf, ssize_t aLen)
{
    int rc = -1;

    char   *bufPtr = aBuf;
    ssize_t bufLen = aLen;

    while (bufLen) {
        ssize_t readLen = read(aFd, bufPtr, bufLen);
        if (-1 == readLen) {
            if (EINTR == errno)
                continue;
            if (bufPtr != aBuf)
                break;
            goto Finally;
        }

        if (0 == readLen)
            break;

        if (readLen > bufLen)
            die("File descriptor %d read %lu overrunning buffer length %lu",
                aFd, (unsigned long) readLen, (unsigned long) bufLen);

        bufPtr += readLen;
        bufLen -= readLen;
    }

    rc = 0;

Finally:

    return rc ? -1 : bufPtr - aBuf;
}

/*----------------------------------------------------------------------------*/
int
fd_wait_rd(int aFd, int aMilliseconds)
{
    int rc = -1;

    struct pollfd pollFd = {
        .fd = aFd, .events = POLLIN
    };

    int fds = poll(&pollFd, 1, aMilliseconds);

    if (-1 == fds) {
        if (EINTR != errno)
            goto Finally;
        fds = 0;
    } else if (fds) {
        if (pollFd.revents & POLLHUP) {
            errno = EINTR;
            goto Finally;
        }
    }

    rc = 0;

Finally:

    return rc ? -1 : fds;
}

/******************************************************************************/
