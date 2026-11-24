/* lat.c - keyboard latency measurement, EXTEND+L (only the LAT_TEST builds,
 * PLATOTerm12_EZXMB_LATENCY*.tap). Counting: lat.asm.
 *
 * For a key sent to the server (one at a time, at most 12) four times in 50 Hz frames:
 *   t0 key read (keyboard.c), t1 sent to the ESP (io.c io_flush: SEND OK; transparent: written),
 *   t2 first data from the server after it (io.c io_main / transparent.asm), t3 that data drawn.
 * Also every AT+CIPRECVLEN? query: its duration and how many came between t1 and t2.
 * EXTEND+L draws averages / maxima in ms on the two bottom console rows and starts again. */
#ifdef LAT_TEST
#include <stdio.h>

extern void con_char_at(unsigned char x,unsigned char y,unsigned char c,unsigned char draw);
extern unsigned char lat_n;
extern unsigned int lat_sum[5],lat_max[5],lat_pf,lat_pn;   /* send, echo, draw, total, polls */
extern void lat_reset(void);

static void row(unsigned char y,const char *s)
{
  unsigned char x;
  for(x=0;x<64;++x)con_char_at(x,y,*s?*s++:' ',1);
}

void lat_show(void)
{
  char line[72];
  unsigned int a[4],p10,q;
  unsigned char i,n=lat_n;
  for(i=0;i<4;++i)a[i]=n?(lat_sum[i]*20+n/2)/n:0;  /* <= 12 x 250 x 20 < 65536 */
  p10=n?(lat_sum[4]*10+n/2)/n:0;
  q=lat_pn?(unsigned int)(((unsigned long)lat_pf*20+lat_pn/2)/lat_pn):0;
  sprintf(line,"LAT n=%u ms avg/max: send %u/%u echo %u/%u draw %u/%u",
          n,a[0],lat_max[0]*20,a[1],lat_max[1]*20,a[2],lat_max[2]*20);
  row(22,line);
  sprintf(line,"total %u/%u  polls/key %u.%u  CIPRECVLEN? %u ms  EXTEND+L",
          a[3],lat_max[3]*20,p10/10,p10%10,q);
  row(23,line);
  lat_reset();
}
#endif
