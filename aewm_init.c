/* aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>.
 * This program is free software; please see LICENSE for details. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <locale.h>
#include <sys/wait.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif
#include "aewm.h"
#include "atom.h"
#include "parser.h"

client_t *head;
int screen;
unsigned long ndesks = 1;
unsigned long cur_desk = 0;
#ifdef SHAPE
Bool shape;
int shape_event;
#endif

XFontStruct *font;
#ifdef X_HAVE_UTF8_STRING
XFontSet font_set;
#endif
#ifdef XFT
XftFont *xftfont;
XftColor xft_fg;
#endif

Colormap def_cmap;
XColor fg;
XColor bg;
XColor bd;
GC invert_gc;
GC string_gc;
GC border_gc;
Cursor map_curs;
Cursor move_curs;
Cursor resize_curs;

char *opt_font = DEF_FONT;
#ifdef XFT
char *opt_xftfont = DEF_XFTFONT;
#endif
char *opt_fg = DEF_FG;
char *opt_bg = DEF_BG;
char *opt_bd = DEF_BD;
int opt_bw = DEF_BW;
int opt_pad = DEF_PAD;
int opt_imap = DEF_IMAP;
char *opt_new[] = { DEF_NEW1, DEF_NEW2, DEF_NEW3, DEF_NEW4, DEF_NEW5 };

static void shutdown(void);
static void read_config(char *);
static void setup_display(void);

int main(int argc, char **argv)
{
    int i;
    struct sigaction act;

    setlocale(LC_ALL, "");
    read_config(NULL);

    for (i = 1; i < argc; i++) {
        if ARG("config", "rc", 1) {
            read_config(argv[++i]);
        } else if ARG("font", "fn", 1) {
            opt_font = argv[++i];
#ifdef XFT
        } else if ARG("xftfont", "fa", 1) {
            opt_xftfont = argv[++i];
#endif
        } else if ARG("fgcolor", "fg", 1) {
            opt_fg = argv[++i];
        } else if ARG("bgcolor", "bg", 1) {
            opt_bg = argv[++i];
        } else if ARG("bdcolor", "bd", 1) {
            opt_bd = argv[++i];
        } else if ARG("bdwidth", "bw", 1) {
            opt_bw = atoi(argv[++i]);
        } else if ARG("padding", "p", 1) {
            opt_pad = atoi(argv[++i]);
        } else if ARG("imap", "i", 0) {
            opt_imap = 1;
        } else if ARG("no-imap", "n", 0) {
            opt_imap = 0;
        } else if ARG("new1", "1", 1) {
            opt_new[0] = argv[++i];
        } else if ARG("new2", "2", 1) {
            opt_new[1] = argv[++i];
        } else if ARG("new3", "3", 1) {
            opt_new[2] = argv[++i];
        } else if ARG("new4", "4", 1) {
            opt_new[3] = argv[++i];
        } else if ARG("new5", "5", 1) {
            opt_new[4] = argv[++i];
        } else if ARG("version", "v",0) {
            printf("aewm: version " VERSION "\n");
            exit(0);
        } else if ARG("help", "h",0) {
            printf(USAGE);
            exit(0);
        } else {
            fprintf(stderr, "aewm: unknown option: '%s'\n" USAGE, argv[i]);
            exit(2);
        }
    }

    act.sa_handler = sig_handler;
    act.sa_flags = 0;
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGCHLD, &act, NULL);

    setup_display();
    event_loop();
    return 0;
}

static void read_config(char *rcfile)
{
    FILE *rc;
    char buf[BUF_SIZE], token[BUF_SIZE], *p;

    if (!(rc = open_rc(rcfile, "aewmrc"))) {
        if (rcfile) fprintf(stderr, "aewm: rc file '%s' not found\n", rcfile);
        return;
    }

    while (get_rc_line(buf, sizeof buf, rc)) {
        p = buf;
        while (get_token(&p, token)) {
            if (strcmp(token, "font") == 0) {
                if (get_token(&p, token))
                    opt_font = strdup(token);
#ifdef XFT
            } else if (strcmp(token, "xftfont") == 0) {
                if (get_token(&p, token))
                    opt_xftfont = strdup(token);
#endif
            } else if (strcmp(token, "fgcolor") == 0) {
                if (get_token(&p, token))
                    opt_fg = strdup(token);
            } else if (strcmp(token, "bgcolor") == 0) {
                if (get_token(&p, token))
                    opt_bg = strdup(token);
            } else if (strcmp(token, "bdcolor") == 0) {
                if (get_token(&p, token))
                    opt_bd = strdup(token);
            } else if (strcmp(token, "bdwidth") == 0) {
                if (get_token(&p, token))
                    opt_bw = atoi(token);
            } else if (strcmp(token, "padding") == 0) {
                if (get_token(&p, token))
                    opt_pad = atoi(token);
            } else if (strcmp(token, "imap") == 0) {
                if (get_token(&p, token))
                    opt_imap = atoi(token);
            } else if (strcmp(token, "button1") == 0) {
                if (get_token(&p, token))
                    opt_new[0] = strdup(token);
            } else if (strcmp(token, "button2") == 0) {
                if (get_token(&p, token))
                    opt_new[1] = strdup(token);
            } else if (strcmp(token, "button3") == 0) {
                if (get_token(&p, token))
                    opt_new[2] = strdup(token);
            } else if (strcmp(token, "button4") == 0) {
                if (get_token(&p, token))
                    opt_new[3] = strdup(token);
            } else if (strcmp(token, "button5") == 0) {
                if (get_token(&p, token))
                    opt_new[4] = strdup(token);
            }
        }
    }
    fclose(rc);
}

static void setup_display(void)
{
#ifdef X_HAVE_UTF8_STRING
    char **missing;
    char *def_str;
    int nmissing;
#endif
    XGCValues gv;
    XColor exact;
    XSetWindowAttributes sattr;
    XWindowAttributes attr;
#ifdef SHAPE
    int shape_err;
#endif
    Window qroot, qparent, *wins;
    unsigned int nwins, i;
    client_t *c;

    dpy = XOpenDisplay(NULL);

    if (!dpy) {
        fprintf(stderr, "aewm: can't open $DISPLAY '%s'\n", getenv("DISPLAY"));
        exit(1);
    }

    XSetErrorHandler(handle_xerror);
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    map_curs = XCreateFontCursor(dpy, XC_dotbox);
    move_curs = XCreateFontCursor(dpy, XC_fleur);
    resize_curs = XCreateFontCursor(dpy, XC_sizing);

    def_cmap = DefaultColormap(dpy, screen);
    XAllocNamedColor(dpy, def_cmap, opt_fg, &fg, &exact);
    XAllocNamedColor(dpy, def_cmap, opt_bg, &bg, &exact);
    XAllocNamedColor(dpy, def_cmap, opt_bd, &bd, &exact);

    font = XLoadQueryFont(dpy, opt_font);
    if (!font) {
        fprintf(stderr, "aewm: font '%s' not found\n", opt_font);
        exit(1);
    }

#ifdef X_HAVE_UTF8_STRING
    font_set = XCreateFontSet(dpy, opt_font, &missing, &nmissing, &def_str);
#endif

#ifdef XFT
    xft_fg.color.red = fg.red;
    xft_fg.color.green = fg.green;
    xft_fg.color.blue = fg.blue;
    xft_fg.color.alpha = 0xffff;
    xft_fg.pixel = fg.pixel;

    xftfont = XftFontOpenName(dpy, DefaultScreen(dpy), opt_xftfont);
    if (!xftfont) {
        fprintf(stderr, "aewm: Xft font '%s' not found\n", opt_font);
        exit(1);
    }
#endif

    gv.function = GXcopy;
    gv.foreground = fg.pixel;
    gv.font = font->fid;
    string_gc = XCreateGC(dpy, root, GCFunction|GCForeground|GCFont, &gv);

    gv.foreground = bd.pixel;
    gv.line_width = opt_bw;
    border_gc = XCreateGC(dpy, root,
        GCFunction|GCForeground|GCLineWidth, &gv);

    gv.function = GXinvert;
    gv.subwindow_mode = IncludeInferiors;
    invert_gc = XCreateGC(dpy, root,
        GCFunction|GCSubwindowMode|GCLineWidth|GCFont, &gv);

    utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
    wm_protos = XInternAtom(dpy, "WM_PROTOCOLS", False);
    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    wm_state = XInternAtom(dpy, "WM_STATE", False);
    wm_change_state = XInternAtom(dpy, "WM_CHANGE_STATE", False);
    net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    net_cur_desk = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    net_num_desks = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    net_client_stack = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
    net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    net_close_window = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);
    net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    net_wm_desk = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    net_wm_state_shaded = XInternAtom(dpy, "_NET_WM_STATE_SHADED", False);
    net_wm_state_mv = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    net_wm_state_mh = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    net_wm_state_fs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    net_wm_strut = XInternAtom(dpy, "_NET_WM_STRUT", False);
    net_wm_strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    net_wm_wintype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    net_wm_type_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    net_wm_type_menu = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
    net_wm_type_splash = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
    net_wm_type_desk = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);

    /* The bit about _NET_CLIENT_LIST_STACKING here is an evil lie. */
    append_atoms(root, net_supported, XA_ATOM, &net_cur_desk, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_num_desks, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_client_list, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_client_stack, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_active_window, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_close_window, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_name, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_desk, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_state, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_state_shaded, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_state_mv, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_state_mh, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_state_fs, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_strut, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_strut_partial, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_wintype, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_type_dock, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_type_menu, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_type_splash, 1);
    append_atoms(root, net_supported, XA_ATOM, &net_wm_type_desk, 1);

    get_atoms(root, net_num_desks, XA_CARDINAL, 0, &ndesks, 1, NULL);
    get_atoms(root, net_cur_desk, XA_CARDINAL, 0, &cur_desk, 1, NULL);

