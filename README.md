# MOFid-Cygwin 🏗️🖥️

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Windows](https://img.shields.io/badge/Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![made-with-python](https://img.shields.io/badge/Made%20with-Python-1f425f.svg)](https://www.python.org/)

A streamlined version of MOFid for easy compilation and use on Windows through Cygwin, featuring the latest OpenBabel version.

## 🎯 Purpose

1. Provide a user-friendly version of the original MOFid repository that's easy to compile and use on Windows through Cygwin.
2. Utilize the latest version of OpenBabel, ensuring compatibility with modern C++11 standards and facilitating compilation with the latest gcc versions (13.2, 14.2, etc.).

## 🚀 Getting Started

### Prerequisites

- [Cygwin](https://www.cygwin.com/) installed with the following packages:
  - gcc-core
  - gcc-g++
  - libstdc++
  - boost-build
  - cmake

### Installation

1. Open a new Cygwin terminal as administrator.

2. Clone this repository:
   ```
   git clone https://github.com/yourusername/mofid-cygwin.git
   cd mofid-cygwin
   ```

3. Remove existing build folders (if present):
   ```
   rm -rf openbabel/build openbabel/installed ./bin
   ```

4. Initialize the build:
   ```
   make init
   ```

5. Verify the installation:
   ```
   bin/sbu irmof_test.cif
   ```
   You should see output similar to the example in the original README.

6. Set up the Python environment:
   ```
   which python
   python set_paths.py
   pip install .
   ```

7. Verify the Python package installation:
   ```
   conda list mofid
   ```

## 🧪 Testing

Open iPython and run:
