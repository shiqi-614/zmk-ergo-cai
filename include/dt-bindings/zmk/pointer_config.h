/*
 * The commands of the pointer settings behavior (ergocai,behavior-pointer-config): what a key bound to
 * &tb_cfg or &js_cfg does to the trackball's or the joystick's pointer settings (src/pointer_config.c).
 */

#pragma once

#define PTR_INVERT_X 0 /* flip x, as the pointer moves after the other settings */
#define PTR_INVERT_Y 1 /* flip y */
#define PTR_SWAP_XY 2  /* swap x and y */
#define PTR_FASTER 3   /* one step faster (up to 4x) */
#define PTR_SLOWER 4   /* one step slower (down to 1/4) */
#define PTR_RESET 5    /* back to the defaults in the devicetree */
#define PTR_MOVE_HERE 6   /* on the highest active layer, move the pointer */
#define PTR_SCROLL_HERE 7 /* on the highest active layer, scroll */
