/**
 * Create a cohort of ssh agents
 *
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
#include "macros.h"
#include "proc.h"

#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/file.h>
#include <sys/stat.h>

/******************************************************************************/
static int optHelp;
static const char *argPath;

/******************************************************************************/
struct Cohort {

    const char *mAgentPath;

    char mLockingPrefix;

    char *mLinkPath;

    struct {
        const char *mName;
        int mFd;
        int mParentFd;
    } mDir;

    pid_t mParentPid;
};

/******************************************************************************/
static int
stdfd_init_(void)
{
    int rc = -1;

    int nullFd = -1;
    int dupFd = -1;

    nullFd = open("/dev/null", O_RDWR);
    if (-1 == nullFd)
        goto Finally;

    for (int stdfd = 0; stdfd < 3; ++stdfd) {
        if (stdfd == nullFd) {

            DEBUG("Creating std fd %d", stdfd);

            dupFd = dup(nullFd);
            if (-1 == dupFd)
                goto Finally;

            nullFd = dupFd;
            dupFd = -1;
        }
    }

    rc = 0;

Finally:

    FINALLY({
        dupFd = fd_close(dupFd);
        nullFd = fd_close(nullFd);
    });

    return rc;
}

/******************************************************************************/
static int
clean_cohort_directory_(struct Cohort *self)
{
    int rc = -1;

    DIR *dirHandle = 0;

    DEBUG("Cleaning directory %s", self->mDir.mName);

    int dirFd = dup(self->mDir.mFd);
    if (-1 == dirFd) {
        error(
            "Error duplicating file descriptor for directory %s",
            self->mDir.mName);
        goto Finally;
    }

    dirHandle = fdopendir(dirFd);
    if (!dirHandle) {
        error("Error opening directory %s", self->mDir.mName);
        goto Finally;
    }

    dirFd = -1;

    struct dirent *dirEntry;

    int errored = 0;
    while (errno = 0, dirEntry = readdir(dirHandle)) {
        if (dirEntry->d_name[0] != self->mLockingPrefix ||
                ! dirEntry->d_name[1]) {
            DEBUG("Skipping %s", dirEntry->d_name);
        } else {
            DEBUG("Removing %s", dirEntry->d_name);
            if (unlinkat(self->mDir.mFd, dirEntry->d_name, 0)) {
                error("Error removing %s from directory %s",
                    dirEntry->d_name, self->mDir.mName);
                errored = errno;
            }
        }
    }
    if (errno) {
        error("Error reading entries in directory %s", self->mDir.mName);
        goto Finally;
    }
    if (errored) {
        errno = errored;
        goto Finally;
    }

    DEBUG("Cleaned directory %s", self->mDir.mName);

    rc = 0;

Finally:

    FINALLY({
        dirFd = fd_close(dirFd);

        if (dirHandle)
            closedir(dirHandle);
    });

    return rc;
}

/*----------------------------------------------------------------------------*/
static int
make_cohort_symlink_(struct Cohort *self)
{
    int rc = -1;

    while (1) {

        /* Repeatedly attempt to create a symlink using a generated name
         * formed by a prefix using the pid of the parent process, and a
         * randomly selected suffix.
         *
         * Once the symlink is created, rename the symlink to the match the
         * selected name. Because the rename operation is atomic, this
         * ensures that there is no window of time where the symlink does
         * not exist.
         */

        uint32_t rnd = random();

        char randomName[
            sizeof(self->mLockingPrefix) +
                sizeof("--") + CHAR_BIT * sizeof(uint32_t)];

        snprintf(randomName, sizeof(randomName), "%c-%" PRIu32 "-%" PRIx32,
            self->mLockingPrefix, self->mParentPid, rnd);

        DEBUG("Creating symlink %s", randomName);

        if (symlinkat(self->mAgentPath, self->mDir.mFd, randomName)) {
            if (EEXIST == errno)
                continue;
            error("Error creating symlink %s to %s",
                randomName, self->mAgentPath);
            goto Finally;
        }

        char linkName[2] = { self->mLockingPrefix, 0 };

        DEBUG("Renaming symlink %s to %s", randomName, linkName);

        if (renameat(
                self->mDir.mFd, randomName, self->mDir.mFd, linkName)) {
            error("Error renaming symlink %s to %s",
                randomName, linkName);
            goto Finally;
        }

        break;
    }

    rc = 0;

Finally:

    FINALLY({
    });

    return rc;
}

