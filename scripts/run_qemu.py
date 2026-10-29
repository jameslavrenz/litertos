#!/usr/bin/env python3
"""Phase 1, Step 2. Run the two-task demo on QEMU mps2-an386.

Steps 3–5 extend the expected transcript. Phase 2 does not use this script;
the Nucleo board is loaded with a programmer instead.
"""

import subprocess
import sys

EXPECTED = "A0\nB0\nA1\nB1\nA2\nB2\nA3\nB3\ndone\n"


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: run_qemu.py build/litert.elf", file=sys.stderr)
        return 2
    cmd = [
        "qemu-system-arm",
        "-machine",
        "mps2-an386",
        "-kernel",
        sys.argv[1],
        "-display",
        "none",
        "-monitor",
        "none",
        "-serial",
        "stdio",
        "-semihosting-config",
        "enable=on,target=native",
    ]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=10, check=False)
    except subprocess.TimeoutExpired as exc:
        sys.stdout.write(exc.stdout or "")
        sys.stderr.write(exc.stderr or "")
        print("qemu timed out", file=sys.stderr)
        return 1
    sys.stdout.write(proc.stdout)
    sys.stderr.write(proc.stderr)
    if proc.stdout != EXPECTED:
        print("unexpected transcript:", repr(proc.stdout), file=sys.stderr)
        return 1
    if proc.returncode != 0:
        print("qemu exit status", proc.returncode, file=sys.stderr)
        return proc.returncode
    print("mps2-an386 transcript ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
