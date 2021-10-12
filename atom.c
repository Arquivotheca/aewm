/* aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>.
 * This program is free software; please see LICENSE for details. */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include "common.h"
#include "atom.h"

Atom utf8_string;
Atom wm_state;
Atom wm_change_state;
Atom wm_protos;
Atom wm_delete;
Atom net_supported;
Atom net_client_list;
Atom net_client_stack;
Atom net_active_window;
Atom net_close_window;
Atom net_cur_desk;
Atom net_num_desks;
Atom net_wm_name;
Atom net_wm_desk;
Atom net_wm_state;
Atom net_wm_state_shaded;
Atom net_wm_state_mv;
Atom net_wm_state_mh;
Atom net_wm_state_fs;
Atom net_wm_state_skipt;
Atom net_wm_state_skipp;
Atom net_wm_strut;
Atom net_wm_strut_partial;
Atom net_wm_wintype;
Atom net_wm_type_dock;
Atom net_wm_type_menu;
Atom net_wm_type_splash;
Atom net_wm_type_desk;

static char *get_string_atom(Window, Atom, Atom);

/* Despite the fact that all these are 32 bits on the wire, libX11 really does
 * stuff an array of longs into *data, so you get 64 bits on 64-bit archs. So
 * we gotta be careful here. */

unsigned long get_atoms(Window w, Atom a, Atom type, unsigned long off,
    unsigned long *ret, unsigned long nitems, unsigned long *left)
{
    Atom real_type;
    int i, real_format = 0;
    unsigned long items_read = 0;
    unsigned long bytes_left = 0;
    unsigned long *p;
    unsigned char *data;

    XGetWindowProperty(dpy, w, a, off, nitems, False, type,
        &real_type, &real_format, &items_read, &bytes_left, &data);

    if (real_format == 32 && items_read) {
        p = (unsigned long *)data;
        for (i = 0; i < items_read; i++) *ret++ = *p++;
        XFree(data);
        if (left) *left = bytes_left;
        return items_read;
    } else {
	return 0;
    }
}

unsigned long set_atoms(Window w, Atom a, Atom type, unsigned long *val,
    unsigned long nitems)
{
    return (XChangeProperty(dpy, w, a, type, 32, PropModeReplace,
        (unsigned char *)val, nitems) == Success);
}

unsigned long append_atoms(Window w, Atom a, Atom type, unsigned long *val,
    unsigned long nitems)
{
    return (XChangeProperty(dpy, w, a, type, 32, PropModeAppend,
        (unsigned char *)val, nitems) == Success);
}

void remove_atom(Window w, Atom a, Atom type, unsigned long remove)
{
    unsigned long tmp, read, left, *new;
    int i, j = 0;

    read = get_atoms(w, a, type, 0, &tmp, 1, &left);
    if (!read) return;

    new = malloc((read + left) * sizeof *new);
    if (read && tmp != remove)
        new[j++] = tmp;

    for (i = 1, read = left = 1; read && left; i += read) {
        read = get_atoms(w, a, type, i, &tmp, 1, &left);
        if (!read)
            break;
        else if (tmp != remove)
            new[j++] = tmp;
    }

    if (j)
        XChangeProperty(dpy, w, a, type, 32, PropModeReplace,
            (unsigned char *)new, j);
    else
        XDeleteProperty(dpy, w, a);
}

/* Get the window-manager name (aka human-readable "title") for a given
 * window. There are two ways a client can set this:
 *
 * 1. _NET_WM_STRING, which has type UTF8_STRING. This is preferred and
 *    is always used if available.
 * 2. WM_NAME, which has type COMPOUND_STRING or STRING. This is the old
 *    ICCCM way, which we fall back to in the absence of _NET_WM_STRING.
 *    In this case, we ask X to convert the value of the property to
 *    UTF-8 for us. N.b.: STRING is Latin-1 whatever the locale.
 *    COMPOUND_STRING is the most hideous abomination ever created.
 *    Thankfully we do not have to worry about any of this.
 *
 * If UTF-8 conversion is not available (XFree86 < 4.0.2, or any older X
 * implementation), only WM_NAME will be checked, and, at least for
 * XFree86 and X.Org, it will only be returned if it has type STRING.
 * This is due to an inherent limitation in their implementation of
 * XFetchName. If you have a different X vendor, YMMV.
 *
 * In all cases, this function asks X to allocate the returned string,
 * so it must be freed with XFree. */

