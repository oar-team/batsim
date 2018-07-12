# The Git Hooks Directory
This directory contains git hooks that might be useful if you want to modify
Batsim's source code.

More information about git hooks can be found
[here](https://git-scm.com/book/it/v2/Customizing-Git-Git-Hooks).

## pre-commit
The [pre-commit](pre-commit) hook is called when you want to commit something.

This script checks whether the commit will probably fail on the CI.
Notably, it checks whether:
  - Doxygen produces warnings
  - Batsim can be built without warnings
  - The execution scripts work
  - Tests pass
