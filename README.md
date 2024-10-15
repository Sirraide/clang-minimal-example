# How to use Clang as a library out-of-tree
This file describes my way of linking against Clang and how to use Clang to parse C or C++ code and operate on the resulting AST.

tl;dr: Scroll all the way to the bottom to see the full code (CMake and source code) for this example. 

## Basic CMake Setup
LLVM uses CMake, so that’s what we’ll be using; I have no idea how to do this without using CMake. First, we enable C and C++ (even
if we only use C++, some of the CMake files we’ll be using shortly require C as well). The CMake version doesn’t matter too much, so
just pick something recent.
```cmake
cmake_minimum_required(VERSION 3.12)
project(example LANGUAGES C CXX)
```

Next, we need to tell CMake where to find LLVM (and by extension, Clang). If your system already has a version of LLVM installed, you
can try using that; that usually works for LLVM, but package managers in my experience at least often don’t have a `clang-devel` package
or anything similar, and our releases generally don’t include any compiled Clang libraries either, which means you will probably have to
build LLVM yourself (see also https://llvm.org/docs/CMake.html):
```bash
$ git clone https://github.com/llvm/llvm-project --depth=1
$ cd llvm-project/llvm
$ cmake -B out -S . -DLLVM_ENABLE_PROJECTS=clang # (I also recommend using Ninja, as well as `mold` as a linker.)
$ cmake --build out
```

Once you’ve obtained a build of LLVM, we need to tell CMake where to find it; we’ll be using a variable for this so other people can
pass a custom path at configure time (we’ll get back to this later).
```cmake
if (NOT DEFINED LLVM_PATH)
    message(FATAL_ERROR "Please set LLVM_PATH to the directory where you built LLVM")
endif()
```

We can now use `find_package` to get most of the information we need (include dirs etc.).
```cmake
set(LLVM_DIR "${LLVM_PATH}/lib/cmake/llvm")
set(Clang_DIR "${LLVM_PATH}/lib/cmake/clang")
find_package(LLVM REQUIRED CONFIG NO_DEFAULT_PATH)
find_package(Clang REQUIRED CONFIG NO_DEFAULT_PATH)
message(STATUS "Found LLVM ${LLVM_VERSION}")
```

Here, we take care to tell CMake to actually find the LLVM version that we want it to use. Even though we mainly want to use Clang, we also need to do this for LLVM; otherwise, you can get into weird situations where the `find_package` call for Clang finds one LLVM version, and the one for LLVM a different one. Passing `NO_DEFAULT_PATH` also helps to make sure we don’t accidentally use our system LLVM (in case the user didn’t supply a valid LLVM_PATH...).

Next, we include some helpers that provide us with some default settings to get started so we don’t need to set up every last detail ourselves. Once again, we need to tell CMake where to find these, but fortunately, the `find_package` calls above have already populated the variables we need for this:
```cmake
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CLANG_CMAKE_DIR}")
include(AddLLVM)
include(AddClang)
``` 

One of these, `AddClang`, defines `add_clang_executable`, which we use in place of `add_executable` to create our example program:
```cmake
add_clang_executable(example
    main.cc
)
```

Next, we need to make sure the compiler finds the right Clang and LLVM headers; omitting this step will cause your program to crash because the compiler finds the system installation’s headers, but we’re linking against the one we just built (the CMake variables for this once again have already been populated by what we’ve done so far). I like to mark these as `SYSTEM` includes so we don’t get any warnings in random LLVM or Clang headers.
```cmake
target_include_directories(example SYSTEM PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)
``` 

Lastly, we link against any libraries we need. Each subdirectory in `clang/include` is
a separate library (e.g. `clang/include/AST` corresponds to the `clangAST` library and CMake target), so if you use any headers from a directory, make sure to link against that directory’s library too. Here are the ones that we’ll be needing:
```cmake
target_link_libraries(example PRIVATE
    clangAST
    clangFrontend
    clangTooling
)
```

## Source code
For this example, we’ll write a program that takes C++ code as a command-line argument, builds an AST from it, and then dumps the entire translation unit to stdout. Of course, once you’ve built the AST, you can do with it whatever you want; this is just a simple example to get you started.

The main function we’ll be using is `clang::tooling::buildASTFromCodeWithArgs()`; we’ll need a few headers for that:
```c++
#include <vector>
#include <string>
#include <clang/Tooling/Tooling.h>   // for clang::tooling::buildASTFromCodeWithArgs
#include <clang/Frontend/ASTUnit.h>  // for clang::ASTUnit
#include <clang/AST/ASTContext.h>    // for clang::ASTContext
#include <clang/AST/Decl.h>          // for clang::TranslationUnitDecl
```

We then invoke this function, passing it our source code (which we’ll just take from the command-line here) as well as some arguments; let’s say we want to treat the input as C++20 and enable `-Wall`.
```c++
using namespace clang;
int main([[maybe_unused]] int argc, char** argv) {
    std::vector<std::string> Args {
        "-std=c++20",
        "-Wall",
    };

    // This parses the code and hands us back an 'ASTUnit', which is
    // the parsed AST together with all of the other state that was
    // used during parsing (e.g. the Preprocessor, the Sema instance,
    // etc.)
    std::unique_ptr<ASTUnit> AST = tooling::buildASTFromCodeWithArgs(
        argv[1],
        Args
    );

    // Something went horribly wrong, bail out.
    if (!AST) return 1;

    // We usually still have a somewhat-valid AST even if there was
    // an error (which will have already been printed), but for this
    // example, we also bail out if there was a compile error.
    if (AST->getDiagnostics().hasUncompilableErrorOccurred()) return 1;

    // Dump the entire AST.
    AST->getASTContext().getTranslationUnitDecl()->dumpColor();
}
```

So far, so good; building this program and running it on a simple code
snippet produces what we want:
```bash
$ ./example 'int main() {}'
```

The output has some builtin-type-definition nonsense at the top, but we can see that our `main` function indeed shows up as the `FunctionDecl` node at the very bottom of the output.
```console
TranslationUnitDecl 0x555742d4b2c8 <<invalid sloc>> <invalid sloc>
|-TypedefDecl 0x555742d4bfd0 <<invalid sloc>> <invalid sloc> implicit __int128_t '__int128'
| `-BuiltinType 0x555742d4b890 '__int128'
|-TypedefDecl 0x555742d4c048 <<invalid sloc>> <invalid sloc> implicit __uint128_t 'unsigned __int128'
| `-BuiltinType 0x555742d4b8b0 'unsigned __int128'
|-TypedefDecl 0x555742da0878 <<invalid sloc>> <invalid sloc> implicit __NSConstantString '__NSConstantString_tag'
| `-RecordType 0x555742d4c150 '__NSConstantString_tag'
|   `-CXXRecord 0x555742d4c0a8 '__NSConstantString_tag'
|-TypedefDecl 0x555742d4bbb0 <<invalid sloc>> <invalid sloc> implicit __builtin_ms_va_list 'char *'
| `-PointerType 0x555742d4bb60 'char *'
|   `-BuiltinType 0x555742d4b370 'char'
|-TypedefDecl 0x555742d4bf58 <<invalid sloc>> <invalid sloc> implicit __builtin_va_list '__va_list_tag[1]'
| `-ConstantArrayType 0x555742d4bf00 '__va_list_tag[1]' 1 
|   `-RecordType 0x555742d4bcc0 '__va_list_tag'
|     `-CXXRecord 0x555742d4bc10 '__va_list_tag'
`-FunctionDecl 0x555742da0930 <input.cc:1:1, col:13> col:5 main 'int ()'
  `-CompoundStmt 0x555742da0a78 <col:12, col:13>
```

## System headers
Alright, let’s try something fancier! Real code usually includes headers, so let’s dump the contents of all of `<vector>`:
```bash
$ ./example '#include <vector>'
```

Unfortunately, this doesn’t work:
```console
In file included from input.cc:1:
In file included from /../lib/gcc/x86_64-redhat-linux/14/../../../../include/c++/14/vector:87:
In file included from /../lib/gcc/x86_64-redhat-linux/14/../../../../include/c++/14/bits/memory_resource.h:38:
/../lib/gcc/x86_64-redhat-linux/14/../../../../include/c++/14/cstddef:50:10: fatal error: 'stddef.h' file not found
   50 | #include <stddef.h>
      |          ^~~~~~~~~~
```

The reason for this is that some headers, like `stddef.h`, are distributed as part of Clang itself rather than being part of the system or standard library. During normal compilation, the way Clang finds these headers is kind of obscure: they are contained in the so called ‘resource directory’, the path to which is resolved *relative* to the path of the Clang executable. The fix for this is to tell `buildASTFromCodeWithArgs()` where a Clang executable is. Fortunately, we just built one, so we can use that:
```cmake
# Tell our program where clang is.
target_compile_definitions(example PRIVATE
    "CLANG_PATH=\"${LLVM_PATH}/bin/clang++\""
)
```

We then pass this to `buildASTFromCodeWithArgs()` as the 4th argument (`ToolName`). The 3rd argument is the ‘file name’ of our input; this is used in diagnostics, but also to determine the source language, so make sure to use `.c` if you want the input to be parsed as C, and some C++ file extension (`.cc`, `.cpp`, etc.) if you want it to be parsed as C++.
```c++
std::unique_ptr<ASTUnit> AST = tooling::buildASTFromCodeWithArgs(
    argv[1],
    Args,
    "input.cc",
    CLANG_PATH
);
```

Now, we can finally dump the contents of `<vector>`. This can take a while if you’re using an IDE’s integrated terminal or a terminal on Windows since the AST is rather huge.
```bash
$ ./example '#include <vector>'
```

## Code listing
### `CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.12)
project(example LANGUAGES C CXX)

