"""Execute the linked Z80 BIOS adapters and ESP driver with simulated ports.

Requires zx==0.13.15 and a completed make -f Makefile.zx-mbel build.
"""
from collections import deque
from pathlib import Path
import re
from zx._spectrum import Spectrum, RunEvents

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-MBEL"
SYMS = dict((name, int(value, 16)) for name, value in re.findall(
    r"^(\w+)\s*= \$([0-9A-Fa-f]+)",
    (BUILD / "platoterm-mbel.map").read_text(), re.M))


class UART:
    def __init__(self, mb):
        self.mb = mb
        self.rx = deque()
        self.sent = bytearray()
        self.command = bytearray()
        self.payload = bytes(range(256)) * 2
        self.count = 0
        self.status = 0
        self.protocol = False
        self.new_header = False

    def read(self, port):
        if port == 0x773b:
            return 0x30 if self.mb else 0
        if port == 0x713b:
            return 1
        if port == 0x133b:
            return self.status | bool(self.rx)
        if port == 0x143b:
            return self.rx.popleft()
        return 0xff

    def write(self, port, value):
        if port != 0x133b:
            return
        self.sent.append(value)
        if not self.protocol:
            return
        if self.count:
            self.count -= 1
            if not self.count:
                self.rx.extend(b"\r\nSEND OK\r\n")
            return
        self.command.append(value)
        if value != 10:
            return
        command = bytes(self.command)
        self.command.clear()
        if command == b"AT+CIPRECVLEN?\r\n":
            reply = b"\r\n+CIPRECVLEN:%d\r\nOK\r\n" % len(self.payload)
        elif command.startswith(b"AT+CIPRECVDATA="):
            n = int(command.split(b"=")[1])
            data, self.payload = self.payload[:n], self.payload[n:]
            header = (b"+CIPRECVDATA:%d," if self.new_header else
                      b"+CIPRECVDATA,%d:") % len(data)
            reply = b"\r\n" + header + data + b"\r\nOK\r\n"
        elif command.startswith(b"AT+CIPSEND="):
            self.count = int(command.split(b"=")[1])
            reply = b"\r\nOK\r\n>"
        elif command.startswith(b"AT+CIPCLOSE"):
            reply = b"\r\nERROR\r\n"
        else:
            reply = b"\r\nOK\r\n"
        self.rx.extend(reply)


class Machine(Spectrum):
    def __new__(cls, uart):
        return super().__new__(cls)

    def __init__(self, uart):
        super().__init__(headless=True)
        self.suppress_interrupts = True
        self.set_on_input_callback(uart.read)
        self.set_on_output_callback(uart.write)
        self.write(0x8000, (BUILD / "platoterm-mbel").read_bytes(), ram_page=0)
        self.mark_addrs(0x7000, 1, 1)

    def call(self, name, *args, hl=0):
        self.ix, self.iy = 0x1234, 0x5c3a
        self.hl = hl
        self.sp = 0x7ff0 - 2 * len(args)
        self.write(self.sp, (0x7000).to_bytes(2, "little") + b"".join(
            arg.to_bytes(2, "little") for arg in reversed(args)))
        self.pc = SYMS[name]
        for frame in range(2000):
            self.fetches_limit = 1000000
            events = self._run()
            if events & RunEvents.BREAKPOINT_HIT:
                assert self.pc == 0x7000, hex(self.pc)
                assert (self.ix, self.iy) == (0x1234, 0x5c3a), name
                return self.hl
            if events & RunEvents.END_OF_FRAME:
                self.write(23672, ((self.read16(23672) + 1) & 65535).to_bytes(2, "little"))
        raise AssertionError("Z80 call did not return: " + name)


for mb in (False, True):
    uart = UART(mb)
    z = Machine(uart)
    z.call("_wifi_uart_init")
    relocated = z.read(SYMS["_wifi_bios"], 544)
    z.call("_wifi_uart_init")
    assert z.read(SYMS["_wifi_bios"], 544) == relocated, "BIOS relocated twice"
    assert z.call("_wifi_uart_read") == 65535
    for value in range(256):
        uart.rx.append(value)
        assert z.call("_wifi_uart_read") == value
    uart.status = 4
    assert z.call("_wifi_uart_read") == 65534
    uart.status = 2
    assert z.call("_wifi_uart_write", hl=65) == 0 and not uart.sent
    uart.status = 0
    for value in range(256):
        assert z.call("_wifi_uart_write", hl=value) == 1
    assert uart.sent == bytes(range(256))

    uart.protocol = True
    z.write(0x6800, b"IRATA.ONLINE\0")
    assert z.call("_esp_open", 0x6800) == 1
    assert z.call("_esp_recv", 0x6800) == 512
    assert z.read(0x6800, 512) == bytes(range(256)) * 2
    uart.new_header = True
    uart.payload = bytes(range(88))
    assert z.call("_esp_recv", 0x6800) == 88
    assert z.read(0x6800, 88) == bytes(range(88))
    assert z.call("_esp_recv", 0x6800) == 0
    uart.sent.clear()
    assert z.call("_esp_send", 0x6800, 88) == 1
    assert uart.sent.endswith(bytes(range(88)))
    assert not uart.rx and not z.read8(SYMS["_esp_error"])
    print("PASS: linked Z80 BIOS relocation, IX/IY, all 256 bytes, busy/overflow, TCP open/send/read on", "MB03+" if mb else "eLeMeNt")
