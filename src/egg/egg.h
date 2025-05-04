/* egg.h
 * Egg Platform API.
 */
 
#ifndef EGG_H
#define EGG_H

#include <stdint.h>

/* Client entry points.
 * These are not part of the Egg Platform API.
 * Rather, they are the functions each client -- you -- must implement, for Egg to call.
 *****************************************************************************************/

/* Last call the platform will make.
 * (status) nonzero means abnormal, something failed.
 * Usually this is unused. But if you defer saving games, good time to poke it.
 * This *does* normally get called if egg_client_init() reports an error.
 */
void egg_client_quit(int status);

/* First call the platform will make.
 * If you return <0, no update or render calls will be made, and the platform will shut down.
 */
int egg_client_init();

/* Called repeatedly while running.
 * (elapsed) is the time in seconds since the last update, or a made-up number the first time.
 * It has already been conditioned by the platform, will always be close to 16.6 ms.
 * During each update, you should poll for input and then adjust your model state.
 */
void egg_client_update(double elapsed);

/* Called repeatedly while running.
 * Normally there's one render after each update but that's not strictly guaranteed.
 * If running headless, render might happen intermittently or not at all.
 * Do not change your model state or make any assumptions about timing -- those are "update" concerns.
 */
void egg_client_render();

/* Global odds and ends.
 ***********************************************************************************/

/* Request platform to terminate at the next opportunity.
 * This function usually does return; termination will happen at the end of the update cycle.
 * But platforms are allowed to terminate immediately without returning, if that makes more sense for them.
 */
void egg_terminate(int status);

/* Dump some text to the debug console.
 * Do not include a newline. We'll strip it if you do.
 * This text will probably not be seen by users, but it's not secret either.
 */
void egg_log(const char *msg);

/* Current real time in seconds from some undefined epoch.
 */
double egg_time_real();

/* Populate (dst) with the local time, represented as up to 7 integers:
 *   [year,month,day,hour,minute,second,millisecond]
 * Everything is formatted for display, in particular (month) is 1-based, the way humans like it.
 */
void egg_time_local(int *dst,int dsta);

/* Helpers for language codes.
 * We pass languages around as 10-bit integers, which encode a 2-letter ISO 631 code.
 */
#define EGG_LANG_FROM_STRING(str) ((((str)[0]-'a'+1)<<5)|((str)[1]-'a'+1))
#define EGG_STRING_FROM_LANG(dst,lang) { \
  (dst)[0]='a'+(((lang)>>5)&0x1f)-1; \
  (dst)[1]='a'+((lang)&0x1f)-1; \
}
#define EGG_STRING_IS_LANG(str) ( \
  ((str)[0]>='a')&&((str)[0]<='z')&&((str)[1]>='a')&&((str)[1]<='z') \
)

/* Read or write a global preference.
 * Do not change these unless the user has prompted you to.
 */
int egg_prefs_get(int k);
int egg_prefs_set(int k,int v);
#define EGG_PREF_LANG 1
#define EGG_PREF_MUSIC 2
#define EGG_PREF_SOUND 3

/* Storage.
 *************************************************************************************/

/* Copy the ROM to (dst) and return its length.
 * If it's longer than (dsta), copy what fits and return the full length.
 */
int egg_rom_get(void *dst,int dsta);

/* Copy one resource from the ROM.
 * Usually it makes more sense for clients to get the entire ROM at once and slice it client-side.
 * Utility libraries might need this piecemeal helper instead.
 */
int egg_rom_get_res(void *dst,int dsta,int tid,int rid);

/* Access to persistent key=value store.
 * Key and value are not terminated and length is always required.
 * Missing and empty fields are indistinguishable.
 * User may be able to turn off saving. If that's so, the platform won't pretend to save anything.
 * Keys must be 1..255 of G0 (space allowed), and values 0..65535 of UTF-8.
 * You do not need to qualify your keys per game; the platform does that.
 */
int egg_store_get(char *v,int va,const char *k,int kc);
int egg_store_set(const char *k,int kc,const char *v,int vc);
int egg_store_key_by_index(char *k,int ka,int p);

/* Standard resource types.
 * See etc/doc/rom-format.md for details.
 */
#define EGG_TID_metadata 1
#define EGG_TID_code 2
#define EGG_TID_strings 3
#define EGG_TID_image 4
#define EGG_TID_song 5
#define EGG_TID_sound 6
#define EGG_TID_tilesheet 7
#define EGG_TID_decalsheet 8
#define EGG_TID_map 9
#define EGG_TID_sprite 10
#define EGG_TID_FOR_EACH \
  _(metadata) \
  _(code) \
  _(strings) \
  _(image) \
  _(song) \
  _(sound) \
  _(tilesheet) \
  _(decalsheet) \
  _(map) \
  _(sprite)
  
