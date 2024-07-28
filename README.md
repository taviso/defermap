# defermap

This is a simple preload library for adding *deferred mapping* to `xterm`.
Deferred mapping just means that a program is running, but the window is not
visible.

You might want this on systems where `xterm` is annoyingly slow to start. This
approach can make an it appear near instantly, even on the slowest systems.

The idea is to cache a few `xterm`s in the background, then when you want a new
terminal, all the slow stuff will already be done and you can start typing
immediately.

## Installation

You can simply copy `defermap.so` to `/usr/lib`, or run `make install` as root.

To uninstall it, just remove the file.

## Usage

You need something to manage the cache for you in the background, `xargs` will work.

Try running a command like this:

```
$ xargs --null --arg-file=/dev/zero --max-procs=3 --replace -- \
     env LD_PRELOAD=defermap.so xterm -display :0 [PARAMS...]
```

This will keep three xterms running in the background.

> Note: If you often rapidly start terminals in quick succession, increase
> `max-procs`.

When you want a new terminal, instead of running `xterm` as you normally would,
do this instead:

```
$ pkill --oldest --signal SIGUSR1 xtermserver
```

An xterm should appear near-instantly.

If you decide you like this, you can make `xterm` an alias to this.

### Advanced

- You can adjust the number of `xterm`s cached dynamically at runtime.

> Note: This is an `xargs` feature, see [here](https://www.gnu.org/software/findutils/manual/html_node/find_html/Controlling-Parallelism.html) for details.

You simply send `xargs` a `SIGUSR1` to increase the cache size, and a
`SIGUSR2` to decrease it.

```
$ pkill --signal SIGUSR1 xargs # increase number of cached xterms
$ pkill --signal SIGUSR2 xargs # decrease number of cached xterms
```

- I print reminders in my `.bashrc`, so don't want xterms sitting in the
  cache for too long.

I reap them if they're unused after a few hours, if you don't like this, edit
`kIdleTimeout` in `defermap.c`.

- If you use a shell function instead of an alias, you can add a fallback in
  case the server isn't running.

```
$ xterm() { pkill --oldest --signal SIGUSR1 xtermserver || /usr/bin/xterm; }
```

## Startup

If you want this to run in the background, you can do something like this in your
`.xinitrc` or similar startup script (please excuse the comically long
commandline):

```
$ start-stop-daemon --startas xargs                 \
                    --background                    \
                    --make-pidfile                  \
                    --pidfile ~/.xtermserver.pid    \
                    --chdir $HOME                   \
                    --quiet                         \
                    --start                         \
                    -- --null                       \
                       --arg-file=/dev/zero         \
                       --max-procs=3                \
                       --replace                    \
                       -- env LD_PRELOAD=defermap.so xterm -display :0 [...]
```

You probably also want an alias or shell function to invoke it, something like
this:

```
$ alias xterm="pkill --oldest --signal SIGUSR1 xtermserver"
```

Naturally, you can also bind this command to some keystroke in your window manager.

## Contact

Contact taviso@gmail.com with questions/comments.
