"""Restore the pinned CPU-only Bullet source checkout for the Waf build."""
from pathlib import Path
import subprocess

COMMIT = '2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5'
REPOSITORY = 'https://github.com/bulletphysics/bullet3.git'
destination = Path(__file__).resolve().parents[2] / '3rdparty' / 'bullet'
if not destination.exists():
    subprocess.run(['git', 'clone', '--depth', '1', '--branch', '3.25',
                    '--filter=blob:none', '--sparse', REPOSITORY, str(destination)], check=True)
actual = subprocess.check_output(['git', '-C', str(destination), 'rev-parse', 'HEAD'], text=True).strip()
if actual != COMMIT:
    raise SystemExit(f'Bullet revision mismatch: {actual}; expected {COMMIT}')
subprocess.run(['git', '-C', str(destination), 'sparse-checkout', 'set',
                'src/BulletDynamics', 'src/BulletCollision', 'src/LinearMath'], check=True)
print(f'Bullet 3.25 ready: {COMMIT}; license: {destination / "LICENSE.txt"}')
