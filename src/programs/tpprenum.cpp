/*! \file tpprenum.cpp
 *  \brief This file is used to launch the TPPRENUM, a program renumerates atoms from PDB.
 *
 * DB renumbering utility. The renumbering relies on the longest
 * bonded atomic sequence search. Atom names will be also modified.
 * TPPRENUM is used when atoms in  PDB file are posed in chaotic order.
 *
 */

#include "core.hpp"
#include "global.hpp"
#include "exceptions.hpp"
#include "runtime.hpp"
#include "paramset.hpp"
#include "pdbutils.hpp"
#include "structio.hpp"

#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/format.hpp>

namespace p_o = boost::program_options;
using tpp::cmdline;

using boost::format;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using namespace boost::numeric;

void helpscreen();

int main(int argc, char * argv[]) {

    string progname("Execution rules for TPPRENUM ");
    progname += PACKAGE_VERSION;
    p_o::options_description desc(progname);
    p_o::variables_map vars;
    desc.add_options()
    ("input,i",p_o::value<std::string>(),"Input filename (any format)")
    ("output,o",p_o::value<std::string>(),"Output filename (any format)")
    ("hex,x","Hexadecimal numbering of main chain")
    ("verbose,v","Verbose mode")
    ("ignore-index,g","Don't ignore index")
    ("help,h", "Print this message")
    ;
    try {
        try { // блок вылавливания исключений boost::program_options
            p_o::store(p_o::parse_command_line(argc, argv, desc), vars);
            p_o::notify(vars);
            if ( (vars.count("verbose") > 1) || (vars.count("ignore-index") > 1) ) throw 1;

            cmdline.add("verbose_flag", vars.count("verbose") ? "on" : "off" );
            cmdline.add("hex_flag",     vars.count("hex") ? "on" : "off" );
            cmdline.add("ignore_index", vars.count("ignore-index") ? "off" : "on" );

            if (vars.count("ignore-index") == 1) {
                cout << "Non-ignoring of indexes is a DANGEROUS MODE!" << endl;
            }
            if (vars.count("help") == 1) helpscreen();
            if (vars.count("input") == 1) {
                cmdline.add("input_file", vars["input"].as<std::string>() );
            }
            else throw 1;
            if (vars.count("output") == 1) {
            	cmdline.add("output_file", vars["output"].as<std::string>() );
            }
            else throw 1;
        }
        catch (boost::program_options::error &e) {
            throw 1;
        }
    }
    catch (int ExC) {
        if (ExC) {
            cerr << format("\nTPPRENUM %1% : Error in input parameters.\n\n") % VERSION;
            cerr << desc;
        }
        return(ExC);
    }
    //
	// finished analysing
	// starting work with input and output files
	//

    if (cmdline.read("verbose_flag") == "on") {
        cout << format ("\
**********************************************************************\n\
*   Biology faculty, Department of biophysics, Erg Research Group    *\n\
*   Moscow, Lomonosov's Moscow State University                      *\n\
*   for more info, see homepage  http://erg.biophys.msu.ru/          *\n\
*                                                                    *\n\
*   Authors:       comcon1, dr.zoidberg, piton                       *\n\
*                                                                    *\n\
*   Product:       program  TPPRENUM-%1$-6s                          *\n\
*                                                                    *\n\
*    PDB renumbering utility. The renumbering relies on the longest  *\n\
* bonded atomic sequence search. Atom names will be also modified.   *\n\
* You need to run TPPRENUM when atoms in your PDB file are posed in  *\n\
* chaotic order.                                                     *\n\
*                                                                    *\n\
*   Modified:     %2$-19s                                *\n\
**********************************************************************\n\
\n\n") % PACKAGE_VERSION % CONFIGURE_CDATE;
    } else {
        cout << format("Starting TPPRENUM-%1$s program.\n") % VERSION;
    }



    // INPUT analysing
    tpp::InputFormat iform;
    tpp::OutputFormat oform;
    string::size_type ind = cmdline.read("input_file").find(".", 0);
    if ( ind == string::npos) {
        cerr << "ERROR:\n";
        cerr << "Couldn't determine format of input file. Please specify extension.\n";
        return 1;
    }
    string subs = cmdline.read("input_file").substr(ind+1);
    if (subs == "pdb") iform = tpp::TPP_IF_PDB;
    else if (subs == "gro") iform = tpp::TPP_IF_GRO;
    else if (subs == "g96") iform = tpp::TPP_IF_G96;
    else if ( (subs == "log") || (subs == "out") ) iform = tpp::TPP_IF_GAMSP;
    else {
        cerr << "ERROR:\n";
        cerr << "Couldn't determine format of input file. Please specify other extension.\n";
        return 1;
    }

    if (cmdline.read("verbose_flag") == "on") {
        switch (iform) {
        case   tpp::TPP_IF_PDB:
            cout << "Input file format: Protein Data Bank." << endl;
            break;
        case   tpp::TPP_IF_GRO:
            cout << "Input file format: GROmacs structure." << endl;
            break;
        case   tpp::TPP_IF_G96:
            cout << "Input file format: Gromos 96 structure." << endl;
            break;
        case tpp::TPP_IF_GAMSP:
            cout << "Input file format: GAMess output." << endl;
            break;
        };
    }

    // OUTPUT analysing
    ind = cmdline.read("output_file").find(".",0);
    if ( ind == string::npos) {
        cerr << "ERROR:\n";
        cerr << "Couldn't determine format of output file. Please specify extension.\n";
        return 1;
    }
    subs = cmdline.read("output_file").substr(ind+1);
    if (subs == "pdb") oform = tpp::TPP_OF_PDB;
    else if (subs == "gro") oform = tpp::TPP_OF_GRO;
    else if (subs == "g96") oform = tpp::TPP_OF_G96;
    else {
        cerr << "ERROR:\n";
        cerr << "Couldn't determine format of output file. Please specify other extension.\n";
        return 1;
    }

    if (cmdline.read("verbose_flag") == "on") {
        switch (oform) {
        case   tpp::TPP_OF_PDB:
            cout << "Output file format: Protein Data Bank." << endl;
            break;
        case   tpp::TPP_OF_GRO:
            cout << "Output file format: GROmacs structure." << endl;
            break;
        case   tpp::TPP_OF_G96:
            cout << "Output file format: Gromos 96 structure." << endl;
            break;
        };
    }

    // main program body, using modules
    try {
        tpp::Topology topology;
        tpp::StructureIO io;
        io.loadFromFile(topology, iform, cmdline.read("input_file").c_str() );
        ublas::vector<unsigned> tail1 = tpp::generate_long_tail1(topology.mol);
        topology.atoms = tpp::mol_renum1(topology.mol, topology.atoms, tail1 );
        io.saveToFile(topology, oform, cmdline.read("output_file").c_str() );
    } catch (tpp::Exception e) {
        cerr << "  TPP_EXCEPTION FROM: " << e["procname"] << endl;
        cerr << "  With following error: " << e["error"] << endl;
        cerr << "  more info see in log-file." << endl;
        return 2;
    }
    cout << "TPPRENUM finished normally!" << endl;
    return 0;
}

