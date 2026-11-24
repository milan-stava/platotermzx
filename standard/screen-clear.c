#include "screen-clear.h"
/* The text console is in HGFX 2 (gfx/con.asm), not in the ULA memory: the rows are
 * cleared there. The screen argument (formerly the ULA bitmap 4000h) is not used. */
extern void con_clear_from(unsigned char row) __z88dk_fastcall;
/* Character row 23. */
void standard_clear_last_row(unsigned char *screen)
{
  (void)screen;
  con_clear_from(23);
}
/* Character rows 12..23, the upper part (title, display choice) stays. */
void standard_clear_lower_screen(unsigned char *screen)
{
  (void)screen;
  con_clear_from(12);
}
