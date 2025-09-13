/* shared_symbols.h
 * This file is consumed by eggdev and editor, in addition to compiling in with the game.
 */

#ifndef SHARED_SYMBOLS_H
#define SHARED_SYMBOLS_H

#define NS_sys_tilesize 16
#define NS_sys_mapw 20
#define NS_sys_maph 16

#define CMD_map_image     0x20 /* u16:imageid */
#define CMD_map_hero      0x21 /* u16:position */
#define CMD_map_neighbors 0x60 /* u16:west, u16:east, u16:north, u16:south */
#define CMD_map_sprite    0x61 /* u16:position, u16:spriteid, u32:arg */
#define CMD_map_door      0x62 /* u16:position, u16:mapid, u16:dstposition, u16:arg */

#define CMD_sprite_image 0x20 /* u16:imageid */
#define CMD_sprite_tile  0x21 /* u8:tileid, u8:xform */

#define NS_tilesheet_physics 1
#define NS_tilesheet_family 0
#define NS_tilesheet_neighbors 0
#define NS_tilesheet_weight 0

#define NS_physics_vacant 0
#define NS_physics_solid 1
#define NS_physics_water 2

#endif