/*----------------------------------------------------------------------------*/
static int
acquire_cohort_lock_(struct Cohort *self, int aNonBlock)
{
    int rc = -1;

    /* Locking is achieved using two sub-locks:
     *
     *  1. A Link Lock that covers ownership of the symlink
     *  2. A Session Lock that covers ownership of the agent session
     *
     * The windows covered by the sub-locks overlap:
     *
     *       Link Lock
     *  +----------------------+
     *  |                      |     Session Lock
     *  | +----------------------------------------------+
     *  | |                                              |
     *  | +----------------------------------------------+
     *  |                      |
     *  +----------------------+
     *     <------------------>  <---------------------->
     *              T1                     T2
     *
     * Changes to the symlink occur in T1 when both the Link Lock and
     * Session Lock are held.
     *
     * Validity of the symlink extends throughout T2 when only the
     * Session Lock is held.
     *
     * To avoid deadlocks, the Link Lock must be acquired before the
     * Session Lock.
     *
     * The Link Lock is implemented using a lock on the parent directory,
     * and the Session Lock using a lock on the child directory.
     */

    DEBUG("%s lock on %s",
        (aNonBlock ? "Polling" : "Acquiring"), self->mDir.mName);

    int locked;

    if (LOCK_NB == aNonBlock) {

        /* When called with LOCK_NB, the function acquires the Link
         * Lock, and makes a single attempt to acquire the Session
         * Lock.
         */

        DEBUG("Acquiring link lock %s/..", self->mDir.mName);

        if (flock(self->mDir.mParentFd, LOCK_EX)) {
            error("Error locking %s/..", self->mDir.mName);
            goto Finally;
        }

        DEBUG("Polling session lock %s", self->mDir.mName);

        if (flock(self->mDir.mFd, LOCK_EX | LOCK_NB)) {
            if (EWOULDBLOCK != errno) {
                error("Error locking %s", self->mDir.mName);
                goto Finally;
            }

            locked = 0;

        } else {
            locked = 1;
        }

    } else {

        /* When called without LOCK_NB, the function repeatedly
         * makes attempts to acquire the Session Lock, after first
         * probing the Session Lock then acquiring the Link Lock.
         *
         * The probes of the Session Lock are used to verify
         * that ownership of the agent session has lapsed. Once
         * this is known, the locking order is honoured by first
         * acquiring the Link Lock, then polling the Session Lock.
         * If the Session Lock cannot be acquired, the Link Lock
         * is released, and the protocol restarted.
         */

        while (1) {

            DEBUG("Probing session lock on %s",  self->mDir.mName);

            if (flock(self->mDir.mFd, LOCK_EX)) {
                error("Error locking %s", self->mDir.mName);
                goto Finally;
            }

            if (flock(self->mDir.mFd, LOCK_UN)) {
                error("Error unlocking %s", self->mDir.mName);
                goto Finally;
            }

            DEBUG("Acquiring link lock %s/..", self->mDir.mName);

            if (flock(self->mDir.mParentFd, LOCK_EX)) {

                if (EWOULDBLOCK == errno) {
                    DEBUG("Link lock not acquired on %s/..", self->mDir.mName);
                    continue;

                } else {
                    error("Error locking %s/..", self->mDir.mName);
                    goto Finally;

                }
            }

            DEBUG("Probing session lock %s", self->mDir.mName);

            if (flock(self->mDir.mFd, LOCK_EX | LOCK_NB)) {
                if (EWOULDBLOCK != errno) {
                    error("Error locking %s", self->mDir.mName);
                    goto Finally;
                }

            } else {

                locked = 1;
                break;
            }

            if (flock(self->mDir.mParentFd, LOCK_UN)) {
                error("Error unlocking %s/..", self->mDir.mName);
                goto Finally;
            }
        }
    }

    if (! locked) {
        DEBUG("Session lock not acquired on %s", self->mDir.mName);

    } else {

        DEBUG("Session lock acquired on %s", self->mDir.mName);

        if (clean_cohort_directory_(self))
            warn("Unable to clean directory %s", self->mDir.mName);

        if (make_cohort_symlink_(self))
            goto Finally;
    }

    rc = locked;

Finally:

    FINALLY({
        flock(self->mDir.mParentFd, LOCK_UN);
    });

    return rc;
}

