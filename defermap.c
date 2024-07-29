#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <errno.h>
#include <sys/prctl.h>
#include <X11/Xlib.h>

// This is a quick preload library for implementing deferred mapping in xterm.
//
// You might want this on systems where xterm is slow to startup.
//
// See README for details.
//
// -- taviso@gmail.com, July 2024

// Set this to zero if you never want a deferred process to give up waiting.
static const int kIdleTimeout = 3600 * 6;

// The thread name you want to use for a process with deferred windows.
static const char kServerName[] = "xtermserver";

// Set this to false if you don't like new windows being activated, or want the
// window manager to handle it.
static const bool kActivateWindow = true;

// Comment this if something isn't working
#define _NDEBUG

// The real XMapWindow function.
static int (*XMapWindowPtr)(Display *, Window);

// The original signal handlers.
static sighandler_t origalrm;
static sighandler_t origusr1;

// The deferred window.
static struct {
    Display *display;
    Window   window;
} deferred;

static void DebugLog(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
#ifndef _NDEBUG
    vfprintf(stderr, format, ap);
    fputc('\n', stderr);
#endif
    va_end(ap);
    return;
}

static int ActivateWindow(Display *display, Window w)
{
    XEvent ev = {0};

    // User can configure this if they like.
    if (kActivateWindow == false)
        return 1;

    // Make sure the window is actually mapped first, or this might not work.
    XSync(display, false);
    XFlush(display);

    ev.xclient.type       = ClientMessage;
    ev.xclient.send_event = true;
    ev.xclient.window     = w;
    ev.xclient.format     = 32; // longs

    ev.xclient.message_type = XInternAtom(display, "_NET_ACTIVE_WINDOW", true);

    return XSendEvent(display,
               DefaultRootWindow(display),
               false,
               SubstructureNotifyMask | SubstructureRedirectMask,
               &ev);
}

static void MapTimeout(int n)
{
    DebugLog("Maptimeout(%d)", n);
    raise(SIGTERM);
}

static void MapDeferred(int n)
{
    Status result;

    // This is our second handler, obviously we're doing unsafe things here,
    // but in practice it should be fine.
    DebugLog("MapDeferred(%d)", n);

    // Cancel any pending timeout.
    alarm(0);

    // Indicate we are no longer a server.
    prctl(PR_SET_NAME, program_invocation_short_name);

    // Remove our signal handlers, we no longer need them.
    signal(SIGUSR1, origusr1);
    signal(SIGALRM, origalrm);

    // Map the deferred window.
    result = XMapWindowPtr(deferred.display, deferred.window);

    // Optional, but I like it.
    ActivateWindow(deferred.display, deferred.window);

    DebugLog("XMapRaised(%p, %p) => %p", deferred.display, deferred.window, result);

    // Now run in the background. This is simply so that xargs -P can be used as
    // a manager, otherwise it won't know that this server has been consumed.
    if (daemon(true, false) == -1)
        DebugLog("daemon() failed, %m");


    return;
}

// We only want to defer parents of the root window, so this function
// checks if the specified window is toplevel or a child window.
static bool IsToplevel(Display *display, Window w)
{
    Window root;
    Window parent;
    Window *children;
    unsigned nchildren;

    DebugLog("IsTopLevel(%p, %p)", display, w);

    if (XQueryTree(display, w, &root, &parent, &children, &nchildren) == 0) {
        DebugLog("XQueryTree() failed");
        return false;
    }

    XFree(children);

    DebugLog("IsTopLevel() => parent %p, root %p", parent, root);

    return parent == root;
}

static void __attribute__((constructor)) _init()
{
    // Make sure child processes dont unintentionally load us.
    unsetenv("LD_PRELOAD");
    DebugLog("defermap loaded, pid %u", getpid());
}

int XMapWindow(Display *display, Window w)
{
    DebugLog("XMapWindow(%p, %p)", display, w);

    // Resolve the real XMapWindow() address.
    if (XMapWindowPtr == NULL) {
        XMapWindowPtr = dlsym(RTLD_NEXT, "XMapWindow");
    }

    DebugLog("XMapWindow@%p", XMapWindowPtr);

    // We only defer the toplevel windows, the rest just passthru.
    // FIXME: will there be multiple top windows, should we keep a list?
    if (deferred.window || !IsToplevel(display, w)) {
        DebugLog("XMapWindow() => passthru");
        return XMapWindowPtr(display, w);
    }

    // Save the details for later.
    deferred.window   = w;
    deferred.display  = display;

    // Setup our signal handlers.
    origusr1 = signal(SIGUSR1, MapDeferred);
    origalrm = signal(SIGALRM, MapTimeout);

    // Set an optional idle timeout.
    alarm(kIdleTimeout);

    // We can now make ourselves known as ready, so pkill -USR1 xtermserver
    // will find us.
    prctl(PR_SET_NAME, kServerName);

    // Done
    return 0;
}