/* Input.
 *******************************************************************************/

/* Enter the interactive input configuration mode.
 * Your game will stop updating after this frame, and resume at some point in the future.
 * This configuration is only pertinent to mapped gamepads.
 */
void egg_input_configure();

/* Recommended input method is mapped gamepads.
 * These can be sourced from the keyboard too.
 * Devices are indexed by playerid, with playerid zero being an aggregate of all the other states.
 * Each state is 16 independent bits.
 */
void egg_input_get_all(int *statev,int statea);
int egg_input_get_one(int playerid);

/* Buttons for mapped gamepads are defined in very nearly the same order as the W3C Standard Mapping Gamepad.
 * We don't have LP or RP buttons, so I moved AUX3 and CD into those positions.
 */
#define EGG_BTN_SOUTH     0x0001 /* Right thumb... */
#define EGG_BTN_EAST      0x0002
#define EGG_BTN_WEST      0x0004
#define EGG_BTN_NORTH     0x0008
#define EGG_BTN_L1        0x0010 /* Triggers... 1 is nearer the face. */
#define EGG_BTN_R1        0x0020
#define EGG_BTN_L2        0x0040
#define EGG_BTN_R2        0x0080
#define EGG_BTN_AUX2      0x0100 /* Select. Left-side auxiliary. */
#define EGG_BTN_AUX1      0x0200 /* Start. Right-side auxiliary. */
#define EGG_BTN_AUX3      0x0400 /* Heart. Center auxiliary. NB 16 in standard mapping; this slot is LP in the spec. */
#define EGG_BTN_CD        0x0800 /* Carrier detect, always nonzero if connected. NB not part of standard mapping; this slot in RP in the spec. */
#define EGG_BTN_UP        0x1000 /* Dpad... */
#define EGG_BTN_DOWN      0x2000
#define EGG_BTN_LEFT      0x4000
#define EGG_BTN_RIGHT     0x8000

struct egg_event {
  int type;
  union {
    struct { int keycode,value; } key;
    struct { int codepoint; } text;
    struct { int x,y; } mmotion;
    struct { int x,y,btnid,value; } mbutton;
    struct { int x,y,dx,dy; } mwheel;
    struct { int x,y,touchid,state; } touch;
    struct { int devid,btnid,value; } gamepad;
  };
};

#define EGG_EVENT_KEY          1 /* Raw keyboard events (HID page 7). (value) 2 for auto-repeat. */
#define EGG_EVENT_TEXT         2 /* Keyboard events mapped to Unicode text. */
#define EGG_EVENT_MMOTION      3 /* Mouse motion. */
#define EGG_EVENT_MBUTTON      4 /* Mouse buttons. */
#define EGG_EVENT_MWHEEL       5 /* Mouse wheel. */
#define EGG_EVENT_TOUCH        6 /* Touch. (state) is (0,1,2)=(end,begin,continue) per (touchid). */
#define EGG_EVENT_GAMEPAD      7 /* Raw gamepads. (btnid) zero signals connect and disconnect. */
#define EGG_EVENT_HIDECURSOR   8 /* Not an event. Enable to hide the cursor, even if events are enabled. */
#define EGG_EVENT_LOCKCURSOR   9 /* Not an event. Hides the cursor and causes motion to report only in relative terms. */
#define EGG_EVENT_NOMAPCURSOR 10 /* Not an event. Enable to get mouse and touch events in window coordinates instead of framebuffer. */

/* Copy events off the queue.
 * This never returns more than (dsta).
 * If it returns exactly (dsta), you should process them then check for more.
 * Never blocks.
 * The event queue will not refill during an update.
 */
int egg_event_get(struct egg_event *dst,int dsta);

/* Enable or disable an event type or feature.
 * Enabling returns <0 if it's definitely not available.
 * If any mouse event is enabled, but not EGG_EVENT_HIDECURSOR or EGG_EVENT_LOCKCURSOR, the cursor will be visible.
 * If any keyboard event is enabled, keyboards will not map to standard gamepads.
 * If any gamepad event is enabled, raw gamepads will not map to standard gamepads.
 * (In general, use standard gamepads or events, not both).
 */
int egg_event_enable(int evttype,int enable);
int egg_event_is_enabled(int evttype);

/* Get the name and IDs for a gamepad device.
 * Or fetch its button declarations by index.
 * I really really advise you to use the standard mapping gamepads instead, it's so much easier.
 * But if you need analogue controls, or something exotic, here it is.
 */
