/*
*      event.c
*      Copyright © 2008, 2009 Martin Duquesnoy <xorg62@gmail.com>
*      All rights reserved.
*
*      Redistribution and use in source and binary forms, with or without
*      modification, are permitted provided that the following conditions are
*      met:
*
*      * Redistributions of source code must retain the above copyright
*        notice, this list of conditions and the following disclaimer.
*      * Redistributions in binary form must reproduce the above
*        copyright notice, this list of conditions and the following disclaimer
*        in the documentation and/or other materials provided with the
*        distribution.
*      * Neither the name of the  nor the names of its
*        contributors may be used to endorse or promote products derived from
*        this software without specific prior written permission.
*
*      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*      "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*      LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*      A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
*      OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*      SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*      LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*      DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*      THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*      (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*      OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "wmfs.h"

#define EVDPY (e->xany.display)
#define MAX_EV 256

/** Check mouse bind condition and execute associated function
 */
static void
do_mousebind(int screen, uint button, int n, MouseBinding m[])
{
     int i = 0;

     for(; i < n; ++i)
     {
          if(m[i].screen == screen || m[i].screen < 0)   /* Screen   */
               if(m[i].tag == seltag[i] || m[i].tag < 0) /* Tag      */
                    if(m[i].button == button)            /* Button   */
                         if(m[i].func)                   /* Function */
                              m[i].func(m[i].cmd);
     }

     return;
}

/** ButtonPress handle event
*/
static void
buttonpress(XEvent *e)
{
     XButtonEvent *ev = &e->xbutton;
     StatusMouse *sm;
     InfoBar *ib;
     Client *c;
     int i, n;

     screen_get_sel();

     ib = &infobar[selscreen];

     /* If the mouse is on a not selected client and you click on it. */
     if(((c = client_gb_win(ev->window)) || (c = client_gb_titlebar(ev->window)))
               && c != sel && ev->button >= Button1 && ev->button <= Button3)
     {
          client_focus(c);
          client_raise(c);

          return;
     }

     /* Titlebar */
     if((c = client_gb_titlebar(ev->window)) && c == sel)
          do_mousebind(selscreen, ev->button, conf.titlebar.nmouse, conf.titlebar.mouse);

     /* Titlebar buttons */
     if((c = client_gb_button(ev->window, &n)))
          for(i = 0; i < conf.titlebar.button[n].nmouse; ++i)
               if(ev->button == conf.titlebar.button[n].mouse[i].button)
                    if(conf.titlebar.button[n].mouse[i].func)
                    {
                         client_focus(c);
                         conf.titlebar.button[n].mouse[i].func(conf.titlebar.button[n].mouse[i].cmd);
                    }

     /* Frame Resize Area */
     if((c = client_gb_resize(ev->window)))
          mouse_resize(c);

     /* Client */
     if((c = client_gb_win(ev->window)) && c == sel)
          do_mousebind(selscreen, ev->button, conf.client.nmouse, conf.client.mouse);

     /* Root */
     if(ev->window == ROOT)
          do_mousebind(selscreen, ev->button, conf.root.nmouse, conf.root.mouse);

     /* Infobars */
     for(i = 0; i < screen_count(); ++i)
          if(ev->window == infobar[i].bar->win)
               do_mousebind(i, ev->button, conf.bars.nmouse, conf.bars.mouse);

     /* Selbar */
     if(conf.bars.selbar && ev->window == ib->bar->win)
          if(INAREA(ev->x, ev->y, ib->selbar_geo))
               do_mousebind(selscreen, ev->button, conf.selbar.nmouse, conf.selbar.mouse);

     /* Tags */
     if(ib->tags_board)
     {
          for(i = 1; i < conf.ntag[selscreen] + 1; ++i)
               if(ib->tags[i] && ev->window == ib->tags[i]->win)
               {
                    do_mousebind(selscreen, ev->button, tags[selscreen][i].nmouse, tags[selscreen][i].mouse);

                    /* Mouse button action on tag */
                    if(ev->button == conf.mouse_tag_action[TagSel])
                         tag_set(i);
                    else if(ev->button == conf.mouse_tag_action[TagTransfert])
                         tag_transfert(sel, i);
                    else if(ev->button == conf.mouse_tag_action[TagAdd])
                         tag_additional(selscreen, seltag[selscreen], i);
                    else if(ev->button == conf.mouse_tag_action[TagNext])
                         tag_set(seltag[selscreen] + 1);
                    else if(ev->button == conf.mouse_tag_action[TagPrev])
                         tag_set(seltag[selscreen] - 1);
               }
     }

     /* Layout button */
     if(ib->layout_button && ev->window == ib->layout_button->win && conf.nlayout > 1)
     {
          if(conf.layout_system && (ev->button == Button1 || ev->button == Button3)) /* True -> menu */
          {
               menulayout.y = spgeo[selscreen].y + ib->layout_button->geo.y + INFOBARH;
               menulayout.x = ib->layout_button->geo.x + (sgeo[selscreen].x - BORDH);

               if(ib->geo.y != spgeo[selscreen].y)
                    menulayout.y = ib->geo.y - (INFOBARH * menulayout.nitem) - SHADH;

               uicb_menu("menulayout");
          }
          else
               layoutswitch(ev->button == Button1 || ev->button == Button4);
     }

     /* Status mouse bindings */
     SLIST_FOREACH(sm, &smhead, next)
          if(sm->infobar->bar->win == ev->window && ev->button == sm->button)
               if(INAREA(ev->x, ev->y, sm->area))
                    if(sm->func)
                         sm->func(sm->cmd);

     return;
}

