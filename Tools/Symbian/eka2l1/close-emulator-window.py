#!/usr/bin/env python3
"""Ask the X11 windows belonging to one emulator process to close normally."""

import ctypes
import sys


class ClientData(ctypes.Union):
    _fields_ = [('b', ctypes.c_char * 20), ('s', ctypes.c_short * 10),
                ('l', ctypes.c_long * 5)]


class ClientMessage(ctypes.Structure):
    _fields_ = [('type', ctypes.c_int), ('serial', ctypes.c_ulong),
                ('send_event', ctypes.c_int), ('display', ctypes.c_void_p),
                ('window', ctypes.c_ulong), ('message_type', ctypes.c_ulong),
                ('format', ctypes.c_int), ('data', ClientData)]


class XEvent(ctypes.Union):
    _fields_ = [('client', ClientMessage), ('pad', ctypes.c_long * 24)]


def close_process_windows(pid):
    x11 = ctypes.CDLL('libX11.so.6')
    pointer = ctypes.c_void_p
    ulong = ctypes.c_ulong
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = pointer
    x11.XDefaultRootWindow.argtypes = [pointer]
    x11.XDefaultRootWindow.restype = ulong
    x11.XInternAtom.argtypes = [pointer, ctypes.c_char_p, ctypes.c_int]
    x11.XInternAtom.restype = ulong
    x11.XQueryTree.argtypes = [pointer, ulong, ctypes.POINTER(ulong),
                              ctypes.POINTER(ulong), ctypes.POINTER(ctypes.POINTER(ulong)),
                              ctypes.POINTER(ctypes.c_uint)]
    x11.XGetWindowProperty.argtypes = [pointer, ulong, ulong, ctypes.c_long,
                                     ctypes.c_long, ctypes.c_int, ulong,
                                     ctypes.POINTER(ulong), ctypes.POINTER(ctypes.c_int),
                                     ctypes.POINTER(ulong), ctypes.POINTER(ulong),
                                     ctypes.POINTER(pointer)]
    x11.XSendEvent.argtypes = [pointer, ulong, ctypes.c_int, ctypes.c_long,
                              ctypes.POINTER(XEvent)]
    x11.XFree.argtypes = [pointer]
    x11.XFlush.argtypes = [pointer]
    x11.XCloseDisplay.argtypes = [pointer]

    display = x11.XOpenDisplay(None)
    if not display:
        raise RuntimeError('Cannot open DISPLAY for graceful emulator shutdown')
    try:
        pid_atom = x11.XInternAtom(display, b'_NET_WM_PID', 1)
        protocols = x11.XInternAtom(display, b'WM_PROTOCOLS', 1)
        delete_window = x11.XInternAtom(display, b'WM_DELETE_WINDOW', 1)

        def property_values(window, atom):
            if not atom:
                return []
            actual_type, count, remaining = ulong(), ulong(), ulong()
            actual_format, data = ctypes.c_int(), pointer()
            status = x11.XGetWindowProperty(display, window, atom, 0, 1024, 0, 0,
                                           ctypes.byref(actual_type), ctypes.byref(actual_format),
                                           ctypes.byref(count), ctypes.byref(remaining),
                                           ctypes.byref(data))
            try:
                if status or actual_format.value != 32 or not data:
                    return []
                values = ctypes.cast(data, ctypes.POINTER(ulong))
                return [values[index] for index in range(count.value)]
            finally:
                if data:
                    x11.XFree(data)

        pending = [x11.XDefaultRootWindow(display)]
        matches = []
        while pending:
            window = pending.pop()
            if property_values(window, pid_atom) == [pid] and delete_window in property_values(window, protocols):
                matches.append(window)
            root, parent, count = ulong(), ulong(), ctypes.c_uint()
            children = ctypes.POINTER(ulong)()
            if x11.XQueryTree(display, window, ctypes.byref(root), ctypes.byref(parent),
                              ctypes.byref(children), ctypes.byref(count)):
                pending.extend(children[index] for index in range(count.value))
            if children:
                x11.XFree(children)

        for window in matches:
            event = XEvent()
            event.client.type = 33  # ClientMessage
            event.client.send_event = 1
            event.client.display = display
            event.client.window = window
            event.client.message_type = protocols
            event.client.format = 32
            event.client.data.l[0] = delete_window
            if not x11.XSendEvent(display, window, 0, 0, ctypes.byref(event)):
                raise RuntimeError('Cannot send WM_DELETE_WINDOW to emulator')
        x11.XFlush(display)
        if not matches:
            raise RuntimeError(f'No closable X11 window belongs to emulator PID {pid}')
    finally:
        x11.XCloseDisplay(display)


if __name__ == '__main__':
    try:
        close_process_windows(int(sys.argv[1]))
    except (IndexError, ValueError, OSError, RuntimeError) as error:
        sys.exit(str(error))
