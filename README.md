# bicycle

a simple, dynamically typed programming language similar to Javascript and focused on simplicity.

## usage

Use CMake to build everything. This will build the common C++ library and the following executables:

### `bicycle_src_intrp`

Read and execute a source file and/or evaluate expressions from the user

usage: `bicycle_src_intrp (-i) ([input file]) (-- [args to program])`  
optional `-i` flag starts the REPL after reading the file if specified

### `bicycle_vmi`

Read and execute a compiled bytecode file

usage `bicycle_vmi [input file] [program arguments]`

### self-hosting

The source code in `src/self/` can be compiled into a compiler for the `bicycle_vmi` VM. A script is forthcoming, basically you can run `src/self/compile.bcy` using `bicycle_src_intrp` to generate bytecode for each module in the compiler. The bytecode files must have the same name as the source, with a `.bcc` extention. Once that process is finished you can run `compile.bcc` in `bicycle_vmi` the same way as from source and compile other things.

