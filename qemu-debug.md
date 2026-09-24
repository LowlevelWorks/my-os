# Debugging a QEMU Crash: Finding the Cause

QEMU has several tools for this. Use them in this order.

## 1. Log every exception and interrupt

```
qemu-system-x86_64 -cdrom os.iso -d int,cpu_reset,guest_errors -D qemu.log -no-reboot -no-shutdown
```

- `-no-reboot` stops QEMU from restarting after a triple fault, so you keep the final state.
- `-no-shutdown` keeps the window and monitor alive after the crash.

Then read `qemu.log`. The **last exception before `CPU Reset`** is the cause:

| Line | Meaning |
|---|---|
| `v=0e` | Page fault. `CR2=` is the address you touched. |
| `v=0d` | General protection fault. Usually a bad segment or a malformed IDT entry. |
| `v=08` | Double fault, so a fault happened while handling a fault. |
| `v=06` | Invalid opcode. |
| `v=21` | IRQ1 (keyboard) after a PIC remap to 0x20. |

Each entry shows `RIP=`. Turn it into a source line:

```
addr2line -e kernel.elf -f -i 0xFFFFFFFF...
# or
objdump -d -M intel kernel.elf | less    # then search for the address
```

## 2. Inspect a frozen machine with the QEMU monitor

Add `-monitor stdio`, or press Ctrl+Alt+2 in the QEMU window. Useful commands:

```
info registers      # RIP, RSP, CR2, CR3, RFLAGS
x/10i $rip          # disassemble at the current instruction
x/16gx $rsp         # dump the stack
info pic            # PIC mask, so you can check IRQ1 is unmasked
info irq            # per-IRQ counters, so you can see keys arriving
info mem            # active page mappings
sendkey a           # inject a keypress
```

For your shell, `info registers` is very informative. If RIP is on the `hlt` in your main loop and `RFLAGS` has no `IF` bit (0x200), then `sti` never ran or something cleared it. If `IF` is set, interrupts are enabled, so check `info pic` and `info irq` next.

## 3. GDB

```
qemu-system-x86_64 -cdrom os.iso -s -S -no-reboot -no-shutdown
```

`-s` opens a GDB server on port 1234 and `-S` pauses at the first instruction. In another terminal:

```
gdb kernel.elf
(gdb) target remote :1234
(gdb) set architecture i386:x86-64
(gdb) hbreak kernel_main
(gdb) continue
(gdb) layout src
(gdb) info registers
(gdb) x/8i $rip
(gdb) stepi
```

Notes:
- Use `hbreak` (hardware breakpoints), not `break`. They survive the CPU mode switches.
- Compile with `-g` and don't strip `kernel.elf`, or you'll get no symbols or source lines.
- If GDB gets confused in the 32-bit boot code, put `hbreak long_mode` and continue from there.
- Useful for you: `hbreak keyboard_handler` (or whatever your ISR is called) and press a key. If it never hits, the interrupt isn't arriving. If it hits, step through it.

## 4. Print output that doesn't depend on your terminal driver

If the VGA terminal is what's broken, log through the debug ports:

```c
static inline void outb(uint16_t port, uint8_t v) {
  __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}
static void debug_write(const char *s) {
  while (*s) outb(0xE9, *s++);
}
```

Run with `-debugcon stdio`. Every `outb(0xE9, c)` shows up in your host terminal, with no setup needed. For a proper serial port, use `-serial stdio` and write to `0x3F8`.

## 5. Apply it to your shell problem

1. Run with `-d int -D qemu.log -no-reboot`, boot, click the QEMU window, and press a key.
2. Search the log:
   - **`v=21` appears, followed by `v=0d` or `v=0e`.** The interrupt arrives but the handler crashes. Look at the ISR stub (`iretq`, register saving) and `RIP`.
   - **`v=21` appears and nothing bad follows.** The handler runs, so the bug is in the keyboard or shell code. Check that the handler reads port `0x60`, sends EOI, and pushes into the buffer that `keyboard_has_input()` reads.
   - **No `v=21` at all.** Check `info pic` for a mask on IRQ1, `info registers` for `IF`, and whether the IDT entry for `0x21` is set.
   - **Any `v=08` or a `CPU Reset` earlier.** Fix that first.

Also make sure the QEMU window has keyboard focus. With `-nographic`, keys go through the serial console instead, which is a common false alarm.

If you paste the last 20 lines of `qemu.log` (from the last exception through the reset), I can tell you exactly what failed.
