"""
Parent module for obtaining MOFid data for a single .cif

@author: Ben Bucior
"""

import sys
import os
import json
from mofid.id_constructor import (extract_fragments, extract_topology,
    assemble_mofkey, assemble_mofid, parse_mofid)
from mofid.cpp_cheminformatics import openbabel_GetSpacedFormula
DEFAULT_OUTPUT_PATH = 'Output'

def cif2mofid(cif_path, output_path=DEFAULT_OUTPUT_PATH, path_to_mofid=None, path_to_cygwin=r"C:\cygwin64\bin"):
    # Assemble the MOFid string from all of its pieces.
    # Also export the MOFkey in an output dict for convenience.
    cif_path = os.path.abspath(cif_path)
    output_path = os.path.abspath(output_path)

    # Extract fragments from the CIF file
    node_fragments, linker_fragments, cat, base_mofkey = extract_fragments(
        mof_path=cif_path,
        output_path=output_path,
        path_to_mofid=path_to_mofid,
        path_to_cygwin=path_to_cygwin
    )

    # Initialize topology variable
    topology = 'NA'

    if cat is not None:
        # Construct paths to the topology files
        sn_topology_file = os.path.join(output_path, 'Output', 'SingleNode', 'topology.cgd')
        an_topology_file = os.path.join(output_path, 'Output','AllNode', 'topology.cgd')

        # Extract topologies for SingleNode and AllNode
        sn_topology = extract_topology(sn_topology_file, path_to_mofid=path_to_mofid, path_to_cygwin=path_to_cygwin)
        an_topology = extract_topology(an_topology_file, path_to_mofid=path_to_mofid, path_to_cygwin=path_to_cygwin)

        # Determine the final topology
        if sn_topology == an_topology or an_topology == 'ERROR':
            topology = sn_topology
        else:
            topology = sn_topology + ',' + an_topology

    mof_name = os.path.splitext(os.path.basename(cif_path))[0]
    mofkey = base_mofkey

    # Get the commit reference
    try:
        with open('.git/ORIG_HEAD', mode='r') as f:
            commit_ref = f.read()[:8]
    except OSError:
        commit_ref = 'NO_REF'

    # Update the mofkey if topology is available
    if topology != 'NA':
        base_topology = topology.split(',')[0]
        mofkey = assemble_mofkey(mofkey, base_topology, commit_ref=commit_ref)

    # Assemble the MOFid string
    all_fragments = []
    all_fragments.extend(node_fragments)
    all_fragments.extend(linker_fragments)
    all_fragments.sort()
    mofid = assemble_mofid(all_fragments, topology, cat=cat,
            mof_name=mof_name, commit_ref=commit_ref)
    parsed = parse_mofid(mofid)

    identifiers = {
        'mofid': mofid,
        'mofkey': mofkey,
        'smiles_nodes': node_fragments,
        'smiles_linkers': linker_fragments,
        'smiles': parsed['smiles'],
        'topology': parsed['topology'],
        'cat': parsed['cat'],
        'cifname': parsed['name']
    }

    return identifiers

if __name__ == '__main__':
    args = sys.argv[1:]
    if len(args) not in [1, 2, 3]:
        raise SyntaxError('Usage: python run_mofid.py path_to_cif_for_analysis.cif OutputPathIfNonstandard OutputMofidOrJson')
    cif_file = args[0]
    output_path = DEFAULT_OUTPUT_PATH
    output_json = False  # printing the MOFid by default, unless json is explicitly requested
    if len(args) >= 2:
        output_path = args[1]
    if len(args) == 3:
        if args[2] == "json":
            output_json = True
        elif args[2] != "mofid":
            raise SyntaxError('Third argument must be json, mofid, or not provided')

    identifiers = cif2mofid(cif_file, output_path)
    if output_json:
        print(json.dumps(identifiers))
    else:
        print(identifiers['mofid'])
        #print(identifiers['mofkey'])  # but incompatible with the use of stdout in run_folder.sh