#ifdef SHAPE
    shape = XShapeQueryExtension(dpy, &shape_event, &shape_err);
#endif

    XQueryTree(dpy, root, &qroot, &qparent, &wins, &nwins);
    for (i = 0; i < nwins; i++) {
        XGetWindowAttributes(dpy, wins[i], &attr);
        if (!attr.override_redirect && attr.map_state == IsViewable) {
            c = new_client(wins[i]);
            map_client(c);
        }
    }
    XFree(wins);

    sattr.event_mask = SubMask|ColormapChangeMask|ButtonMask;
    XChangeWindowAttributes(dpy, root, CWEventMask, &sattr);
}

void sig_handler(int signum)
{
    switch (signum) {
        case SIGINT:
        case SIGTERM:
        case SIGHUP:
            shutdown();
            break;
        case SIGCHLD:
            wait(NULL);
            break;
    }
}

int handle_xerror(Display *dpy, XErrorEvent *e)
{
#ifdef DEBUG
    client_t *c = find_client(e->resourceid, MATCH_WINDOW);
#endif

    if (e->error_code == BadAccess && e->resourceid == root) {
        fprintf(stderr, "aewm: root window unavailable\n");
        exit(1);
    } else {
        char msg[255];
        XGetErrorText(dpy, e->error_code, msg, sizeof msg);
        fprintf(stderr, "aewm: X error (%#lx): %s\n", e->resourceid, msg);
#ifdef DEBUG
        if (c) dump_info(c);
#endif
    }

#ifdef DEBUG
    if (c) del_client(c, DEL_WITHDRAW);
#endif
    return 0;
}