char *get_wm_name(Window w)
{
    char *name;
#ifdef X_HAVE_UTF8_STRING
    XTextProperty name_prop;
    XTextProperty name_prop_converted;
    char **name_list;
    int nitems;

    if ((name = get_string_atom(w, net_wm_name, utf8_string))) {
        return name;
    } else if (XGetWMName(dpy, w, &name_prop)) {
        if (Xutf8TextPropertyToTextList(dpy, &name_prop, &name_list,
                &nitems) == Success && nitems >= 1) {
            /* Now we've got a freshly allocated XTextList. Since it might
             * have multiple items that need to be joined, and we need to
             * return something that can be freed by XFree, we roll it back up
             * into an XTextProperty. */
            if (Xutf8TextListToTextProperty(dpy, name_list, nitems,
                    XUTF8StringStyle, &name_prop_converted) == Success) {
                XFreeStringList(name_list);
                return (char *)name_prop_converted.value;
            } else {
                /* Not much we can do here. This should never happen anyway.
                 * Famous last words. */
                XFreeStringList(name_list);
                return NULL;
            }
        } else {
            return (char *)name_prop.value;
        }
    } else {
        /* There is no prop. There is only NULL! */
        return NULL;
    }
#else
    XFetchName(dpy, w, &name);
    return name;
#endif
}

/* I give up on trying to do this the right way. We'll just request as many
 * elements as possible. If that's not the entire string, we're fucked. In
 * reality this should never happen. (That's the second time I get to say
 * ``this should never happen'' in this file!)
 *
 * Standard gripe about casting nonsense applies. */

static char *get_string_atom(Window w, Atom a, Atom type)
{
    Atom real_type;
    int real_format = 0;
    unsigned long items_read = 0;
    unsigned long bytes_left = 0;
    unsigned char *data;

    XGetWindowProperty(dpy, w, a, 0, LONG_MAX, False, type,
        &real_type, &real_format, &items_read, &bytes_left, &data);

    /* XXX: should check bytes_left here and bail if nonzero, in case someone
     * wants to store a >4gb string on the server */

    if (real_format == 8 && items_read >= 1)
        return (char *)data;
    else
        return NULL;
}

/* Reads the _NET_WM_STRUT_PARTIAL or _NET_WM_STRUT hint into the args, if it
 * exists. In the case of _NET_WM_STRUT_PARTIAL we cheat and only take the
 * first 4 values, because that's all we care about. This means we can use the
 * same code for both (_NET_WM_STRUT only specifies 4 elements). Each number
 * is the margin in pixels on that side of the display where we don't want to
 * place clients. If there is no hint, we act as if it was all zeros (no
 * margin). */

int get_strut(Window w, strut_t *s)
{
    Atom real_type;
    int real_format = 0;
    unsigned long items_read = 0;
    unsigned long bytes_left = 0;
    unsigned char *data;
    unsigned long *strut_data;

    XGetWindowProperty(dpy, w, net_wm_strut_partial, 0, 12, False,
        XA_CARDINAL, &real_type, &real_format, &items_read, &bytes_left,
        &data);

    if (!(real_format == 32 && items_read >= 12))
        XGetWindowProperty(dpy, w, net_wm_strut, 0, 4, False, XA_CARDINAL,
            &real_type, &real_format, &items_read, &bytes_left, &data);

    if (real_format == 32 && items_read >= 4) {
        strut_data = (unsigned long *)data;
        s->left = strut_data[0];
        s->right = strut_data[1];
        s->top = strut_data[2];
        s->bottom = strut_data[3];
        XFree(data);
        return 1;
    } else {
        s->left = 0;
        s->right = 0;
        s->top = 0;
        s->bottom = 0;
        return 0;
    }
}

unsigned long get_wm_state(Window w)
{
    unsigned long state;

    if (get_atoms(w, wm_state, wm_state, 0, &state, 1, NULL))
        return state;
    else
        return WithdrawnState;
}