/* ClientMessage handle event
*/
static void
clientmessageevent(XEvent *e)
{
     XClientMessageEvent *ev = &e->xclient;
     Client *c;
     InfoBar *ib;
     Systray *sy;
     int s, mess_t = 0;
     Atom rt;
     int rf;
     ulong ir, il;
     uchar *ret = NULL;
     uchar *ret_cmd = NULL;
     void (*func)(uicb_t);

     if(ev->format != 32)
          return;

     s = screen_count();

     while(mess_t < net_last + s && net_atom[mess_t] != ev->message_type)
          ++mess_t;

     if(ev->window == ROOT)
     {
          /* Manage _NET_CURRENT_DESKTOP */
          if(mess_t == net_current_desktop
             && ev->data.l[0] >= 0
             && ev->data.l[0] < conf.ntag[selscreen])
               tag_set((int)(ev->data.l[0] + 1));

          /* Manage _WMFS_SET_SCREEN */
          if(mess_t == wmfs_set_screen
             && ev->data.l[0] >= 0
             && ev->data.l[0] <= s)
               screen_set_sel((int)(ev->data.l[0]));

          /* Manage _NET_ACTIVE_WINDOW */
          else if(mess_t == net_active_window)
          {
               if((c = client_gb_win(ev->window)))
                    client_focus(c);
               else if((sy = systray_find(ev->data.l[0])))
                    XSetInputFocus(EVDPY, sy->win, RevertToNone, CurrentTime);
          }
     }
     else if(ev->window == traywin)
     {
          /* Manage _NET_WM_SYSTEM_TRAY_OPCODE */
          if(mess_t == net_wm_system_tray_opcode)
          {
               if(ev->data.l[1] == XEMBED_EMBEDDED_NOTIFY)
               {
                    systray_add(ev->data.l[2]);
                    systray_update();
               }
               else if(ev->data.l[1] == XEMBED_REQUEST_FOCUS)
                    if((sy = systray_find(ev->data.l[2])))
                         ewmh_send_message(sy->win, sy->win, "_XEMBED",
                                   XEMBED_FOCUS_IN, XEMBED_FOCUS_CURRENT, 0, 0, 0);
          }
     }

     /* Manage _NET_WM_STATE */
     if(mess_t == net_wm_state)
          if((c = client_gb_win(ev->window)))
               ewmh_manage_net_wm_state(ev->data.l, c);

     /* Manage _NET_CLOSE_WINDOW */
     if(mess_t == net_close_window)
          if((c = client_gb_win(ev->window)))
               client_kill(c);

     /* Manage _NET_WM_DESKTOP */
     if(mess_t == net_wm_desktop)
          if((c = client_gb_win(ev->window)) && ev->data.l[0] != (long)0xFFFFFFFF)
               tag_transfert(c, ev->data.l[0]);

     if(ev->data.l[4])
     {
          /* Manage _WMFS_STATUSTEXT_x */
          if(mess_t >= wmfs_statustext)
          {
               if(XGetWindowProperty(EVDPY, ROOT, net_atom[mess_t], 0, 4096,
                              False, net_atom[utf8_string], &rt, &rf, &ir, &il, &ret) == Success)
               {
                    ib = &infobar[mess_t - wmfs_statustext];
                    free(ib->statustext);
                    ib->statustext = xstrdup((char*)ret);
                    _infobar_draw(ib);
                    XFree(ret);
               }
          }

          /* Manage _WMFS_FUNCTION && _WMFS_CMD */
          if(mess_t == wmfs_function || mess_t == wmfs_cmd)
          {
               XGetWindowProperty(EVDPY, ROOT, net_atom[wmfs_function], 0, 4096,
                         False, net_atom[utf8_string], &rt, &rf, &ir, &il, &ret);
               XGetWindowProperty(EVDPY, ROOT, net_atom[wmfs_cmd], 0, 4096,
                         False, net_atom[utf8_string], &rt, &rf, &ir, &il, &ret_cmd);

               if((func = name_to_func((char*)ret, func_list)))
                    func((uicb_t)ret_cmd);

               XFree(ret_cmd);
               XFree(ret);
          }
     }

     /* Manage _WMFS_UPDATE_HINTS */
     if(mess_t == wmfs_update_hints)
     {
          ewmh_get_number_of_desktop();
          ewmh_update_current_tag_prop();
          ewmh_get_client_list();
          ewmh_get_desktop_names();
          ewmh_set_desktop_geometry();
          screen_count();
          screen_get_sel();
     }

     return;
}

