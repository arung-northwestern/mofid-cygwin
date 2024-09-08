import os
import sys

mofid_path = os.path.split(os.path.abspath(__file__))[0]
mofidpy_path = os.path.join(mofid_path, 'Python')
bin_path = os.path.join(mofid_path, 'bin')

# Allow for custom openbabel path
if len(sys.argv) > 1:
    openbabel_path = sys.argv[1]
else:
    openbabel_path = os.path.join(mofid_path, 'openbabel')

resources_path = os.path.join(mofid_path, 'Resources')

for p in [mofid_path, mofidpy_path, bin_path, openbabel_path, resources_path]:
    if not os.path.exists(p):
        raise ValueError('No directory ' + p)

# Set OpenBabel environment variables
os.environ['BABEL_DATADIR'] = os.path.join(openbabel_path, 'data')
os.environ['BABEL_LIBDIR'] = os.path.join(openbabel_path, 'build', 'lib')

# Add OpenBabel lib directory to LD_LIBRARY_PATH
if 'LD_LIBRARY_PATH' in os.environ:
    os.environ['LD_LIBRARY_PATH'] = f"{os.path.join(openbabel_path, 'build', 'lib')}:{os.environ['LD_LIBRARY_PATH']}"
else:
    os.environ['LD_LIBRARY_PATH'] = os.path.join(openbabel_path, 'build', 'lib')

# Add OpenBabel bin directory to PATH
os.environ['PATH'] = f"{os.path.join(openbabel_path, 'build', 'bin')}:{os.environ['PATH']}"

with open(os.path.join(mofidpy_path, 'paths.py'), 'w') as f:
    f.write(f"mofid_path = r'{mofid_path}'\n"
            f"bin_path = r'{bin_path}'\n"
            f"openbabel_path = r'{openbabel_path}'\n"
            f"resources_path = r'{resources_path}'\n"
            f"BABEL_DATADIR = r'{os.environ['BABEL_DATADIR']}'\n"
            f"BABEL_LIBDIR = r'{os.environ['BABEL_LIBDIR']}'\n"
            f"LD_LIBRARY_PATH = r'{os.environ['LD_LIBRARY_PATH']}'\n")

print(f"Paths set. OpenBabel path: {openbabel_path}")
print(f"BABEL_DATADIR: {os.environ['BABEL_DATADIR']}")
print(f"BABEL_LIBDIR: {os.environ['BABEL_LIBDIR']}")
print(f"LD_LIBRARY_PATH: {os.environ['LD_LIBRARY_PATH']}")
print(f"PATH: {os.environ['PATH']}")