int egg_gamepad_get_name(char *dst,int dsta,int *vid,int *pid,int *version,int devid);
int egg_gamepad_get_button(int *btnid,int *hidusage,int *lo,int *hi,int *rest,int devid,int btnix);

/* Audio.
 **********************************************************************************************/

/* (trim) in 0..1.
 * (pan) -1..0..1 = left..center..right
 */
void egg_play_sound(int soundid,double trim,double pan);

/* Request a (songid) that doesn't exist to play silence, eg 0.
 * If you request what's already playing, default is to do nothing.
 * (force) nonzero to start it over in that case.
 */
void egg_play_song(int songid,int force,int repeat);

/* Current song ID is zero if the song didn't exist or finished playing, regardless of what ID was requested.
 * When a song repeats, its playhead repeats too.
 * Behavior of setting to an OOB playhead is undefined, but will never cause the song to stop or anything crazy.
 * Playhead is in seconds.
 * The platform may impose a transition period between songs.
 * Anything you ask for during that period relates to the new song, even if it hasn't actually started yet.
 */
int egg_song_get_id();
double egg_song_get_playhead();
void egg_song_set_playhead(double playhead);

/* Video.
 ******************************************************************************************/
 
#define EGG_TEXTURE_SIZE_LIMIT 4096

/* You shouldn't normally care about the "screen" size.
 * That's the window your game is running in, usually substantially larger than the framebuffer.
 * It can matter, if you are using unmapped mouse or touch events.
 * Don't make any assumptions about how the framebuffer is mapped onto the screen size; use the provided transformers.
 */
void egg_video_get_screen_size(int *w,int *h);
void egg_video_fb_from_screen(int *x,int *y);
void egg_video_screen_from_fb(int *x,int *y);

/* Texture objects are identified by a unique positive integer.
 * You must delete any textures you create (don't need to at quit).
 * Texture ID 1 is created implicitly before init, and represents the framebuffer.
 * It can't be deleted or resized.
 */
void egg_texture_del(int texid);
int egg_texture_new();
void egg_texture_get_size(int *w,int *h,int texid);

/* Replace a texture with an image resource and mark it read-only.
 */
int egg_texture_load_image(int texid,int imageid);

/* Replace a texture with an RGBA image.
 * Or if (src,srcc)=(0,0), initialize the texture with undefined content.
 * Marks the texture read-write.
 * This is permitted against texture 1, only if (w,h) match its current size.
 */
int egg_texture_load_raw(int texid,int w,int h,int stride,const void *src,int srcc);

/* Copy RGBA image data out of an image.
 * (dsta) must be at least (w*h*4).
 * Fails if (dst) too small (does not return actual length, as you might expect).
 */
int egg_texture_get_pixels(void *dst,int dsta,int texid);

/* Clear to transparent black.
 */
void egg_texture_clear(int texid);

#define EGG_XFORM_XREV 1
#define EGG_XFORM_YREV 2
#define EGG_XFORM_SWAP 4

#define EGG_RENDER_POINTS           1 /* egg_render_raw */
#define EGG_RENDER_LINES            2 /* egg_render_raw */
#define EGG_RENDER_LINE_STRIP       3 /* egg_render_raw */
#define EGG_RENDER_TRIANGLES        4 /* egg_render_raw */
#define EGG_RENDER_TRIANGLE_STRIP   5 /* egg_render_raw */
#define EGG_RENDER_TILE             6 /* egg_render_tile */
#define EGG_RENDER_FANCY            7 /* egg_render_fancy */

struct egg_render_uniform {
  int mode;
  int dsttexid;
  int srctexid;
  uint32_t tint;
  uint8_t alpha;
};

struct egg_render_raw {
  int16_t x,y;
  int16_t tx,ty;
  uint8_t r,g,b,a;
};

struct egg_render_tile {
  int16_t x,y;
  uint8_t tileid;
  uint8_t xform;
};

struct egg_render_fancy {
  int16_t x,y;
  uint8_t tileid;
  uint8_t rotation;
  uint8_t size;
  uint8_t tr,tg,tb,ta; // Tint.
  uint8_t pr,pg,pb; // Primary color replacement, straight 0x80 is noop.
  uint8_t a; // Extra alpha multiplier.
};

/* Render one batch of primitives.
 * (uniform->mode) determines the expected format of each vertex.
 * (vtxc) is in BYTES, not vertices, as a validation mechanism.
 */
void egg_render(const struct egg_render_uniform *uniform,const void *vtxv,int vtxc);
  
#endif
