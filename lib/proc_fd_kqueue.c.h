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
#include "fd.h"

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/event.h>
#include <sys/wait.h>

/******************************************************************************/
int proc_fd(pid_t aPid)
{
    int rc = -1;

    int procFd = -1;

    if (!aPid) {
        errno = EINVAL;
        goto Finally;
    }

    procFd = kqueue();
    if (-1 == procFd)
        goto Finally;

    struct kevent kEvent;

    EV_SET(
        &kEvent, aPid,
        EVFILT_PROC,
        EV_ADD,
        NOTE_EXIT | NOTE_EXITSTATUS, 0, 0);

    if (-1 == kevent(procFd, &kEvent, 1, 0, 0, 0))
        goto Finally;

    rc = 0;

Finally:

    FINALLY({
        if (rc)
            procFd = fd_close(procFd);
    });

    return rc ? rc : procFd;
}

/*----------------------------------------------------------------------------*/
int proc_fd_read(int aProcFd)
{
    int rc = -1;

    int events = -1;

    struct kevent kEvent;

    struct timespec zeroTimeout = { 0, 0 };

    events = kevent(aProcFd, 0, 0, &kEvent, 1, &zeroTimeout);
    if (-1 == events) {
        if (EINTR != errno)
            goto Finally;
        events = 0;
    }

    rc = 0;

Finally:

    return rc ? rc : events;
}

/******************************************************************************/