/* Ick. Argh. You didn't see this function. */

int ignore_xerror(Display *dpy, XErrorEvent *e)
{
    return 0;
}

/* We use XQueryTree here to preserve the window stacking order, since
 * the order in our linked list is different. */

static void shutdown(void)
{
    unsigned int nwins, i;
    Window qroot, qparent, *wins;
    client_t *c;

    XQueryTree(dpy, root, &qroot, &qparent, &wins, &nwins);
    for (i = 0; i < nwins; i++) {
        c = find_client(wins[i], MATCH_FRAME);
        if (c) del_client(c, DEL_REMAP);
    }
    XFree(wins);

    XFreeFont(dpy, font);
#ifdef X_HAVE_UTF8_STRING
    XFreeFontSet(dpy, font_set);
#endif
#ifdef XFT
    XftFontClose(dpy, xftfont);
#endif
    XFreeCursor(dpy, map_curs);
    XFreeCursor(dpy, move_curs);
    XFreeCursor(dpy, resize_curs);
    XFreeGC(dpy, invert_gc);
    XFreeGC(dpy, border_gc);
    XFreeGC(dpy, string_gc);

    XInstallColormap(dpy, DefaultColormap(dpy, screen));
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);

    XDeleteProperty(dpy, root, net_supported);
    XDeleteProperty(dpy, root, net_client_list);

    XCloseDisplay(dpy);
    exit(0);
}