/*!
 * \brief Prints program info and usage to stdout.
 */
void helpscreen()
{
    cout << format("\n\
--------------------------------*****---------------------------------\n\
                                          ERG Research Group,         \n\
                                          Department of biophysics,   \n\
                                          Biology faculty, MSU, Russia\n\
\n\
           THE PART OF TOPOLOGY PREPROCESSOR PROJECT                  \n\
        ---      (comcon1, zoidberg, piton)       ---                 \n\
  TPP version: %1$-3s, compiled at %2$-8s on GCC %3$s.\n\
  BOOST version:  %4$-8s \n\
  OpenBabel version: %5$-8s \n\
  OpenBabel share data: %6$-8s \n\
\n\
                                TPPRENUM\n\
    PDB renumbering utility. The renumbering relies on the longest  \n\
 bonded atomic sequence search. Atom names will be also modified.   \n\
 You need to run TPPRENUM when atoms in your PDB file are posed in  \n\
 chaotic order.                                                     \n\
\n\
 USAGE: \n\
 tpprenum -i <input> -o <output> [-v]   \n\
      -i  the name of (I)nput-file, in PDB or GRO/G96 format.           \n\
      -o  the name of (O)utput-file, contained prepared structure.      \n\
      -v  (V)erbose mode, typing more information during the execution\n\
      -x  he(X)adecimal numbering of main (heavy-atom) chain          \n\
      -g  Do not i(G)nore index in PDB (vs sequental order).    \n\
      -h  print this message.                                         \n\
\n\
--------------------------------*****---------------------------------\n\
") % PACKAGE_VERSION % CONFIGURE_CDATE % __VERSION__ % BOOST_LIB_VERSION
         % BABEL_VERSION % BABEL_DATADIR << endl;
    throw 0;
}


