# Installation & Set-up

## Doxygen

Download and install: http://www.doxygen.nl/download.html

## Graphviz

Download and install: https://graphviz.gitlab.io/download/

### Using macports

**Install macports**

MacPorts* provides both stable and development versions of Graphviz and the Mac GUI Graphviz.app.
These can be obtained via the ports “graphviz”, “graphviz-gui”.

``sudo port install graphviz``

 python37 has the following notes:
  To make this the default Python or Python 3 (i.e., the version run by the 'python' or 'python3' commands), run one or both of:

````
      sudo port select --set python python37
      sudo port select --set python3 python37
````

## Set-up

Copy ``config.sh.include.sample`` to ``config.sh.include``.

This file is included from ``generate_doc.sh``.

Set your path variables in ``config.sh.include``:
````
# doxygen path
# for example, on a Mac:
export DOXYGEN_PATH="/Applications/Doxygen.app/Contents/Resources"

# dot path
export DOT_PATH="/opt/local/bin/dot"
````

------------------------------------------------------------------------------
The End.