/** ConfigureRequesthandle events
*/
static void
configureevent(XEvent *e)
{
     XConfigureRequestEvent *ev = &e->xconfigurerequest;
     XWindowChanges wc;
     Client *c;

     /* Check part */
     if(((c = client_gb_win(ev->window)) || (c = client_gb_win(ev->window)))
               && (c->flags & (LMaxFlag | MaxFlag | FSSFlag)))
          return;

     if((c = client_gb_win(ev->window)))
     {
          if(ev->value_mask & CWX)
               c->geo.x = ev->x + BORDH;
          if(ev->value_mask & CWY)
               c->geo.y = ev->y + TBARH;
          if(ev->value_mask & CWWidth)
               c->geo.width = ev->width;
          if(ev->value_mask & CWHeight)
               c->geo.height = ev->height;

          if(c->flags & FreeFlag || !(c->flags & TileFlag))
               client_moveresize(c, c->geo, False);
          else
          {
               client_configure(c);
               arrange(c->screen, True);
          }
     }
     else
     {
          wc.x            = ev->x;
          wc.y            = ev->y;
          wc.width        = ev->width;
          wc.height       = ev->height;
          wc.border_width = ev->border_width;
          wc.sibling      = ev->above;
          wc.stack_mode   = ev->detail;

          XConfigureWindow(EVDPY, ev->window, ev->value_mask, &wc);
     }

     return;
}

/** DestroyNotify handle event
*/
static void
destroynotify(XEvent *e)
{
     XDestroyWindowEvent *ev = &e->xdestroywindow;
     Client *c;
     Systray *s;

     if((c = client_gb_win(ev->window)))
     {
          client_unmanage(c);
          XSetErrorHandler(errorhandler);
     }
     else if((s = systray_find(ev->window)))
     {
          setwinstate(s->win, WithdrawnState);
          systray_del(s);
          systray_update();
     }

     return;
}

