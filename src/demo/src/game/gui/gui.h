/* gui.h
 * I'm not generalizing widgets. Each widget type is its own unique thing.
 * This is not a great example for client apps:
 *  - You ought to use string ID+index instead of literal strings, to aid translation.
 *  - Usually you'll want a general "widget" type.
 *  - Lots of logic has to happen above us, at the controller level.
 */
 
#ifndef GUI_H
#define GUI_H

/* Contextless bits and bobs.
 ******************************************************************/

/* Render a single line of text.
 * (x,y) is the top-left corner of the first glyph.
 * Returns horizontal advancement in pixels, typically (srcc*8).
 * WARNING: Leaves graf's tint at (rgba).
 */
int gui_render_string(int x,int y,const char *src,int srcc,uint32_t rgba);
#define gui_string_h 8

/* List.
 * Text labels arranged vertically, with scrolling.
 * Intended to occupy the whole screen but you can give a smaller box too.
 *******************************************************************/
 
struct gui_list;

void gui_list_del(struct gui_list *list);

/* Bounds are fixed at construction.
 */
struct gui_list *gui_list_new(int x,int y,int w,int h);

/* Set a callback for optionid zero to set as a fallback.
 * Otherwise it is only called for the given optionid.
 * Callbacks don't care whether an option exists under that id, and don't get removed when their option does.
 */
void *gui_list_get_userdata(const struct gui_list *list);
void gui_list_set_userdata(struct gui_list *list,void *userdata);
void gui_list_cb_activate(struct gui_list *list,int optionid,void (*cb)(struct gui_list *list,int optionid));
void gui_list_cb_adjust(struct gui_list *list,int optionid,void (*cb)(struct gui_list *list,int optionid,int dx));
void gui_list_cb_focus(struct gui_list *list,int optionid,void (*cb)(struct gui_list *list,int optionid,int focus));

/* Insert at (p==-1) to append. Otherwise (p) must be valid.
 * Insert with (optionid) zero to have one selected monotonically.
 * All options have a unique positive optionid.
 */
int gui_list_insert(struct gui_list *list,int p,int optionid,const char *text,int textc,int enable);
int gui_list_remove(struct gui_list *list,int optionid);
int gui_list_replace(struct gui_list *list,int optionid,const char *text,int textc,int enable); // text (0,0) to leave unchanged. eg ("",0) to set empty.
int gui_list_optionid_by_index(const struct gui_list *list,int p);
int gui_list_index_by_optionid(const struct gui_list *list,int optionid);
int gui_list_get_selection(const struct gui_list *list); // => optionid
int gui_list_text_by_optionid(void *dstpp,const struct gui_list *list,int optionid);

void gui_list_update(struct gui_list *list,double elapsed,int input,int pvinput);
void gui_list_render(struct gui_list *list);

/* Terminal.
 * Uniform grid of 8x8-pixel glyphs.
 * Space and anything outside G0 is blank.
 * Doesn't use font. So it's a better choice for text that changes often.
 * Non-interactive.
 **********************************************************************/
 
struct gui_term;

void gui_term_del(struct gui_term *term);

struct gui_term *gui_term_new(int x,int y,int w,int h);

void gui_term_get_size(int *colc,int *rowc,const struct gui_term *term); // (w,h)/8, you can already know it.
void gui_term_set_background(struct gui_term *term,uint32_t rgba); // Default 0x000000ff.
void gui_term_set_foreground(struct gui_term *term,uint32_t rgba); // Default 0xffffffff. If alpha<1, our own background will show thru.
void gui_term_get_bounds(int *x,int *y,int *w,int *h,const struct gui_term *term,int col,int row,int colc,int rowc); // Clamps to main bounds.

/* Writing modifies multiple cells but only in one row.
 * We don't do line breaking.
 */
void gui_term_clear(struct gui_term *term);
void gui_term_write(struct gui_term *term,int x,int y,const char *src,int srcc);
void gui_term_writef(struct gui_term *term,int x,int y,const char *fmt,...);

/* Move content by (dx,dy). eg if you're appending a line at the bottom (0,-1).
 */
void gui_term_scroll(struct gui_term *term,int dx,int dy);

/* Announce your intention to edit (c) cells starting at (x,y).
 * If that fits, we mark ourselves dirty and return a pointer to it.
 * Cells are stored LRTB packed.
 */
char *gui_term_manual_edit(struct gui_term *term,int x,int y,int c);

void gui_term_update(struct gui_term *term,double elapsed);
void gui_term_render(struct gui_term *term);

#endif
