"""
Calculate a MOFid string

Extract the node and linker identities from bin/sbu (my Open Babel code) as
well as topological classification from Systre.  This script runs everything
to wrap it up as a formated string.

@author: Ben Bucior
"""
from pathlib import Path
def convert_to_cygwin_path(windows_path):
    """Convert a Windows path to a Cygwin-style path and escape spaces.

    Args:
        windows_path: The Windows-style path to convert.

    Returns:
        str: The converted Cygwin-style path with escaped spaces.

    Example:
        >>> convert_to_cygwin_path(Path(r"C:\Program Files\MyApp"))
        '/c/Program\\ Files/MyApp'
    """
    import os  # Importing os for path manipulation

    cygwin_path = str(windows_path).replace('\\', '/')
    if cygwin_path[1:3] == ':':
        cygwin_path = '/' + cygwin_path[0].lower() + cygwin_path[2:]

    # Escape spaces with backslashes
    cygwin_path = cygwin_path.replace(' ', '\\ ')
    return cygwin_path


def make_command(path_to_cif, executable_path, flags=""):
    """Create a command string for running a Cygwin executable with a CIF file.

    Args:
        path_to_cif: The path to the CIF file.

        executable_path: The path to the executable.
        flags: Additional flags to include in the command.

    Returns:
        str: The command string to be executed in Cygwin.

    Example:
        >>> command = make_command(Path(r"C:\path\to\file.cif"), Path(r"C:\path\to\executable"), "-ha -res")

        >>> print(command)

        'C:/path/to/executable -ha -res C:/path/to/file.cif'
    """
    import os  # Importing os for path manipulation


    # Convert paths to Cygwin format
    cygwin_executable_path = convert_to_cygwin_path(executable_path)
    cygwin_cif_path = convert_to_cygwin_path(path_to_cif)

    # Print the resolved paths for debugging
    print("Cygwin Executable Path:", cygwin_executable_path)
    print("Cygwin CIF Path:", cygwin_cif_path)

    # Prepare the command for Cygwin terminal without quotes around paths
    command = f"{cygwin_executable_path} {flags} {cygwin_cif_path}"
    print('Command is', command)
    return command
def run_cygwin_from_jupyter(cygwin_command, cygwin_path=r"C:\cygwin64\bin"):
    """Run a Cygwin command from a Jupyter notebook.

    Args:
        cygwin_command: The command to run in Cygwin.
        cygwin_path: The path to the Cygwin installation (default is r"C:\cygwin64\bin").

    Returns:
        None: This function prints the output and errors directly.
    """
    import os  # Importing os for environment manipulation
    import subprocess  # Importing subprocess to run commands

    # Add Cygwin bin to the PATH
    os.environ["PATH"] = f"{cygwin_path};{os.environ['PATH']}"

    # Run the command
    result = subprocess.run(
        [f"{cygwin_path}\\bash.exe", "-c", cygwin_command],
        capture_output=True,
        text=True,
        shell=True
    )

    # Print the output
    print(result.stdout)

    # Print any errors
    if result.stderr:
        print("Errors:", result.stderr)

def run_cygwin_from_jupyter_rt(cygwin_command, cygwin_path=r"C:\cygwin64\bin"):
    """Run a Cygwin command from a Jupyter notebook in real-time.

    Args:
        cygwin_command: The command to run in Cygwin.
        cygwin_path: The path to the Cygwin installation (default is r"C:\cygwin64\bin").

    Returns:
        tuple: (return_code, output, errors)
    """
    import os
    import subprocess
    import sys

    # Add Cygwin bin to the PATH
    os.environ["PATH"] = f"{cygwin_path};{os.environ['PATH']}"

    # Run the command
    process = subprocess.Popen(
        [f"{cygwin_path}\\bash.exe", "-c", cygwin_command],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
        universal_newlines=True
    )

    output = []
    errors = []

    # Print output in real-time and capture it
    for line in process.stdout:
        print(line, end='')
        output.append(line)
        sys.stdout.flush()

    # Capture errors (if any)
    for line in process.stderr:
        print("Error:", line, end='', file=sys.stderr)
        errors.append(line)
        sys.stderr.flush()

    # Wait for the process to complete and get the return code
    return_code = process.wait()

    return return_code, ''.join(output), ''.join(errors)

import sys
import os
from mofid.paths import resources_path, bin_path

if sys.version_info[0] < 3:
    try:
        import subprocess32 as subprocess
    except:
        raise AssertionError('You must install subprocess32 if running Python2')
else:
    import subprocess

# Make sure Java is in user's path
try:
    subprocess.call('java',stderr=subprocess.PIPE)
except EnvironmentError:
    raise AssertionError('You must have Java in your path!')

# Make sure Java is in user's path
try:
    subprocess.call('java',stderr=subprocess.PIPE)
except EnvironmentError:
    raise AssertionError('You must have Java in your path!')

