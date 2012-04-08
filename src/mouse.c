/*
*      mouse.c
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

Window prevwin;

/** Draw the border when a client in dragging/resizing with mouse
 */
static void
mouse_dragborder(Geo geo, GC g)
{
     XDrawRectangle(dpy, ROOT, g,
                    geo.x - (BORDH >> 1),
                    geo.y - (TBARH - (BORDH >> 1)),
                    geo.width + BORDH,
                    geo.height + TBARH);

     return;
}


static void
mouse_cfactor_border(Client *c, int f[4], GC g)
{
     int e;

     mouse_dragborder(cfactor_geo(c->wrgeo, f, &e), g);

     return;
}

/** Move a client in tile grid with the mouse
 *\param c Client double pointer
 */
static void
mouse_move_tile_client(Client **c)
{
     Client *sc;
     Window w;
     int d;

     if(!((*c)->flags & (TileFlag | LMaxFlag)))
          return;

     XQueryPointer(dpy, ROOT, &w, &w, &d, &d, &d, &d, (uint*)&d);

     if(((sc = client_gb_win(w)) || (sc = client_gb_frame(w)) || (sc = client_gb_titlebar(w)))
               && (*c)->win != sc->win && !((*c)->flags & HideFlag) && !(sc->flags & HideFlag) && (sc->flags & TileFlag))
     {
          client_swap(sc, *c);
          client_focus(sc);
          swap_ptr((void**)c, (void**)&sc);
     }

     return;

}

/** Move a client from one tag to another with dah mouse
 *\param c client pointer
 */
static void
mouse_move_tag_client(Client *c)
{
     Window w;
     int i, d, s;

     if(!(c->flags & (TileFlag | LMaxFlag)))
          return;

     s = c->screen;

     XQueryPointer(dpy, infobar[selscreen].tags_board->win, &w, &w, &d, &d, &d, &d, (uint*)&d);

     if(w == prevwin)
          return;

     prevwin = w;

     for(i = 1; i < conf.ntag[selscreen] + 1; ++i)
          if(infobar[selscreen].tags[i]->win == w
             && tags[selscreen][i].layout.func != freelayout)
          {
               tag_transfert(c, i);

               if(s != c->screen)
                    arrange(c->screen, True);
          }

     return;
}

/** Move the client with the mouse
 * \param c Client pointer
*/
static void
mouse_move(Client *c)
{
     int ocx, ocy, mx, my;
     int dint;
     uint duint;
     Window dw;
     Geo geo = c->geo;
     XGCValues xgc;
     GC gci;
     XEvent ev;

     if(c->flags & (MaxFlag | FSSFlag))
          return;

     ocx =  c->geo.x;
     ocy =  c->geo.y;

     if(XGrabPointer(dpy, ROOT, False, MouseMask, GrabModeAsync, GrabModeAsync,
                     None, cursor[CurMove], CurrentTime) != GrabSuccess)
          return;

     if(!(c->flags & TileFlag) && !(c->flags & LMaxFlag))
          XGrabServer(dpy);

     /* Set the GC for the rectangle */
     xgc.function = GXinvert;
     xgc.subwindow_mode = IncludeInferiors;
     xgc.line_width = BORDH;
     gci = XCreateGC(dpy, ROOT, GCFunction | GCSubwindowMode | GCLineWidth, &xgc);

     if(!(c->flags & TileFlag) && !(c->flags & LMaxFlag))
          mouse_dragborder(c->geo, gci);

     XQueryPointer(dpy, ROOT, &dw, &dw, &mx, &my, &dint, &dint, &duint);

     do
     {
          XMaskEvent(dpy, MouseMask | SubstructureRedirectMask, &ev);
          screen_get_sel();

          if(ev.type == MotionNotify)
          {
              mouse_move_tile_client(&c);
              mouse_move_tag_client(c);

               /* To move a client normally, in freelayout */
               if(!(c->flags & TileFlag) && !(c->flags & LMaxFlag))
               {
                    mouse_dragborder(geo, gci);

                    geo.x = (ocx + (ev.xmotion.x - mx));
                    geo.y = (ocy + (ev.xmotion.y - my));

                    /*
                     * Need to draw 2 times the same rectangle because
                     * it is draw with the revert color; revert + revert = normal
                     */
                    mouse_dragborder(geo, gci);
               }
          }
          else if((ev.type == MapRequest || ev.type == ConfigureRequest))
               HANDLE_EVENT(&ev);
     }
     while(ev.type != ButtonRelease);

     /* One time again to delete all the trace on the window */
     if(!(c->flags & (TileFlag | LMaxFlag)))
     {
          mouse_dragborder(geo, gci);
          client_moveresize(c, geo, False);
          frame_update(c);
          XUngrabServer(dpy);
     }
     client_update_attributes(c);
     XUngrabPointer(dpy, CurrentTime);
     XFreeGC(dpy, gci);

     return;
}

/** Resize a client with the mouse
 * \param c Client pointer
*/