/*----------------------------------------------------------------------------*/
static int
wait_cohort_parent_(struct Cohort *self)
{
    int rc = -1;

    int parentFd = -1;

    /* Although mParentPid refers to the parent, the parent process might have
     * terminated, and the pid reused for a different process. Close this
     * loop hole by explicitly verifying that the parent is unchanged.
     *
     * A more straightforward strategy would be to call proc_fd() in the parent,
     * and allow the child to inherit the file descriptor across fork(2).
     * Unfortunately this strategy is not supported by kqueue(2) which
     * explicitly says that the file descriptor is not inherited across a fork.
     */

    DEBUG("Waiting for parent pid %lld", (long long) self->mParentPid);

    parentFd = proc_fd(self->mParentPid);
    if (-1 == parentFd)
        goto Finally;

    if (getppid() != self->mParentPid) {
        errno = ESRCH;
        goto Finally;
    }

    while (1) {

        struct pollfd pollFds[1] = {
            { .fd = parentFd, .events = POLLIN },
        };

        int polled = poll(pollFds, NUMBEROF(pollFds), -1);

        if (-1 == polled) {
            error("Error polling parent fd %d for pid %lld",
                parentFd, (long long) self->mParentPid);
            goto Finally;
        }

        int parentProc = proc_fd_read(parentFd);

        if (-1 == parentProc) {
            error("Error checking event for parent pid %lld",
                (long long) self->mParentPid);
            goto Finally;
        }

        if (parentProc)
            break;
    }

    DEBUG("Parent pid %lld exited", (long long) self->mParentPid);

    rc = 0;

Finally:

    FINALLY({
        parentFd = fd_close(parentFd);
    });

    return rc;
}

/*----------------------------------------------------------------------------*/
static int
run_cohort_(struct Cohort *self, int aReadyFd)
{
    int rc = -1;

    int locked = acquire_cohort_lock_(self, LOCK_NB);
    if (-1 == locked)
        goto Finally;

    DEBUG("Child ready");

    static char ready[1] = { 0 };

    if (1 != fd_write(aReadyFd, ready, 1)) {
        error("Error writing ready indication");
        goto Finally;
    }

    if (! locked) {
        locked = acquire_cohort_lock_(self, 0);
        if (-1 == locked)
            goto Finally;
    }

    if (! locked) {
        errno = ENOLCK;
        goto Finally;
    }

    if (wait_cohort_parent_(self))
        goto Finally;

    rc = 0;

Finally:

    FINALLY({
    });

    return rc;
}

/*----------------------------------------------------------------------------*/
struct Cohort *
close_cohort(struct Cohort *self)
{
    if (self) {
        self->mDir.mFd = fd_close(self->mDir.mFd);
        self->mDir.mParentFd = fd_close(self->mDir.mParentFd);

        FREE(self->mLinkPath);
    }

    return 0;
}