# Some default paths
GAVROG_LOC = os.path.join(resources_path,'Systre-experimental-20.8.0.jar')
JAVA_LOC = 'java'
RCSR_PATH = os.path.join(resources_path,'RCSRnets.arc')
DEFAULT_SYSTRE_CGD = os.path.join('Output','SingleNode','topology.cgd')
SYSTRE_TIMEOUT = 30  # max time to allow Systre to run (seconds), since it hangs on certain CGD files
SBU_BIN = os.path.join(bin_path,'sbu')

# Can update the RCSR version at http://rcsr.anu.edu.au/systre
SYSTRE_CMD_LIST = [
    JAVA_LOC,
    '-Xmx1024m',  # allocate up to 1GB of memory
    '-cp', GAVROG_LOC,  # call a specific classpath in the .jar file
    'org.gavrog.apps.systre.SystreCmdline',
    RCSR_PATH  # RCSR archive to supplement the old version in the .jar file
    ]

def runcmd(cmd_list, timeout=None):
    if timeout is None:
        return subprocess.run(cmd_list, universal_newlines=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    else:
        return subprocess.run(cmd_list, universal_newlines=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout)

def extract_fragments(mof_path, output_path=Path('.').resolve(), path_to_mofid=None, path_to_cygwin=r"C:\cygwin64\bin"):
    """
    Extract MOF decomposition information.

    Args:
        mof_path (str): Path to the MOF file.
        output_path (str): Path to the output directory.
        path_to_mofid (str, optional): Path to the MOFid installation. If None, uses the default.
        path_to_cygwin (str, optional): Path to the Cygwin installation. Defaults to r"C:\cygwin64\bin".

    Returns:
        tuple: (node_fragments, linker_fragments, cat, base_mofkey)
    """
    import os
    from mofid.paths import bin_path as default_bin_path

    # Use provided path_to_mofid or default
    bin_path = path_to_mofid if path_to_mofid is not None else default_bin_path
    SBU_BIN = path_to_mofid / 'bin' / 'sbu'

    # Add these debug prints
    print(f"SBU_BIN: {SBU_BIN}")
    print(f"mof_path: {mof_path}")
    print(f"output_path: {output_path}")
    print(f"Current working directory: {os.getcwd()}")
    print(f"PATH: {os.environ.get('PATH')}")
    print(f"LD_LIBRARY_PATH: {os.environ.get('LD_LIBRARY_PATH')}")
    print(f"BABEL_DATADIR: {os.environ.get('BABEL_DATADIR')}")
    print(f"BABEL_LIBDIR: {os.environ.get('BABEL_LIBDIR')}")

    # Create the command for running sbu
    sbu_command = make_command(mof_path, SBU_BIN)

    # Run the command using Cygwin
    return_code, cpp_output, errors = run_cygwin_from_jupyter_rt(sbu_command, cygwin_path=path_to_cygwin)

    if return_code != 0:
        print("Error: sbu command failed.")
        print("Errors:", errors)
        all_fragments = ['*']  # Null-behaving atom for Open Babel and rdkit, so the .smi file is still useful
    else:
        all_fragments = cpp_output.strip().split('\n')
        all_fragments = [x.strip() for x in all_fragments]  # clean up extra tabs, newlines, etc.

    cat = None
    if 'simplified net(s)' in all_fragments[-1]:
        cat = all_fragments.pop()[8]  # '# Found x simplified net(s)'
        cat = str(int(cat) - 1)
        if cat == '-1':
            cat = None

    # Parse node/linker fragment notation
    if all_fragments[0] != '# Nodes:':
        return (['*'], [], cat, '')
    all_fragments.pop(0)
    linker_flag_loc = all_fragments.index('# Linkers:')
    node_fragments = all_fragments[:linker_flag_loc]
    linker_fragments = all_fragments[linker_flag_loc+1:]  # could be the empty set

    base_mofkey = None
    if return_code == 0:  # If it's a successful run
        mofkey_loc = os.path.join(
            output_path, 'Output', 'MetalOxo', 'mofkey_no_topology.txt')
        with open(mofkey_loc) as f:
            base_mofkey = f.read().rstrip()  # ending newlines, etc.

    return (sorted(node_fragments), sorted(linker_fragments), cat, base_mofkey)

def extract_topology(topology_file, path_to_mofid=None, path_to_cygwin=r"C:\cygwin64\bin"):
    """
    Extract underlying MOF topology using Systre.

    Args:
        topology_file (str): Path to the topology.cgd file.
        path_to_mofid (str, optional): Path to the MOFid installation. If None, uses the default.
        path_to_cygwin (str, optional): Path to the Cygwin installation. Defaults to r"C:\cygwin64\bin".

    Returns:
        str: The extracted topology or an error message.
    """
    import os
    from mofid.paths import resources_path as default_resources_path

    # Use provided path_to_mofid or default for resources
    resources_path = os.path.join(path_to_mofid, 'Resources') if path_to_mofid else default_resources_path

    # Update paths based on the new resources_path and convert to Cygwin style
    GAVROG_LOC = convert_to_cygwin_path(os.path.join(resources_path, 'Systre-experimental-20.8.0.jar'))
    RCSR_PATH = convert_to_cygwin_path(os.path.join(resources_path, 'RCSRnets.arc'))

    # Convert the topology file path to Cygwin style
    # topology_file =(topology_file)

    # Create the command for running Systre
    systre_command = make_command(
        topology_file,
        'java',
        f"-Xmx1024m -cp {GAVROG_LOC} org.gavrog.apps.systre.SystreCmdline {RCSR_PATH}"
    )

    # Run the command using Cygwin and capture output
    return_code, java_output, errors = run_cygwin_from_jupyter_rt(systre_command, cygwin_path=path_to_cygwin)

    if return_code != 0:
        print("Error running Systre.")
        print("Errors:", errors)
        return 'ERROR'

    topologies = []  # What net(s) are found in the simplified framework(s)?
    current_component = 0
    topology_line = False
    repeat_line = False
    for raw_line in java_output.split('\n'):
        line = raw_line.strip()
        if topology_line:
            topology_line = False
            rcsr = line.split()
            assert rcsr[0] == 'Name:'
            topologies.append(rcsr[1])
        elif repeat_line:
            repeat_line = False
            assert line.split()[0] == 'Name:'
            components = line.split('_')  # Line takes the form 'Name:    refcode_clean_component_x'
            assert components[-2] == 'component'
            topologies.append(topologies[int(components[-1]) - 1])  # Subtract one since Systre is one-indexed
        elif 'ERROR' in line:
            return 'ERROR'
        elif 'Structure was found in archive' in line:
            topology_line = True
        elif line == 'Structure is new for this run.':
            # This line is only printed if new to both versions of the RCSR database:
            # a copy saved in the .jar file and an updated version in Resources/RCSRnets.arc
            topologies.append('UNKNOWN')
        elif line == 'Structure already seen in this run.':
            repeat_line = True
        elif 'Processing component ' in line:
            assert len(topologies) == current_component  # Should extract one topology per component
            current_component += 1
            line_num = line.split('component')[-1].split(':')[0].strip()
            assert line_num == str(current_component)

    if len(topologies) == 0:
        return 'ERROR'  # unexpected format
    first_net = topologies[0]  # Check that all present nets are consistent
    for net in topologies:
        if net != first_net:
            return 'MISMATCH'
    return first_net

def assemble_mofid(fragments, topology, cat = None, mof_name='NAME_GOES_HERE', commit_ref = "NO_REF"):
    # Assemble the MOFid string from its components
    mofid = '.'.join(fragments) + ' '
    mofid = mofid + 'MOFid-v1' + '.'
    mofid = mofid + topology + '.'
    if cat == 'no_mof':
        mofid = mofid + cat
    elif cat is not None:
        mofid = mofid + 'cat' + cat
    else:
        mofid = mofid + 'NA'
    if mofid.startswith(' '):  # Null linkers.  Make .smi compatible
        mofid = '*' + mofid + 'no_mof'
    mofid = mofid + '.' + commit_ref
    mofid = mofid + ';' + mof_name
    return mofid

def assemble_mofkey(base_mofkey, base_topology, commit_ref="NO_REF"):
    # Add a topology to an existing MOFkey
    return base_mofkey.replace('MOFkey-v1', 'MOFkey-v1.' + base_topology + '.' + commit_ref)

def parse_mofid(mofid):
    # Deconstruct a MOFid string into its pieces
    #[mofid_data, mofid_name]
    # Normalize the string by removing trailing spaces, e.g. newlines
    mofid_parts = mofid.rstrip().split(';')
    mofid_data = mofid_parts[0]
    if len(mofid_parts) > 1:
        mofid_name = ';'.join(mofid_parts[1:])
    else:
        mofid_name = None

    components = mofid_data.split()
    if len(components) == 1:
        if mofid_data.lstrip != mofid_data:  # Empty SMILES: no MOF found
            components.append(components[0])  # Move metadata to the right
            components[0] = ''
        else:
            raise ValueError('MOF metadata required')
    smiles = components[0]
    if len(components) > 2:
        raise ValueError('Bad MOFid containing extra spaces before the semicolon:' + mofid)
    metadata = components[1]
    metadata = metadata.split('.')

    cat = None
    topology = None
    for loc, tag in enumerate(metadata):
        if loc == 0 and not tag.startswith('MOFid'):
            raise ValueError('MOFid-v1 must start with the correct tag')
        if loc == 0 and tag[5:] != '-v1':
            raise ValueError('Unsupported version of MOFid')
        elif loc == 1:
            topology = tag
        elif tag.lower().startswith('cat'):
            cat = tag[3:]
        else:
            pass  # ignoring other MOFid tags, at least for now
    return dict(
        smiles = smiles,
        topology = topology,
        cat = cat,
        name = mofid_name
    )
