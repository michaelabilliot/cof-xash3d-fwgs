"""Name the unnamed retail functions of units from our compiled objects.

  python tools/name_unit.py [--dll client] <unit> [<unit> ...]

Runs, for the given units, under one lock (build/jobs/names.lock) so that
parallel agents do not overwrite each other's results:
1. tools/build.py base (our objects must be current);
2. tools/bytematch.py --write (identical bytes, relocations masked);
3. tools/align_names.py --write (position inside the object);
4. tools/build.py target (merges the name maps and delinks the units);
5. tools/sync_names.py (progress.csv and the unit lists);
6. tools/build.py target again, so the new names are in the targets.
Then `tools/build.py check <unit>` shows the new functions. A position name
is a proposal until the function reaches 100%.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOCK = os.path.join(ROOT, "build", "jobs", "names.lock")


def run(args, quiet=False):
    res = subprocess.run([sys.executable] + args, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = res.stdout.decode("latin-1")
    if not quiet:
        for line in out.splitlines():
            # only the results, not the per-unit "ok" lines
            if not line.rstrip().endswith(" ok"):
                print(line)
    return res.returncode


def lock():
    """Take the naming lock (also used by tools/add_name.py)."""
    os.makedirs(os.path.dirname(LOCK), exist_ok=True)
    while True:
        try:
            fd = os.open(LOCK, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            break
        except FileExistsError:
            # a lock older than 30 minutes is left over from a killed run
            try:
                if time.time() - os.path.getmtime(LOCK) > 1800:
                    os.remove(LOCK)
                    continue
            except OSError:
                continue
            time.sleep(5)
    os.write(fd, str(os.getpid()).encode())
    os.close(fd)


def unlock():
    os.remove(LOCK)


def main():
    argv = sys.argv[1:]
    dll = ["--dll", argv[argv.index("--dll") + 1]] if "--dll" in argv else []
    units = [a for i, a in enumerate(argv) if not a.startswith("--") and (i == 0 or argv[i - 1] != "--dll")]
    if not units:
        sys.exit(__doc__)
    lock()
    try:
        tools = os.path.join(ROOT, "tools")
        if run([os.path.join(tools, "build.py"), "base"] + units):
            sys.exit("a unit does not compile: fix it first")
        run([os.path.join(tools, "bytematch.py")] + dll + ["--write"] + units)
        run([os.path.join(tools, "align_names.py")] + dll + ["--write"] + units)
        run([os.path.join(tools, "build.py"), "target"] + units, quiet=True)
        run([os.path.join(tools, "sync_names.py")] + dll)
        run([os.path.join(tools, "build.py"), "target"] + units)
    finally:
        unlock()


if __name__ == "__main__":
    main()
