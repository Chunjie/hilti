///
/// Outputs paths and flags for using HILTI and BinPAC++.
///
/// TODO: Currently we do not support installation outside of the built-tree
/// and all values returned here are thus in-tree.
///

#include <iostream>
#include <list>
#include <string>
#include <sstream>

#include <util/util.h>

#include <autogen/hilti-config.h>
#include <autogen/binpac-config.h>

using namespace std;

void usage()
{
    std::cerr << R"(
Usage: hilti-config [options]

== General options.

    --distbase              Print path of the HILTI source distribution.
    --prefix                Print path of installation (TODO: same as --distbase currently)
    --version               Print HILTI and BinPAC versions.
    --help                  Print this usage summary

== Options controlling what to include in output.

    --compiler              Output flags for integrating the C++ compilers into host applications.
    --runtime               Output flags for integrating the C runtime libraries into host applications.

    --debug                 Output flags for working with debugging versions.
    --disable-binpac++      Do not output flags specific to BinPAC++.

== Compiler and linker flags

    --cflags                Print flags for C compiler (--runtime only).
    --cxxflags              Print flags for C++ compiler.
    --ldflags               Print flags for linker.
    --libs                  Print libraries for linker.

== Options for running the command-line compilers.

    --hiltic-binary         Print the full path to the hiltic binary.
    --hilti-libdirs         Print standard HILTI library directories.
    --hilti-runtime-libs    Print the full path of the HILTI run-time library.

    --binpac++-binary       Print the full path to the binpac++ binary.
    --binpac++-libdirs      Print standard BinPAC++ library directories.
    --binpac++-runtime-libs Print the full path of the HILTI run-time library.


Example: hilti-config --compiler --cxxflags --debug

)";
}

void printList(stringstream& out, const std::list<string>& l)
{
    out << util::strjoin(l, " ") << std::endl;
}

void appendList(std::list<string>* dst, const std::list<string>& l, const string& prefix = "")
{
    for ( auto i : l )
        dst->push_back(prefix + i);
}

void appendList(std::list<string>* dst, const string& s, const string& prefix = "")
{
    dst->push_back(prefix + s);
}

int main(int argc, char** argv)
{
    bool want_hilti = true;
    bool want_binpac = true;
    bool want_compiler = false;
    bool want_runtime = false;
    bool want_debug = false;

    std::list<string> cflags;
    std::list<string> cxxflags;
    std::list<string> ldflags;
    std::list<string> libs;

    std::list<string> options;

    // First pass over arguments: look for control options.

    for ( int i = 1; i < argc; i++ ) {
        string opt = argv[i];

        if ( opt == "--help" || opt == "-h" ) {
            usage();
            return 0;
        }

        if ( opt == "--compiler" ) {
            want_compiler = true;
            continue;
        }

        if ( opt == "--runtime" ) {
            want_runtime = true;
            continue;
        }

        if ( opt == "--debug" ) {
            want_debug = true;
            continue;
        }

        if ( opt == "--disable-binpac++" ) {
            want_binpac = false;
            continue;
        }

        options.push_back(opt);
    }

    auto hilti_config = hilti::configuration();
    auto binpac_config = binpac::configuration();

    ////// HILTI

    if ( want_hilti ) {

        if ( want_compiler ) {
            appendList(&cflags, hilti_config.compiler_include_dirs, "-I");
            appendList(&cflags, hilti_config.compiler_cflags);
            appendList(&cxxflags, hilti_config.compiler_include_dirs, "-I");
            appendList(&cxxflags, hilti_config.compiler_cxxflags);
            appendList(&ldflags, hilti_config.compiler_ldflags);
            appendList(&libs, hilti_config.compiler_shared_libraries, "-l");
            appendList(&libs, util::strsplit(hilti_config.compiler_llvm_libraries));
        }

        if ( want_runtime ) {
            appendList(&cflags, hilti_config.runtime_cflags);
            appendList(&cflags, hilti_config.runtime_include_dirs, "-I");
            appendList(&cxxflags, hilti_config.runtime_cflags);
            appendList(&cxxflags, hilti_config.runtime_include_dirs, "-I");
            appendList(&ldflags, hilti_config.runtime_ldflags);
            appendList(&libs, hilti_config.runtime_shared_libraries, "-l");
            appendList(&libs, hilti_config.runtime_library_a);
        }
    }

    ////// BinPAC++

    if ( want_binpac ) {

        if ( want_compiler ) {
            appendList(&cflags, binpac_config.compiler_include_dirs, "-I");
            appendList(&cflags, binpac_config.compiler_cflags);
            appendList(&cxxflags, binpac_config.compiler_include_dirs, "-I");
            appendList(&cxxflags, binpac_config.compiler_cxxflags);
            appendList(&ldflags, binpac_config.compiler_ldflags);
            appendList(&libs, binpac_config.compiler_shared_libraries, "-l");
        }

        if ( want_runtime ) {
            appendList(&cflags, binpac_config.runtime_cflags);
            appendList(&cflags, binpac_config.runtime_include_dirs, "-I");
            appendList(&cxxflags, binpac_config.runtime_cflags);
            appendList(&cxxflags, binpac_config.runtime_include_dirs, "-I");
            appendList(&ldflags, binpac_config.runtime_ldflags);
            appendList(&libs, binpac_config.runtime_shared_libraries, "-l");
        }
    }

    bool need_component = false;
    stringstream out;

    for ( auto opt : options ) {

        if ( opt == "--distbase" ) {
            out << hilti_config.distbase << std::endl;
            continue;
        }

        if ( opt == "--prefix" ) {
            out << hilti_config.prefix << std::endl;
            continue;
        }

        if ( opt == "--version" ) {
            out << "HILTI "    << hilti_config.version << std::endl;
            out << "BinPAC++ " << binpac_config.version << std::endl;
            continue;
        }

        if ( opt == "--cflags" ) {
            need_component = true;
            printList(out, cflags);
            continue;
        }

        if ( opt == "--cxxflags" ) {
            need_component = true;
            printList(out, cxxflags);
            continue;
        }

        if ( opt == "--ldflags" ) {
            need_component = true;
            printList(out, ldflags);
            continue;
        }

        if ( opt == "--libs" ) {
            need_component = true;
            printList(out, libs);
            continue;
        }

        if ( opt == "--hiltic-binary" ) {
            out << hilti_config.path_hiltic << std::endl;
            continue;
        }

        if ( opt == "--hiltic-libdirs" ) {
            printList(out, hilti_config.hilti_library_dirs);
            continue;
        }

        if ( opt == "--hilti-runtime-libs" ) {
            if ( want_debug )
                out << hilti_config.runtime_library_bca_dbg << std::endl;
            else
                out << hilti_config.runtime_library_bca << std::endl;
            continue;
        }

        if ( opt == "--binpac++-binary" ) {
            out << binpac_config.path_binpacxx << std::endl;
            continue;
        }

        if ( opt == "--binpac++-libdirs" ) {
            printList(out, binpac_config.binpac_library_dirs);
            continue;
        }

        if ( opt == "--binpac++-runtime-libs" ) {
            if ( want_debug )
                out << binpac_config.runtime_library_bca_dbg << std::endl;
            else
                out << binpac_config.runtime_library_bca << std::endl;
            continue;
        }

        std::cerr << "hilti-config: unknown option " << opt << "; use --help to see list." << std::endl;
        return 1;

    }

    if ( need_component && ! (want_compiler || want_runtime) ) {
        cerr << "hilti-config: Either --compiler or --runtime (or both) must be given when printing flags." << std::endl;
        return 1;
    }


    cout << out.str();

    return 0;
}