/** EnterNotify handle event
*/
static void
enternotify(XEvent *e)
{
     XCrossingEvent *ev = &e->xcrossing;
     Client *c;
     int n;

     if((ev->mode != NotifyNormal || ev->detail == NotifyInferior)
               && ev->window != ROOT)
          return;

     if(tags[selscreen][seltag[selscreen]].flags & IgnoreEnterFlag)
     {
          tags[selscreen][seltag[selscreen]].flags &= ~IgnoreEnterFlag;
          return;
     }

     /* Don't handle EnterNotify event if it's about systray */
     if(systray_find(ev->window) || ev->window == traywin)
          return;

     if(conf.focus_fmouse)
     {
          if((c = client_gb_win(ev->window))
                    || (c = client_gb_frame(ev->window))
                    || (c = client_gb_titlebar(ev->window))
                    || (c = client_gb_button(ev->window, &n)))
               client_focus(c);
          else
               client_focus(NULL);
     }


     return;
}

/** ExposeEvent handle event
*/
static void
expose(XEvent *e)
{
     XExposeEvent *ev = &e->xexpose;
     Client *c;
     BarWindow *bw;

     /* BarWindows */
     SLIST_FOREACH(bw, &bwhead, next)
          if(ev->window == bw->win)
          {
               barwin_refresh(bw);
               break;
          }

     /* Client frame */
     if((c = client_gb_titlebar(ev->window)))
          frame_update(c);

     return;
}

/** FocusChange handle event
*/
static void
focusin(XEvent *e)
{
     if(sel && e->xfocus.window != sel->win)
          client_focus(sel);

     return;
}

/** KeyPress handle event
*/
static void
keypress(XEvent *e)
{
     XKeyPressedEvent *ev = &e->xkey;
     KeySym keysym;
     int i;

     keysym = XKeycodeToKeysym(EVDPY, (KeyCode)ev->keycode, 0);
     for(i = 0; i < conf.nkeybind; ++i)
          if(keysym == keys[i].keysym
             && (keys[i].mod & ~(numlockmask | LockMask))
             == (ev->state & ~(numlockmask | LockMask))
             && keys[i].func)
               keys[i].func(keys[i].cmd);

     return;
}

/** MappingNotify handle event
*/
static void
mappingnotify(XEvent *e)
{
     XMappingEvent *ev = &e->xmapping;
     XRefreshKeyboardMapping(ev);

     if(ev->request == MappingKeyboard)
          grabkeys();

     return;
}

/** MapNotify handle event
  */
static void
mapnotify(XEvent *e)
{
     XMapEvent *ev = &e->xmap;
     Client *c;
     Systray *s;

     if(ev->window != ev->event && !ev->send_event)
          return;

     if((c = client_gb_win(ev->window)))
          setwinstate(c->win, NormalState);
     else if((s = systray_find(ev->window)))
     {
          setwinstate(s->win, NormalState);
          ewmh_send_message(s->win, s->win, "_XEMBED", CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, 0, 0);
     }

     return;
}

/** MapRequest handle event
*/
static void
maprequest(XEvent *e)
{
     XMapRequestEvent *ev = &e->xmaprequest;
     XWindowAttributes at;
     Systray *s;

     CHECK(XGetWindowAttributes(EVDPY, ev->window, &at));
     CHECK(!at.override_redirect);

     if((s = systray_find(ev->window)))
     {
          ewmh_send_message(s->win, s->win, "_XEMBED", CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, 0, 0);
          systray_update();
     }
     else if(!client_gb_win(ev->window))
          client_manage(ev->window, &at, True);

     return;
}

/** PropertyNotify handle event
*/
static void
propertynotify(XEvent *e)
{
     XPropertyEvent *ev = &e->xproperty;
     Client *c;
     Systray *s;
     Window trans;
     XWMHints *h;

     if(ev->state == PropertyDelete)
          return;

     if((s = systray_find(ev->window)))
     {
          systray_state(s);
          systray_update();
     }

     if((c = client_gb_win(ev->window)))
     {
          switch(ev->atom)
          {
          case XA_WM_TRANSIENT_FOR:
               XGetTransientForHint(EVDPY, c->win, &trans);
               if((c->flags & (TileFlag | MaxFlag)) && client_gb_win(trans))
                    arrange(c->screen, True);
               break;
          case XA_WM_NORMAL_HINTS:
               client_size_hints(c);
               break;
          case XA_WM_HINTS:
               if((h = XGetWMHints(EVDPY, c->win)) && (h->flags & XUrgencyHint) && c != sel)
               {
                    client_urgent(c, True);
                    XFree(h);
               }
               break;
          case XA_WM_NAME:
               client_get_name(c);
               break;
          default:
               if(ev->atom == net_atom[net_wm_name])
                    client_get_name(c);
               break;
          }
     }

     return;
}

