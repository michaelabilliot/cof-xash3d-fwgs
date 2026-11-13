"""Write docs/progress.csv safely.

On Windows, opening a file for writing fails with EINVAL or EACCES while
another process (an agent running disasm.py, objdiff, a virus scan) has
it open. Write to a temporary file next to it and replace the original,
retrying for a few seconds; the original is never left truncated.
"""
import csv
import os
import time


def write_rows(path, fields, rows, attempts=40):
    tmp = path + ".tmp"
    with open(tmp, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    for i in range(attempts):
        try:
            os.replace(tmp, path)
            return
        except OSError:
            if i == attempts - 1:
                raise
            time.sleep(0.25)
