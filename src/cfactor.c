/*
*      cfactor.c
*      Copyright © 2011 Martin Duquesnoy <xorg62@gmail.com>
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

#define CLIENT_RESIZE_DIR(d)         \
void                                 \
uicb_client_resize_##d(uicb_t cmd)   \
{                                    \
    CHECK(sel);                      \
    cfactor_set(sel, d, atoi(cmd));  \
}

/* uicb_client_resize_dir() */
CLIENT_RESIZE_DIR(Right);
CLIENT_RESIZE_DIR(Left);
CLIENT_RESIZE_DIR(Top);
CLIENT_RESIZE_DIR(Bottom);

/** Clean client tile factors
  *\param c Client pointer
*/
void
cfactor_clean(Client *c)
{
     CHECK(c);

     if(!(tags[c->screen][c->tag].flags & (SplitFlag | CleanFactFlag)))
          return;

     c->tilefact[Right] = c->tilefact[Left]   = 0;
     c->tilefact[Top]   = c->tilefact[Bottom] = 0;

     return;
}

/** Return new geo of client with factors applied
  *\param c Client pointer
  *\return geo
*/
Geo
cfactor_geo(Geo geo, int fact[4], int *err)
{
     Geo cgeo = geo;

     *err = 0;

     /* Right factor */
     cgeo.width += fact[Right];

     /* Left factor */
     cgeo.x -= fact[Left];
     cgeo.width += fact[Left];

     /* Top factor */
     cgeo.y -= fact[Top];
     cgeo.height += fact[Top];

     /* Bottom factor */
     cgeo.height += fact[Bottom];

     /* Too big/small */
     if(cgeo.width > sgeo[selscreen].width || cgeo.height > sgeo[selscreen].height
               || cgeo.width < (BORDH << 1) || cgeo.height < (BORDH + TBARH))
     {
          *err = 1;
          return geo;
     }

     return cgeo;
}

/** Get c parents of row and resize
  *\param c Client pointer
  *\param p Direction of resizing
  *\param fac Factor of resizing
*/
static void
_cfactor_arrange_row(Client *c, Position p, int fac)
{
     Geo cgeo = c->frame_geo;
     Client *cc = tiled_client(c->screen, SLIST_FIRST(&clients));

     /* Travel clients to search parents of row and apply fact */
     for(; cc; cc = tiled_client(c->screen, SLIST_NEXT(cc, next)))
          if(CFACTOR_PARENTROW(cgeo, cc->frame_geo, p))
          {
               cc->tilefact[p] += fac;
               client_moveresize(cc, cc->geo, (tags[cc->screen][cc->tag].flags & ResizeHintFlag));
          }

     return;
}

/** Get c parents of row and check geo with futur resize factor
  *\param c Client pointer
  *\param p Direction of resizing
  *\param fac Factor of resizing
  *\return False in case of error
*/
static bool
_cfactor_check_geo_row(Client *c, Position p, int fac)
{
     Geo cgeo = c->frame_geo;
     Client *cc = tiled_client(c->screen, SLIST_FIRST(&clients));
     int e, f[4] = { 0 };

     f[p] += fac;

     /* Travel clients to search parents of row and check geos */
     for(; cc; cc = tiled_client(c->screen, SLIST_NEXT(cc, next)))
          if(CFACTOR_PARENTROW(cgeo, cc->frame_geo, p))
          {
               (Geo)cfactor_geo(cc->wrgeo, f, &e);
               if(e)
                    return False;
          }

     return True;
}

/* Resize only 2 client with applied factor
   *\param c1 Client pointer
   *\param c2 Client pointer
   *\param p Direction of resizing
   *\param fac Facotr
*/
static void
cfactor_arrange_two(Client *c1, Client *c2, Position p, int fac)
{
     c1->tilefact[p] += fac;
     c2->tilefact[RPOS(p)] -= fac;

     /* Needed in case of padding */
     if(conf.client.padding)
     {
           c1->flags |= FLayFlag;
           c2->flags |= FLayFlag;
     }

     client_moveresize(c1, c1->geo, (tags[c1->screen][c1->tag].flags & ResizeHintFlag));
     client_moveresize(c2, c2->geo, (tags[c2->screen][c2->tag].flags & ResizeHintFlag));

     return;

}

/** Get c parents of row and resize, exception checking same size before arranging row
  *\param c Client pointer
  *\param gc Client pointer
  *\param p Direction of resizing
  *\param fac Factor of resizing
*/
static void
cfactor_arrange_row(Client *c, Client *gc, Position p, int fac)
{
     if(CFACTOR_CHECK2(c->frame_geo, gc->frame_geo, p))
          cfactor_arrange_two(c, gc, p, fac);
     else
     {
          _cfactor_arrange_row(c, p, fac);
          _cfactor_arrange_row(gc, RPOS(p), -fac);
     }

     return;
}

/** Check future geometry of factorized client
  *\param c Client pointer
  *\param g Client pointer
  *\param p Direction of resizing
  *\param fac Factor of resizing
 */
static bool
cfactor_check_geo(Client *c, Client *g, Position p, int fac)
{
     int e, ee;
     int cf[4] = { 0 }, gf[4] = { 0 };

     /* Check c & g first */
     cf[p] += fac;
     gf[RPOS(p)] -= fac;

     (Geo)cfactor_geo(c->geo, cf, &e);
     (Geo)cfactor_geo(g->geo, gf, &ee);

     /* Size failure */
     if(e || ee || !_cfactor_check_geo_row(c, p, fac)
               || !_cfactor_check_geo_row(g, RPOS(p), -fac))
          return False;

     return True;
}

/** Manual resizing of tiled clients
  * \param c Client pointer
  * \param p Direction of resizing
  * \param fac Factor of resizing
*/
void
cfactor_set(Client *c, Position p, int fac)
{
     Client *gc = NULL;

     if(!c || !(c->flags & TileFlag) || p > Bottom)
          return;

     /* Get next client with direction of resize */
     gc = client_get_next_with_direction(c, p);

     if(!gc || c->screen != gc->screen)
          return;

     /* Check size */
     if(!cfactor_check_geo(c, gc, p, fac))
          return;

     /* Arrange client and row parents */
     cfactor_arrange_row(c, gc, p, fac);

     /* Enable split with cfactor_enable_split option */
     if(conf.cfactor_enable_split
               && !(tags[c->screen][c->tag].flags & SplitFlag))
     {
          tags[c->screen][c->tag].flags |= SplitFlag;
          infobar_draw_layout(&infobar[c->screen]);
     }

     return;
}

/** Apply a complete factor array to a client
  * \param c Client pointer
  * \param fac factor array
*/
void
cfactor_multi_set(Client *c, int fac[4])
{
     if(!c)
          return;

     cfactor_set(c, Right,  fac[Right]);
     cfactor_set(c, Left,   fac[Left]);
     cfactor_set(c, Top,    fac[Top]);
     cfactor_set(c, Bottom, fac[Bottom]);

     return;
}


