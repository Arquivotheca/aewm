/* aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>.
 * This program is free software; please see LICENSE for details. */

#ifndef AEWM_ATOM_H
#define AEWM_ATOM_H

#include <X11/Xlib.h>

struct strut {
    unsigned long left;
    unsigned long right;
    unsigned long top;
    unsigned long bottom;
};

typedef struct strut strut_t;

extern Atom utf8_string;
extern Atom wm_state;
extern Atom wm_change_state;
extern Atom wm_protos;
extern Atom wm_delete;
extern Atom net_supported;
extern Atom net_client_list;
extern Atom net_client_stack;
extern Atom net_active_window;
extern Atom net_close_window;
extern Atom net_cur_desk;
extern Atom net_num_desks;
extern Atom net_wm_name;
extern Atom net_wm_desk;
extern Atom net_wm_state;
extern Atom net_wm_state_shaded;
extern Atom net_wm_state_mv;
extern Atom net_wm_state_mh;
extern Atom net_wm_state_fs;
extern Atom net_wm_state_skipt;
extern Atom net_wm_state_skipp;
extern Atom net_wm_strut;
extern Atom net_wm_strut_partial;
extern Atom net_wm_wintype;
extern Atom net_wm_type_dock;
extern Atom net_wm_type_menu;
extern Atom net_wm_type_splash;
extern Atom net_wm_type_desk;

extern unsigned long get_atoms(Window, Atom, Atom, unsigned long,
    unsigned long *, unsigned long, unsigned long *);
extern unsigned long set_atoms(Window, Atom, Atom, unsigned long *,
    unsigned long);
extern unsigned long append_atoms(Window, Atom, Atom, unsigned long *,
    unsigned long);
extern void remove_atom(Window, Atom, Atom, unsigned long);
extern char *get_wm_name(Window);
int get_strut(Window, strut_t *);
extern unsigned long get_wm_state(Window);

#endif /* AEWM_ATOM_H */