# Ensure that the user has set LLVM_PATH to the directory
# containing the LLVM build they want to use.
if (NOT DEFINED LLVM_PATH)
    message(FATAL_ERROR "Please set LLVM_PATH to the directory where you built LLVM")
endif()

# Find LLVM and Clang.
set(LLVM_DIR "${LLVM_PATH}/lib/cmake/llvm")
set(Clang_DIR "${LLVM_PATH}/lib/cmake/clang")
find_package(LLVM REQUIRED CONFIG NO_DEFAULT_PATH)
find_package(Clang REQUIRED CONFIG NO_DEFAULT_PATH)

# Print the package version so we can tell at a glance
# if we actually did this right.
message(STATUS "Found LLVM ${LLVM_VERSION}")

# Include the required LLVM and Clang CMake modules.
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CLANG_CMAKE_DIR}")
include(AddLLVM)
include(AddClang)

# Define our executable.
add_clang_executable(example
    main.cc
)

# Add Clang's and LLVM's include directories.
target_include_directories(example SYSTEM PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

# Link against any libraries we need.
target_link_libraries(example PRIVATE
    clangAST
    clangFrontend
    clangTooling
)

# Tell our program where clang is.
target_compile_definitions(example PRIVATE
    "CLANG_PATH=\"${LLVM_PATH}/bin/clang++\""
)
```

### `main.cc`:
```c++
#include <vector>
#include <string>
#include <clang/Tooling/Tooling.h>   // for clang::tooling::buildASTFromCodeWithArgs
#include <clang/Frontend/ASTUnit.h>  // for clang::ASTUnit
#include <clang/AST/ASTContext.h>    // for clang::ASTContext
#include <clang/AST/Decl.h>          // for clang::TranslationUnitDecl

using namespace clang;
int main([[maybe_unused]] int argc, char** argv) {
    std::vector<std::string> Args {
        "-std=c++20",
        "-Wall",
    };

    // This parses the code and hands us back an 'ASTUnit', which is
    // the parsed AST together with all of the other state that was
    // used during parsing (e.g. the Preprocessor, the Sema instance,
    // etc.)
    std::unique_ptr<ASTUnit> AST = tooling::buildASTFromCodeWithArgs(
        argv[1],
        Args,
        "input.cc",
        CLANG_PATH
    );

    // Something went horribly wrong, bail out.
    if (!AST) return 1;

    // We usually still have a somewhat-valid AST even if there was
    // an error (which will have already been printed), but for this
    // example, we also bail out if there was a compile error.
    if (AST->getDiagnostics().hasUncompilableErrorOccurred()) return 1;

    // Dump the entire AST.
    AST->getASTContext().getTranslationUnitDecl()->dumpColor();
}
```