/** XReparentEvent handle event
 */
static void
reparentnotify(XEvent *ev)
{
     (void)ev;

     return;
}

/** SelectionClearEvent handle event
 */
static void
selectionclearevent(XEvent *ev)
{
     /* Getting selection if lost it */
     if(ev->xselectionclear.window == traywin)
          systray_acquire();

     systray_update();

     return;
}

/** UnmapNotify handle event
 */
static void
unmapnotify(XEvent *e)
{
     XUnmapEvent *ev = &e->xunmap;
     Client *c;
     Systray *s;

     if((c = client_gb_win(ev->window))
        && ev->send_event
        && !(c->flags & HideFlag))
     {
          client_unmanage(c);
          XSetErrorHandler(errorhandler);
     }

     if((s = systray_find(ev->window)))
     {
          systray_del(s);
          systray_update();
     }

     return;
}

/** XMotionNotify handle event
 */
static void
motionnotify(XEvent *e)
{
     XMotionEvent *ev = &e->xmotion;
     Client *c;

     if(!conf.focus_fmouse || !conf.focus_fmov)
          return;

     if((c = client_gb_win(ev->subwindow)))
          if(c != sel)
               client_focus(c);

     return;
}

/** XRandr handle event
 */
#ifdef HAVE_XRANDR
void
xrandrevent(XEvent *e)
{
     /* Update xrandr configuration */
     if(!XRRUpdateConfiguration(e))
          return;

     /* Reload WMFS to update the screen(s) geometry changement */
     quit();
     for(; argv_global[0] && argv_global[0] == ' '; ++argv_global);
     execvp(argv_global, all_argv);

}
#endif /* HAVE_XRANDR */

/** Key grabbing function
*/
void
grabkeys(void)
{
     int i;
     KeyCode code;

     XUngrabKey(dpy, AnyKey, AnyModifier, ROOT);
     for(i = 0; i < conf.nkeybind; ++i)
          if((code = XKeysymToKeycode(dpy, keys[i].keysym)))
          {
               XGrabKey(dpy, code, keys[i].mod, ROOT, True, GrabModeAsync, GrabModeAsync);
               XGrabKey(dpy, code, keys[i].mod | LockMask, ROOT, True, GrabModeAsync, GrabModeAsync);
               XGrabKey(dpy, code, keys[i].mod | numlockmask, ROOT, True, GrabModeAsync, GrabModeAsync);
               XGrabKey(dpy, code, keys[i].mod | LockMask | numlockmask, ROOT, True, GrabModeAsync, GrabModeAsync);
          }

     return;
}

/** Make event handle function pointer array
*/
void
event_make_array(void)
{
     int i = MAX_EV;

     event_handle = xcalloc(MAX_EV, sizeof(event_handle));

     /* Fill array with non-used function (do nothing) */
     while(i--)
          event_handle[i] = reparentnotify;

     event_handle[ButtonPress]      = buttonpress;
     event_handle[ClientMessage]    = clientmessageevent;
     event_handle[ConfigureRequest] = configureevent;
     event_handle[DestroyNotify]    = destroynotify;
     event_handle[EnterNotify]      = enternotify;
     event_handle[Expose]           = expose;
     event_handle[FocusIn]          = focusin;
     event_handle[KeyPress]         = keypress;
     event_handle[MapNotify]        = mapnotify;
     event_handle[MapRequest]       = maprequest;
     event_handle[MappingNotify]    = mappingnotify;
     event_handle[MotionNotify]     = motionnotify;
     event_handle[PropertyNotify]   = propertynotify;
     event_handle[ReparentNotify]   = reparentnotify;
     event_handle[SelectionClear]   = selectionclearevent;
     event_handle[UnmapNotify]      = unmapnotify;

#ifdef HAVE_XRANDR
     event_handle[xrandr_event + RRScreenChangeNotify] = xrandrevent;
#endif /* HAVE_XRANDR */

     return;
}