/*----------------------------------------------------------------------------*/
struct Cohort *
create_cohort(struct Cohort *self_, const char *aPath, const char *aAgentPath)
{
    struct Cohort *self = 0;

    int dirFd = -1;
    int parentDirFd = -1;

    char *linkPath = 0;

    int readyPipe[2] = {-1 , -1};

    pid_t childPid = -1;

    self_->mAgentPath = 0;
    self_->mLockingPrefix = '@';
    self_->mLinkPath = 0;
    self_->mDir.mName = 0;
    self_->mDir.mFd = -1;
    self_->mDir.mParentFd = -1;
    self_->mParentPid = 0;

    DEBUG("Create cohort in %s using %s", aPath, aAgentPath);

    char linkPathBuffer[
        strlen(aPath) + sizeof("/") + sizeof(self_->mLockingPrefix)];

    snprintf(linkPathBuffer, sizeof(linkPathBuffer), "%s/%c",
        aPath, self_->mLockingPrefix);

    linkPath = strdup(linkPathBuffer);
    if (!linkPath)
        goto Finally;

    pid_t parentPid = getpid();

    DEBUG("Parent pid %lld", (long long) parentPid);

    if (pipe(readyPipe))
        goto Finally;

    DEBUG("Opening directory %s", aPath);

    dirFd = open(aPath, O_RDONLY | O_DIRECTORY);
    if (-1 == dirFd)
        goto Finally;

    parentDirFd = openat(dirFd, "..", O_RDONLY | O_DIRECTORY);
    if (-1 == parentDirFd)
        goto Finally;

    struct stat dirStat, parentDirStat;

    if (fstat(dirFd, &dirStat) || fstat(parentDirFd, &parentDirStat))
        goto Finally;

    if (dirStat.st_dev == parentDirStat.st_dev &&
            dirStat.st_ino == parentDirStat.st_ino) {
        error("Directory names mount point at %s", aPath);
        goto Finally;
    }

    self_->mAgentPath = aAgentPath;
    self_->mLinkPath = linkPath;
    self_->mDir.mName = aPath;
    self_->mDir.mFd = dirFd;
    self_->mDir.mParentFd = parentDirFd;
    self_->mParentPid = parentPid;

    linkPath = 0;
    dirFd = -1;
    parentDirFd = -1;
    parentPid = 0;

    childPid = fork();

    if (-1 == childPid) {
        goto Finally;

    } else if (!childPid) {

        DEBUG("Child starting");
        readyPipe[0] = fd_close(readyPipe[0]);

        /* The standard file descriptors were sanitised using stdfd_init() when
         * the parent process was started. Ensure that the child no longer
         * shares stdin and stdout file descriptors with the parent process.
         */

        close(0);
        close(1);

        int stdFd[2];

        if (pipe(stdFd))
            goto Finally;

        if (0 != stdFd[0] || 1 != stdFd[1]) {
            error("Error resetting stdin and stdout");
            errno = ENOSYS;
            goto Finally;
        }

        static const struct {
            int mSignal;
            const char *mName;
        } ignoredSignals[] = {
            { SIGTERM, "SIGTERM" },
            { SIGINT,  "SIGINT" },
            { SIGQUIT, "SIGQUIT" },
            { SIGHUP,  "SIGHUP" },
        };

        for (unsigned sx = 0; sx < NUMBEROF(ignoredSignals); ++sx) {
            if (SIG_ERR == signal(ignoredSignals[sx].mSignal, SIG_IGN)) {
                error("Error ignoring signal %s", ignoredSignals[sx].mName);
                goto Finally;
            }
        }

        if (run_cohort_(self_, readyPipe[1]))
            goto Finally;

        DEBUG("Child exiting");

    } else {

        DEBUG("Waiting for child pid %lld", (long long) childPid);

        readyPipe[1] = fd_close(readyPipe[1]);

        char readyBuf[1];
        if (1 != fd_read(readyPipe[0], readyBuf, 1))
            goto Finally;

        readyPipe[0] = fd_close(readyPipe[0]);

        DEBUG("Child pid %lld ready", (long long) childPid);
    }

    self = self_;
    self_ = 0;

Finally:

    FINALLY({
        readyPipe[0] = fd_close(readyPipe[0]);
        readyPipe[1] = fd_close(readyPipe[1]);

        dirFd = fd_close(dirFd);
        parentDirFd = fd_close(parentDirFd);

        FREE(linkPath);

        self_ = close_cohort(self_);
    });

    if (!childPid) {

        int exitCode = self ? EXIT_SUCCESS : EXIT_FAILURE;

        self = close_cohort(self);

        exit(exitCode);
    }

    return self;
}

