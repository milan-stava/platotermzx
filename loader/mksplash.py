#!/usr/bin/env python3
# Generates loader/splash.bin (the PLATO splash, standard/upstream/splash_spectrum.h) once. The
# build pads it to 6144 B ('EN' at the end) and the loader stores it in URAM page 16 like a module.
import re
t=open('standard/upstream/splash_spectrum.h').read(); t=t[t.index('{')+1:t.rindex('}')]
b=bytes(int(x,0) for x in re.findall(r'0[xX][0-9a-fA-F]+|\b\d+\b',t))
assert len(b)==1282
open('loader/splash.bin','wb').write(b)