void
mouse_resize(Client *c)
{
     Geo geo = c->geo, ogeo = c->geo;
     Position pos = Right;
     XEvent ev;
     Window w;
     int d, u, omx, omy;
     XGCValues xgc;
     GC gci;
     int f[4] = { 0 };

     if(c->flags & (MaxFlag | LMaxFlag | FSSFlag))
          return;

     XQueryPointer(dpy, ROOT, &w, &w, &omx, &omy, &d, &d, (uint *)&u);

     if((omx - c->geo.x) < (c->geo.width >> 1))
          pos = Left;

     if(XGrabPointer(dpy, ROOT, False, MouseMask, GrabModeAsync, GrabModeAsync, None,
                     cursor[((c->flags & TileFlag) ? CurResize : ((pos == Right) ? CurRightResize : CurLeftResize))],
                     CurrentTime) != GrabSuccess)
          return;

     if(!(c->flags & TileFlag))
          XGrabServer(dpy);

     /* Set the GC for the rectangle */
     xgc.function = GXinvert;
     xgc.subwindow_mode = IncludeInferiors;
     xgc.line_width = BORDH;
     gci = XCreateGC(dpy, ROOT, GCFunction | GCSubwindowMode | GCLineWidth, &xgc);

     if(!(c->flags & TileFlag))
     {
          if(pos == Right)
               XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->geo.width + conf.client.borderheight, c->geo.height);
          else
               XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, 0, c->geo.height);
          mouse_dragborder(c->geo, gci);
     }
     else
          mouse_cfactor_border(c, f, gci);

     do
     {
          XMaskEvent(dpy, MouseMask | SubstructureRedirectMask, &ev);

          if(ev.type == MotionNotify)
          {
               /* To resize client in tile mode with cfactor */
               if(c->flags & TileFlag)
               {
                    mouse_cfactor_border(c, f, gci);

                    if(omx >= c->frame_geo.x + (c->frame_geo.width >> 1))
                         f[Right] = ev.xmotion.x_root - omx;
                    else
                         f[Left] = omx - ev.xmotion.x_root;

                    if(omy >= c->frame_geo.y + (c->frame_geo.height >> 1))
                         f[Bottom] = ev.xmotion.y_root - omy;
                    else
                         f[Top] = omy - ev.xmotion.y_root;

                    mouse_cfactor_border(c, f, gci);
               }
               /* Free mode */
               else if(!(c->flags & TileFlag))
               {
                    mouse_dragborder(geo, gci);

                    if(pos == Right)
                    {
                         geo.width = ((ev.xmotion.x - c->geo.x < c->minw) ? c->minw : ev.xmotion.x - c->geo.x);
                         geo.height = ((ev.xmotion.y - c->geo.y < c->minh) ? c->minh : ev.xmotion.y - c->geo.y);
                    }
                    else
                    {
                         geo.x = (geo.width != c->maxw) ? c->geo.x - (c->geo.x - ev.xmotion.x) : geo.x;
                         geo.width  = ((c->geo.width + (c->geo.x - geo.x) < c->minw)
                                       ? c->minw && (geo.x = (c->geo.x + c->geo.width) - c->minw)
                                       : c->geo.width + (c->geo.x - geo.x));
                         geo.height = ((ev.xmotion.y - c->geo.y <= c->minh) ? c->minh : ev.xmotion.y - c->geo.y);
                    }

                    client_geo_hints(&geo, c);

                    mouse_dragborder((ogeo = geo), gci);

                    XSync(dpy, False);
               }
          }
     }
     while(ev.type != ButtonRelease);

     if(!(c->flags & TileFlag))
     {
          mouse_dragborder(ogeo, gci);
          client_moveresize(c, geo, True);
          frame_update(c);
          XUngrabServer(dpy);
     }
     else
     {
          mouse_cfactor_border(c, f, gci);
          cfactor_multi_set(c, f);
     }

     client_update_attributes(c);
     XUngrabPointer(dpy, CurrentTime);
     XFreeGC(dpy, gci);

     return;
}

/** Grab buttons
 * \param c Client pointer
 * \param focused For know if c is or not focused
*/
void
mouse_grabbuttons(Client *c, bool focused)
{
     size_t i = 0;
     uint but[] = {Button1, Button2, Button3, Button4, Button5};

     XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
     if(focused)
          for(; i < LEN(but); ++i)
          {
               XGrabButton(dpy, but[i], conf.client.mod, c->win, False,
                           ButtonMask, GrabModeAsync, GrabModeSync, None, None);
               XGrabButton(dpy, but[i], conf.client.mod | LockMask, c->win, False,
                           ButtonMask, GrabModeAsync, GrabModeSync, None, None);
               XGrabButton(dpy, but[i], conf.client.mod | numlockmask, c->win, False,
                         ButtonMask, GrabModeAsync, GrabModeSync, None, None);
               XGrabButton(dpy, but[i], conf.client.mod | LockMask | numlockmask, c->win, False,
                           ButtonMask, GrabModeAsync, GrabModeSync, None, None);
          }
     else
          XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
                      ButtonMask, GrabModeAsync, GrabModeSync, None, None);

     return;
}

/** Move the selected client
 * \param cmd uicb_t type unused
*/
void
uicb_mouse_move(uicb_t cmd)
{
     (void)cmd;
     CHECK(sel);

     mouse_move(sel);

     return;
}

/** Reisze the selected client
 * \param cmd uicb_t type unused
*/
void
uicb_mouse_resize(uicb_t cmd)
{
     (void)cmd;
     CHECK(sel);

     mouse_resize(sel);

     return;
}