/*----------------------------------------------------------------------------*/
const char *
cohort_link_path(const struct Cohort *self)
{
    return self->mLinkPath;
}

/******************************************************************************/
void
usage(void)
{
    static const char usageText[] =
        "[-d] sshagentsock -- cmd ...\n"
        "\n"
        "Options:\n"
        "  -d --debug      Emit debug information\n"
        "\n"
        "Arguments:\n"
        "  sshagentsock    Socket path for SSH agent\n"
        "  cmd ...         Program to execute\n";

    help(usageText, optHelp);

    exit(EXIT_FAILURE);
}

/******************************************************************************/
static char **
parse_options(int argc, char **argv)
{
    int rc = -1;

    static char shortOpts[] = "+hd";

    static struct option longOpts[] = {
        { "help",  no_argument, 0, 'h' },
        { "debug", no_argument, 0, 'd' },
        { 0 },
    };

    while (1) {
        int ch = getopt_long(argc, argv, shortOpts, longOpts, 0);

        if (-1 == ch)
            break;

        switch (ch) {
        default:
            break;

        case 'h':
            optHelp = 1;
            goto Finally;

        case ':':
        case '?':
            goto Finally;

        case 'd':
            debug("%s", DebugEnable); break;

        }
    }

    if (argc >= optind && !strcmp("--", argv[optind-1]))
        goto Finally;

    if (argc > optind && strcmp("--", argv[optind]))
        argPath = argv[optind++];
    else
        goto Finally;

    if (argc > optind && !strcmp("--", argv[optind]))
        ++optind;
    else
        goto Finally;

    if (argc > optind)
        rc = 0;

Finally:

    FINALLY({
        if (!rc) {
            argv += optind;
            argc -= optind;
        }
    });

    return rc ? 0 : argv;
}

/******************************************************************************/
int
main(int argc, char **argv)
{
    int exitCode = 255;

    struct Cohort cohort_, *cohort = 0;

    /* Ensure that stdin, stdout, and stderr, file descriptors are allocated
     * to avoid confusing new file activity with these standard descriptors.
     */

    if (stdfd_init_()) {
        error("Error initialising standard file descriptors");
        goto Finally;
    }

    srandom(getpid());

    char **cmd = parse_options(argc, argv);
    if (!cmd || !cmd[0])
        usage();

    /* Only create a cohort if SSH_AUTH_SOCK is defined. Otherwise
     * forgo the cohort and fall through to execute the command
     * directly.
     */

    static const char sshAuthSockEnv[] = "SSH_AUTH_SOCK";

    const char *agentPath = getenv(sshAuthSockEnv);

    if (agentPath) {

        cohort = create_cohort(&cohort_, argPath, agentPath);
        if (!cohort) {
            error("Error creating agent cohort");
            goto Finally;
        }

        if (setenv(sshAuthSockEnv, cohort_link_path(cohort), 1)) {
            error("Error configuring %s", sshAuthSockEnv);
            goto Finally;
        }
    }

    DEBUG("Execute command %s", cmd[0]);
    if (execvp(cmd[0], cmd))
        die("Error executing %s", cmd[0]);

    exitCode = 0;

Finally:

    FINALLY({
        cohort = close_cohort(cohort);
    });

    return exitCode;
}

/******************************************************************************/
