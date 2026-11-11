/**
 * PLATOTerm64 - A PLATO Terminal for the Commodore 64
 * Based on Steve Peltz's PAD
 * 
 * Author: Thomas Cherryhomes <thom.cherryhomes at gmail dot com>
 *
 * terminal.c - Terminal state functions
 */

#include <stdbool.h>
#include <string.h>
#include "terminal.h"
#include "screen.h"

#define true 1
#define false 0

/**
 * ASCII Features to return in Features
 */
#define ASC_ZFGT        0x01
#define ASC_ZPCKEYS     0x02
#define ASC_ZKERMIT     0x04
#define ASC_ZWINDOW     0x08

/**
 * protocol.c externals
 */
extern CharMem CurMem;
extern padBool TTY;
extern padBool ModeBold;
extern padBool Rotate;
extern padBool Reverse;
extern DispMode CurMode;
extern padBool FlowControl;

/**
 * screen.c externals
 */
extern unsigned char CharWide;
extern unsigned char CharHigh;
extern padPt TTYLoc;

extern unsigned char already_started;
static unsigned char startup_row_pending;
extern void screen_clear_status_row(void);

/* The server's initial probe draws an A before asking for terminal features.
 * Clear the status row once at that handshake, never on ordinary later echoes. */
void terminal_handshake_complete(void)
{
  if (!startup_row_pending) return;
  startup_row_pending=0;
  screen_clear_status_row();
}

/**
 * terminal_init()
 * Initialize terminal state
 */
void terminal_init(void)
{
  terminal_set_tty();
}

/**
 * terminal_initial_position()
 * Set terminal initial position after splash screen.
 */
void terminal_initial_position(void)
{
  startup_row_pending=1;
  TTYLoc.x=0;
  TTYLoc.y=256; // Right under splashscreen.
}

/**
 * terminal_set_tty(void) - Switch to TTY mode
 */
void terminal_set_tty(void)
{
  startup_row_pending=0;
  if (already_started)
    screen_clear();
  TTY=true;
  ModeBold=padF;
  Rotate=padF;
  Reverse=padF;
  CurMem=M0;
  /* CurMode=ModeRewrite; */
  CurMode=ModeWrite;    /* For speed reasons. */
  CharWide=8;
  CharHigh=16;
  TTYLoc.x = 0;        // leftmost coordinate on screen
  TTYLoc.y = 495;      // Top of screen - one character height
}

/**
 * terminal_set_plato(void) - Switch to PLATO mode
 */
void terminal_set_plato(void)
{
  TTY=false;
  screen_clear();
}

/**
 * terminal_get_features(void) - Inquire about terminal ASCII features
 */
unsigned char terminal_get_features(void)
{
  return ASC_ZFGT; /* This terminal can do Fine Grained Touch (FGT) */
}

/**
 * terminal_get_type(void) - Return the appropriate terminal type
 */
unsigned char terminal_get_type(void)
{
  return 12; /* ASCII terminal type */
}

/**
 * terminal_get_subtype(void) - Return the appropriate terminal subtype
 */
unsigned char terminal_get_subtype(void)
{
  return 1; /* ASCII terminal subtype IST-III */
}

/**
 * terminal_get_load_file(void) - Return the appropriate terminal loadfile (should just be 0)
 */
unsigned char terminal_get_load_file(void)
{
  return 0; /* This terminal does not load its resident from the PLATO system. */
}

/**
 * terminal_get_configuration(void) - Return the terminal configuration
 */
unsigned char terminal_get_configuration(void)
{
  return 0x40; /* Touch panel is present. */
}

/**
 * terminal_get_char_address(void) - Return the base address of the character set.
 */
unsigned short terminal_get_char_address(void)
{
  return 0x3000; /* What the? Shouldn't this be 0x3800? */
}

/**
 * terminal_mem_read - Read a byte of program memory.
 * not needed for our terminal, but must
 * be decoded.
 */
padByte terminal_mem_read(padWord addr)
{
  return (0xFF);
}

/**
 * terminal_mem_load - Write a byte to non-character memory.
 * not needed for our terminal, but must be decoded.
 */
void terminal_mem_load(padWord addr, padWord value)
{
  /* Not Implemented */
}

/**
 * Mode5, 6, and 7 are basically stubbed.
 */
void terminal_mode_5(padWord value)
{ 
}

void terminal_mode_6(padWord value)
{ 
}

void terminal_mode_7(padWord value)
{ 
}

/**
 * terminal_ext_allow - External Input allowed. Not implemented.
 */
void terminal_ext_allow(padBool allow)
{
  /* Not Implemented */
}

/**
 * terminal_set_ext_in - Set which device to get input from.
 * Not implemented
 */
void terminal_set_ext_in(padWord device)
{
}

/**
 * terminal_set_ext_out - Set which device to send external data to.
 * Not implemented
 */
void terminal_set_ext_out(padWord device)
{
}

/**
 * terminal_ext_in - get an external input from selected device.
 * Not implemented.
 */
padByte terminal_ext_in(void)
{
  return 0;
}

/**
 * terminal_ext_out - Send an external output to selected device
 * Not implemented.
 */
void terminal_ext_out(padByte value)
{
}

/* Convert the 8x16 PLATO glyph to the original ZX 5x6 raster.
 * Keep the original vertical mapping for density and sparse projection.
 * A fixed shift replaces 128 variable shifts; sparse projection is accumulated
 * in the same pass instead of scanning a transposed bitmap a second time. */
static unsigned char dense_row[16]={0,0,0,5,5,5,10,10,15,15,20,20,20,25,25,25};
static unsigned char sparse_row[16]={0,0,0,1,1,1,2,2,3,3,4,4,4,5,5,5};
static unsigned char dense_column[8]={0,0,1,2,2,3,3,4};
static unsigned char sparse_column[8]={128,128,64,32,32,16,16,8};
static unsigned char thresholds[30]={3,2,3,3,2,3,2,3,3,2,2,1,2,2,1,2,1,2,2,1,3,2,3,3,2,3,2,3,3,2};
extern unsigned char fontm23[768];
extern unsigned short fontptr[160];
void terminal_char_load(padWord charnum, charData theChar)
{
  /* Decoder is sequential; fixed scratch avoids costly Z80 stack indexing. */
  static unsigned char weights[30], sparse[6];
  static unsigned char column,row,pixels,index,mask,bits;
  static unsigned int word;
  static unsigned char *dest;
  if ((unsigned int)charnum>=128) return;
  pixels=0;
  dest=fontm23+fontptr[charnum];
  memset(weights,0,sizeof(weights));
  memset(sparse,0,sizeof(sparse));
  for(column=0;column<8;++column) {
    word=theChar[column];
    for(row=0;row<16;++row) {
      if(word & 0x8000) {
        ++pixels;
        ++weights[dense_row[row]+dense_column[column]];
        sparse[sparse_row[row]] |= sparse_column[column];
      }
      word <<= 1;
    }
  }
  if(pixels<54 || pixels>=85) {
    memcpy(dest,sparse,6);
    return;
  }
  index=0;
  for(row=0;row<6;++row) {
    bits=0;mask=128;
    for(column=0;column<5;++column) {
      if(weights[index]>=thresholds[index]) bits|=mask;
      ++index;mask>>=1;
    }
    dest[row]=bits;
  }
}

/**
 * terminal_ready() - Show ready
 */
void terminal_ready(void)
{
  TTYLoc.x=TTYLoc.y=0;
  ShowPLATO("PLATOTerm 1.2 Ready. EXTEND 9 - Print. EXTEND 0 - Help",54);
}
