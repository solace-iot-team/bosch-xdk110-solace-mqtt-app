
#!/bin/bash

source ./config.sh.include

export PATH="$PATH:$DOXYGEN_PATH:$DOT_PATH"

export RESOURCE_PATH="./resources"

export SOURCE_PATH="../source"

# $(SOLACE_APP_VERSION)
export SOLACE_APP_VERSION="2.0.1"

# usage:  @date $(SOLACE_APP_DATE)
export SOLACE_APP_DATE="Jan-2019 to Feb-2020"

# usage:  @author $(SOLACE_APP_AUTHOR)
export SOLACE_APP_AUTHOR="Solace IoT Team"

#usage: $(XDK_VERSION)
export XDK_VERSION="3.6.1"

doxygen Doxyfile > doxygen.log 2>&1
less doxygen.log

# The End.
