"""Download the official bzip2 1.0.2 source into refs/ (git-ignored).

  python tools/fetch_bzip2.py

hw.dll links bzip2 1.0.2 ("1.0.2, 30-Dec-2001"), compiled as 7 objects with
the VC6 processor pack (docs/toolchains.md). The hw/bzip2/* units compile
the unmodified official source from refs/bzip2-1.0.2-dl/bzip2-1.0.2. This
fetches the release tarball from sourceware.org, checks its SHA-512 against
the value sourceware publishes, and unpacks it.
"""
import hashlib
import os
import tarfile
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
URL = "https://sourceware.org/pub/bzip2/bzip2-1.0.2.tar.gz"
SHA512 = ("2d857226af70621737823a69e2f1790378636d5d8cfe60d4f87bf4e0cd9c5458"
          "aa22d358fbf51cf50e187a2c673b3304f276448ca07e6ae205a2c5e64135eeff")
DEST = os.path.join(ROOT, "refs", "bzip2-1.0.2-dl")


def main():
    os.makedirs(DEST, exist_ok=True)
    tgz = os.path.join(DEST, "bzip2-1.0.2.tar.gz")
    if not os.path.exists(tgz):
        urllib.request.urlretrieve(URL, tgz)
    digest = hashlib.sha512(open(tgz, "rb").read()).hexdigest()
    if digest != SHA512:
        os.remove(tgz)
        raise SystemExit("SHA-512 mismatch, download removed")
    with tarfile.open(tgz) as t:
        t.extractall(DEST)
    print("bzip2 1.0.2 source in", os.path.join(DEST, "bzip2-1.0.2"))


if __name__ == "__main__":
    main()
