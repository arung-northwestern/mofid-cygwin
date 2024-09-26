# MOFid-Cygwin 🏗️🖥️

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Windows](https://img.shields.io/badge/Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![made-with-python](https://img.shields.io/badge/Made%20with-Python-1f425f.svg)](https://www.python.org/)

A streamlined version of MOFid for easy compilation and use on Windows through Cygwin, featuring the latest OpenBabel version. The Python wrapper has been modified to work from Jupyter notebboks (Ipython) with commands executed in Cygwin.

## 🎯 Purpose

1. Provide a user-friendly version of the original MOFid repository that's easy to compile and use on Windows through Cygwin.
2. Utilize the latest version of OpenBabel, ensuring compatibility with modern C++ standards and facilitating compilation with the latest gcc versions (13.2, 14.2, etc.).
3. Add wrapper functions to run MOFid commands in Cygwin from Jupyter notebooks.

## 🚀 Getting Started

### 📋 Prerequisites

- [Cygwin](https://www.cygwin.com/) installed with the following packages:
  - gcc-core
  - gcc-g++
  - libstdc++
  - boost-build
  - cmake
  - binutils (provides the `ar` command)
- **Java Runtime Environment**: Install using conda:
  ```
  conda install -c conda-forge openjdk
  ```

### 🛠️ Installation

1. Open a new Cygwin terminal as administrator.

2. Clone this repository:
   ```
   git clone https://github.com/yourusername/mofid-cygwin.git
   cd mofid-cygwin
   ```

3. Clone the OpenBabel repository inside the parent directory and remove its .git folder:
   ```
   git clone https://github.com/openbabel/openbabel.git
   cd openbabel
   rm -rf .git
   cd ..
   ```

4. Remove existing build folders (if present):
   ```
   rm -rf openbabel/build openbabel/installed ./bin
   ```

5. Initialize the build:
   ```
   make init
   ```

6. Verify the installation:

Run this in the cygwin terminal in the `mofid-cygwin` folder after compilation.

   ```
   bin/sbu irmof_test.cif
   ```
   You should see output showing the SMILES of the nodes and linkers.

7. Set up the Python environment:
   ```
   which python  # Ensure you are in the correct environment
   python set_paths.py
   pip install .
   ```

8. Verify the Python package installation:

   ```
   conda list mofid
   ```

## 📚 Usage (Detailed)

For a detailed example of how to use MOFid from a Jupyter notebook, check out the [**run_from_jupyter.ipynb**](tests/run_from_jupyter.ipynb) file located in the `tests` folder. This notebook demonstrates how to run MOFid functions directly from Jupyter, including extracting fragments and topologies, as well as using the `cif2mofid` wrapper for quick access to compiling MOFid for a CIF file directly. 🎉

## 🧪 Testing

Open iPython and run the following code:

```
from mofid.run_mofid import cif2mofid
from pathlib import Path
path_to_mofid = Path('your path to mofid-cygwin compiled folder')
path_to_cif = Path('./irmof_test.cif').resolve()
output_path = Path('.').resolve()
result = cif2mofid(cif_path=path_to_cif, output_path=output_path, path_to_mofid=path_to_mofid)
print(result)
```
You should see something like this in the cell output:

```
{'mofid': '[O-]C(=O)c1ccc(cc1)C(=O)[O-].[Zn]O([Zn])[Zn] MOFid-v1.pcu.cat0.NO_REF;P1-IRMOF-1',
'mofkey': 'Zn.KKEYFWRCBNTPAC.MOFkey-v1.pcu.NO_REF',
'smiles_nodes': ['[Zn]O([Zn])[Zn]'],
'smiles_linkers': ['[O-]C(=O)c1ccc(cc1)C(=O)[O-]'],
'smiles': '[O-]C(=O)c1ccc(cc1)C(=O)[O-].[Zn]O([Zn])[Zn]',
'topology': 'pcu',
'cat': '0',
'cifname': 'P1-IRMOF-1'}
```
This confirms that the MOFid package is correctly installed and working.

## 📚 Original README

For reference, here's a summary of the original MOFid README:

> **MOFid** 🔬
>
> A system for rapid identification and analysis of metal-organic frameworks.
>
> Please cite [DOI: 10.1021/acs.cgd.9b01050](https://pubs.acs.org/doi/abs/10.1021/acs.cgd.9b01050) if you use MOFid in your work.
>
> **2024 update** 🆕
>
> This is the main repository for MOFid code, with some notable updates in 2024:
> - Updated dependencies
> - Support of gcc 11.x
> - Continuous integration
>
> The original MOFid code released in 2019 is available in the [mofid1.0_archive](https://github.com/snurr-group/mofid/tree/mofid1.0_archive) branch.
>
> **Usage** 📖
>
> [View the documentation](https://snurr-group.github.io/mofid/) for usage information.

