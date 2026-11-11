# Login arrow investigation

The missing arrow after NEXT is requested by IRATA, not caused by the client
erasing text locally. On 2026-11-11 a TCP connection to irata.online:8005 using
only the public guest/guest entry reproduced these decoded operations:

1. Prompt in M0 at (24,224).
2. Arrow (M1 character 0x3f) at (24,208), ModeWrite.
3. Typed guest text in M0 at (40,208).
4. After CR/NEXT: **ModeInverse block (24,223) to (39,208)**.
5. Group prompt at (24,176), new arrow at (24,160).

The block removes the completed field's arrow. It does not cover the entered
name or the prompt above. On the ZX screen the rectangle is x=12..19,
y=108..113 inclusive. A pixel regression in tests/screen-render.c checks that
exact operation, including preservation of all pixels outside the rectangle.

Submitting guest as the group moves directly to the guest menu; this does not
reproduce a private account's password screen, so no claim is made that the
second transition was captured with a private account. No private credentials
were used or captured. No change to production rendering is needed to keep an
arrow that the server deliberately removes. Alpha17 remains the current TAP.

The earlier alpha17 rectangle fixes are valid but were not the cause of the
reported disappearing arrow. Do not introduce a special case that ignores
server erase commands just to retain inactive-field arrows.

Optional capture: tests/capture-guest.ps1 connects to the service, answers its
terminal echo requests and sends public guest fields. It is not run by the
normal test suite and none of its tracing is linked into the TAP. Local capture
and decoder trace files are in ignored build-STD/guest-*.bin and guest-replay.log.
