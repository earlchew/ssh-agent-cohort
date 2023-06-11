# SSH Agent Cohorts

**ssh-agent-cohort** is a utility that is useful to
support `tmux(1)` sessions over SSH that use agent
forwarding. When the remote SSH process is created,
the environment variable `SSH_AUTH_SOCK` points
at connection specific socket that provides access
to the agent. The lifetime of the socket is bound
to the lifetime of the connection, whereas the
lifetimes of the processes controlled by tmux are
typically far longer.

**ssh-agent-cohort** mediates between the long
lived tmux processes, and the shorter lived
SSH agent sockets, by allowing the user to
specify a long lived name which tmux processes
can use to access one of the shorter lived
agent connections.

## Description

Normally, an SSH session is re-connected to an
established tmux session using a `tmux attach`
command, or similar. To support remote SSH agents,
the user selects a fixed location for the
socket, for example, `/tmp/tmux/$UID/ssh/agent`.
The socket location is propagated into tmux sessions
by configuring the environment variable `SSH_AUTH_SOCK`
before starting tmux, for example:

    env SSH_AUTH_SOCK=/tmp/tmux/$UID/ssh/agent tmux attach

**ssh-double-agent** automates this by configuring
the `SSH_AUTH_SOCK` environment variable before
starting the next command, and also binding one
of the live SSH agent sockets to the named socket
location, for example:

    env ssh-agent-cohort /tmp/tmux/$UID/ssh/agent -- tmux attach

Apart from automating the setting of `SSH_AUTH_SOCK`
environment variable, **ssh-double-agent** also
takes care of selecting an SSH agent connection when
there is more than one SSH connection. For example,
suppose the use creates 3 SSH connections. Typically,
**ssh-double-agent** will use the first connection,
and when that connection terminates, **ssh-double-agent**
will switch over to one of the remaining 2 SSH connections.

## Getting Started

### Dependencies

**ssh-double-agent** is implemented entirely in Posix C,
with **libc** as its only dependency. The program is known
to compile on:
* Mac OS
* Linux

### Installing

**ssh-double-agent** is compiled from source using the accompanying `Makefile`.

### Executing program

Documentation is provided in the accompanying `ssh-double-agent.man` file
which is created from using `man` target in the `Makefile`.
