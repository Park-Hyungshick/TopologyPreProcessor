#include "topio.hpp"
#include "openbabel/obconversion.h"
#include "openbabel/obiter.h"
#include "strutil.h"
#include <cctype>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#define PHOENIX_LIMIT 5
#include "lexical.hpp"
#include <boost/spirit/attribute.hpp>
#include <boost/spirit/phoenix/primitives.hpp>
#include <boost/spirit/phoenix/operators.hpp>
#include <boost/spirit/phoenix/functions.hpp>
#include <boost/spirit/phoenix/binders.hpp>
#include <boost/spirit/phoenix/casts.hpp>
#include <boost/spirit/utility/regex.hpp>


#define OPENBABEL
#define BOOST_SPIRIT_DEBUG

 
namespace tpp {

using OpenBabel::OBConversion;
using OpenBabel::OBMol;
using OpenBabel::OBMolAtomIter;
using OpenBabel::OBAtomAtomIter;
using namespace phoenix;


void mol_to_atoms(t_topology &tp) throw (t_exception) {
  tp.atoms.clear();
  pair<t_atom_array::iterator, bool> res;
  FOR_ATOMS_OF_MOL(it,tp.mol) {
    t_atom cur0;
    cur0.index = it->GetIdx();
    cur0.coord(0) = it->GetX();
    cur0.coord(1) = it->GetY();
    cur0.coord(2) = it->GetZ();
    cur0.mol_id   = 1;
    cur0.res_name = it->GetResidue()->GetName();
    cur0.atom_name = string("") + it->GetType(); 
    cur0.atom_type = string("") + it->GetType(); 
    cur0.charge = it->GetPartialCharge();
    cur0.mass = it->GetAtomicMass();
    cur0.ncharge = it->GetAtomicNum();
    cur0.num_connects = it->GetValence();
    int iii=0;
    FOR_NBORS_OF_ATOM(ait, &*it) {
      cur0.connects[iii] = ait->GetIdx();
      iii++;
    }
    res = tp.atoms.insert(cur0);
    if (!res.second) {
               t_input_params params;
               PARAM_ADD(params, "procname", "tpp::load_struct");
               PARAM_ADD(params, "error", "PDB parsing error");
               throw t_exception("Repeat index or something else.", params);
    }
  }
}

void save_topology_rtp(t_topology &tp, const char *fname) throw (t_exception) {
  // test if file exists
  runtime.log_write(string("Trying to write RTP-topology into '")+fname+"'.\n");
  fstream out(fname, ios::out);
  if (!out.is_open()) { 
    runtime.log_write("Fail to open file for write.\n");
    BOOST_CHECK(0);
  }
  // header
  out << format("\
%1%\
[ %2$4s ]\n\
") % top_comment % tp.res_name;

  // atoms specification
  out << "\n[ atoms ]\n";
  for (t_atom_array::iterator it = tp.atoms.begin(); it != tp.atoms.end(); ++it) {
    out << format("%1$-4s %2$-4s %3$6.3f %4$3d \n") %
           it->atom_name.c_str() % it->atom_type.c_str() % it->charge %
           (int)(it->c_gnr);
  }
  out << flush;

  // bonds
  if (tp.parameters.get<1>().find(TPP_TTYPE_BON) != tp.parameters.get<1>().end()) {
    out << "\n[ bonds ]\n";
    for (t_top_map::nth_index_iterator<1>::type it = tp.parameters.get<1>().lower_bound(TPP_TTYPE_BON);
         it != tp.parameters.get<1>().upper_bound(TPP_TTYPE_BON); ++it)
      for (t_top_array::nth_index_iterator<1>::type it0 = tp.elements.get<1>().lower_bound(it->defname);
           it0 != tp.elements.get<1>().upper_bound(it->defname); ++it0)
        if (it->f == -1) {
          out << format("%1$4s %2$4s bndTPP_%3$s_%4$s \n") 
                  % tp.atoms.find((int)it0->i)->atom_name.c_str()
                  % tp.atoms.find((int)it0->j)->atom_name.c_str()
                  % tp.atoms.find((int)it0->i)->atom_type2.c_str()
                  % tp.atoms.find((int)it0->j)->atom_type2.c_str();
        } else {
          out << format("%1$4s %2$4s \n")
                  % tp.atoms.find((int)it0->i)->atom_name.c_str()
                  % tp.atoms.find((int)it0->j)->atom_name.c_str();
        }
  }

  // angles
  bool flag = true;
  for (t_top_map::nth_index<2>::type::iterator it = tp.parameters.get<2>().lower_bound(-1);
      it != tp.parameters.get<2>().upper_bound(-1); ++it)  
    if (it->type == TPP_TTYPE_ANG)
      for (t_top_array::nth_index_iterator<1>::type it0 = tp.elements.get<1>().lower_bound(it->defname);
           it0 != tp.elements.get<1>().upper_bound(it->defname); ++it0) {
        if (flag) {
          out << "\n[ angles ]\n";
          flag = false;
        }
        out << format("%1$4s %2$4s %3$4s dhTPP_%4$s_%5$s_%6$s \n") 
                % tp.atoms.find((int)it0->i)->atom_name.c_str()
                % tp.atoms.find((int)it0->j)->atom_name.c_str()
                % tp.atoms.find((int)it0->k)->atom_name.c_str()
                % tp.atoms.find((int)it0->i)->atom_type2.c_str()
                % tp.atoms.find((int)it0->j)->atom_type2.c_str()
                % tp.atoms.find((int)it0->k)->atom_type2.c_str();
      }

  // dihedrals
  flag = true;
  for (t_top_map::nth_index<2>::type::iterator it = tp.parameters.get<2>().lower_bound(-1);
      it != tp.parameters.get<2>().upper_bound(-1); ++it)  
    if (it->type == TPP_TTYPE_RBDIH)
      for (t_top_array::nth_index_iterator<1>::type it0 = tp.elements.get<1>().lower_bound(it->defname);
           it0 != tp.elements.get<1>().upper_bound(it->defname); ++it0) {
        if (flag) {
          out << "\n[ dihedrals ]\n";
          flag = false;
        }
        out << format("%1$4s %2$4s %3$4s %4$4s dhTPP_%5$s_%6$s_%7$s_%8$s \n") 
                % tp.atoms.find((int)it0->i)->atom_name.c_str()
                % tp.atoms.find((int)it0->j)->atom_name.c_str()
                % tp.atoms.find((int)it0->k)->atom_name.c_str()
                % tp.atoms.find((int)it0->l)->atom_name.c_str()
                % tp.atoms.find((int)it0->i)->atom_type2.c_str()
                % tp.atoms.find((int)it0->j)->atom_type2.c_str()
                % tp.atoms.find((int)it0->k)->atom_type2.c_str()
                % tp.atoms.find((int)it0->l)->atom_type2.c_str();
      }

  out << "; topology successfully writed" << endl;
  out.close();
}

void save_topology(t_topology &tp, const char *fname) throw (t_exception) {
  bool ncf = (PARAM_READ(cmdline, "nocalculate_flag") == "on");
  // test if file exists
  runtime.log_write(string("Trying to write topology into '")+fname+"'.\n");
  fstream out(fname, ios::out);
  if (!out.is_open()) { 
    runtime.log_write("Fail to open file for write.\n");
    BOOST_CHECK(0);
  }
  // header
  out << format("\
%1%\
#include <%2%>\n\
\n\
[ moleculetype ]\n\
 %3$4s %4$1d\n\
\n\
; Force constant parameters\n") % top_comment % tp.ffinclude.c_str() % tp.res_name % (int)(tp.nrexcl);
  // force constants parameters '#define's
  for (t_top_map::nth_index_iterator<1>::type it = tp.parameters.get<1>().begin(); 
        it != tp.parameters.get<1>().upper_bound(TPP_TTYPE_SYMDIH); ++it) {
    if ( (it->f != -1) ^  ncf ) {
      switch (it->type) {
       case TPP_TTYPE_BON:   out << format("#define %1$-15s %2$1d %3$5.3f %4$9.2e ;1\n") 
                              % it->defname % it->f % it->c0 % it->c1; break;
       case TPP_TTYPE_ANG:   out << format("#define %1$-15s %2$1d %3$5.1f %4$6.1f ;2\n")
                             % it->defname % it->f % it->c0 % it->c1; break;
       case TPP_TTYPE_RBDIH: out << format("#define %1$-15s 3 %2$+5.1f %3$+5.1f %4$+5.1f %5$+5.1f %6$+5.1f %7$+5.1f ;3\n")
                             % it->defname % it->c0 % it->c1 % it->c2 % it->c3 % it->c4 % it->c5; break;
       case TPP_TTYPE_IMPDIH:out << format("#define %1$-15s 2 %2$5.1f %3$6.1f ;4\n")
                             % it->defname % it->c0 % it->c1; break;
       case TPP_TTYPE_SYMDIH:out << format("#define %1$-15s 1 %2$5.1f %3$6.1f %4$1d ;5\n")
                             % it->defname % it->c0 % it->c1 % numeric_cast<unsigned>(it->c2);
      };
    }

  }
  // atoms specification
  out << "\n[ atoms ]\n";
  for (t_atom_array::iterator it = tp.atoms.begin(); it != tp.atoms.end(); ++it) {
    out << format("%1$3d  %2$-10s 1  %3$-4s  %4$-4s  %5$2d  %6$5.3f  %7$8.6f ; %8$s\n") %
           (int)(it->index) % it->atom_type.c_str() % tp.res_name.c_str() % it->atom_name.c_str() % 
           (int)(it->c_gnr) % it->charge % it->mass % it->comment;
  }
  out << flush;
  // bonds
  if (tp.parameters.get<1>().find(TPP_TTYPE_BON) != tp.parameters.get<1>().end()) {
    out << "\n[ bonds ]\n";
    for (t_top_map::nth_index_iterator<1>::type it = tp.parameters.get<1>().lower_bound(TPP_TTYPE_BON);
         it != tp.parameters.get<1>().upper_bound(TPP_TTYPE_BON); ++it)
      for (t_top_array::nth_index_iterator<1>::type it0 = tp.elements.get<1>().lower_bound(it->defname);
           it0 != tp.elements.get<1>().upper_bound(it->defname); ++it0)
        if (ncf && (it->f != -1) )
          out << format("%1$3d %2$3d  %3$2d\n") % (int)it0->i % (int)it0->j % it->f;
        else
          out << format("%1$3d %2$3d  %3$-15s\n") % (int)it0->i % (int)it0->j % it0->defname;
  }
  // angles
  if (tp.parameters.get<1>().find(TPP_TTYPE_ANG) != tp.parameters.get<1>().end()) {
    out << "\n[ angles ]\n";
    for (t_top_map::nth_index_iterator<1>::type it = tp.parameters.get<1>().lower_bound(TPP_TTYPE_ANG);
         it != tp.parameters.get<1>().upper_bound(TPP_TTYPE_ANG); ++it)
      for (t_top_array::nth_index_iterator<1>::type it0 = tp.elements.get<1>().lower_bound(it->defname);
           it0 != tp.elements.get<1>().upper_bound(it->defname); ++it0)
        if (ncf && (it->f != -1) )
          out << format("%1$3d %2$3d %3$3d  %4$2d\n") % (int)it0->i % (int)it0->j % (int)it0->k % it->f;
        else
          out << format("%1$3d %2$3d %3$3d  %4$-15s\n") % (int)it0->i % (int)it0->j % (int)it0->k % it0->defname;
  }
  // dihedrals
  if ( ( tp.parameters.get<1>().find(TPP_TTYPE_RBDIH) != tp.parameters.get<1>().end() ) ||
       ( tp.parameters.get<1>().find(TPP_TTYPE_IMPDIH) != tp.parameters.get<1>().end() ) ||
       ( tp.parameters.get<1>().find(TPP_TTYPE_SYMDIH) != tp.parameters.get<1>().end() ) 
      ) {
    out << "\n[ dihedrals ]\n";
    for (t_top_map::nth_index_iterator<1>::type it = tp.parameters.get<1>().lower_bound(TPP_TTYPE_RBDIH);
         it != tp.parameters.get<1>().upper_bound(TPP_TTYPE_SYMDIH); ++it)
      for (t_top_array::nth_index_iterator<1>::type it0 = tp.elements.get<1>().lower_bound(it->defname);
           it0 != tp.elements.get<1>().upper_bound(it->defname); ++it0)
        if (ncf && (it->f != -1) )
          out << format("%1$3d %2$3d %3$3d %4$3d %5$2d\n") % (int)it0->i % (int)it0->j % (int)it0->k % (int)it0->l % it->f;
        else
          out << format("%1$3d %2$3d %3$3d %4$3d %5$-15s\n") % (int)it0->i % (int)it0->j % (int)it0->k % (int)it0->l % it0->defname;
  }
  // pairs
  if ( tp.parameters.get<1>().count(TPP_TTYPE_PAIR) > 1 ) {
    out << "\n[ pairs ]\n";
    for (t_top_map::nth_index_iterator<1>::type it = tp.parameters.get<1>().lower_bound(TPP_TTYPE_PAIR);
         it != tp.parameters.get<1>().upper_bound(TPP_TTYPE_PAIR); ++it)
      for (t_top_array::nth_index_iterator<1>::type it0 = tp.elements.get<1>().lower_bound(it->defname);
           it0 != tp.elements.get<1>().upper_bound(it->defname); ++it0) {
        if (it0->defname == "ONE_PAIR") 
          out << format("%1$3d %2$3d %3$1d\n") % (int)it0->i % (int)it0->j % it->f;
        else
          out << format("%1$3d %2$3d %3$1d %4$10.5f %5$10.5f\n") % (int)it0->i % (int)it0->j % it->f % it->c0 % it->c1;
      }
  }
  // exclusion
  if ( tp.parameters.get<1>().find(TPP_TTYPE_EXCL) != tp.parameters.get<1>().end() ) {
    out << "\n[ exclusions ]\n";
    string defname = (tp.parameters.get<1>().find(TPP_TTYPE_EXCL))->defname; // in map there is only one exclusion
      for (t_top_array::nth_index_iterator<1>::type it0 = tp.elements.get<1>().lower_bound(defname);
           it0 != tp.elements.get<1>().upper_bound(defname); ++it0)
        out << format("%1$3d %2$3d\n") % it0->i % it0->j;
  }
  out << "; topology successfully writed" << endl;
  out.close();
}

// save information about lacking topology
void save_lack(t_topology &tp, const char *fname) {
      cout << format("TPP will write %1$d lack parameters to %2$s.\n") 
        % tp.parameters.get<2>().count(-1) % fname;
      std::ofstream qalcfile(fname, ios::out);
      BOOST_CHECK(qalcfile.is_open());
      qalcfile << "; TPP topology lack\n";
      for (t_top_map::nth_index<2>::type::iterator typit = tp.parameters.get<2>().lower_bound(-1);
         typit != tp.parameters.get<2>().upper_bound(-1); ++typit) {  
        switch (typit->type) {
          case TPP_TTYPE_BON:
           qalcfile << format("#define %1$s 1 0.00 0.00 ;1\n") % typit->defname;
           break;
          case TPP_TTYPE_ANG:
           qalcfile << format("#define %1$s 1 0.00 0.00 0.00 ;2\n") % typit->defname;
           break;
          case TPP_TTYPE_IMPDIH: 
           qalcfile << format("#define %1$s 2 0.00 0.00 0.00 ;3\n") % typit->defname;
           break;
          case TPP_TTYPE_RBDIH:
           qalcfile << "; Overwritted with pairs: \n";
           qalcfile << format("#define %1$s 3 0.00 0.00  0.00 0.00 0.00 ;4") % typit->defname;
           break;
        };
      }
      qalcfile << flush;
      qalcfile.close();
} // end save_lack

template<typename T>
void print(T a) {
  cout << a;
}

typedef ublas::matrix<double> umd;
static member_function_ptr<umd::reference, umd,  umd::size_type, umd::size_type, umd::const_reference> 
 ins_elem = &umd::insert_element;

extern void load_hessian(ublas::matrix<double>& mtx, const char *fname) throw (t_exception) {
   {
  runtime.log_write(string("Trying to read ") + fname + " into hessian matrix.\n");
  fstream inf(fname, ios::in);
  if (!inf.is_open()) { 
    runtime.log_write("Fail to open file for read.\n");
    BOOST_CHECK(0);
  }
  inf.close(); 
   }

  lex::iterator first(fname);
  lex::iterator last = first.make_end();
  lex::rule_r soms, define, common;
  int num_x = 0, num_y = 0;
  int x = 1, y = 1;
  soms = ( (*(anychar_p - eol_p)) - ( +blank_p >> str_p("CARTESIAN FORCE CONSTANT MATRIX") >> *(anychar_p - eol_p) ) )  >> eol_p;
  common = +soms >>
    +blank_p >> str_p("CARTESIAN FORCE CONSTANT MATRIX")[var(x) = 0][var(y) = 0]>> *(anychar_p - eol_p) >> eol_p
    >> *(anychar_p - eol_p) >> eol_p >>
    // starting one block
    +( *blank_p >> eol_p >>
      (+blank_p >> uint_p[assign_a(num_x)] >> !(+blank_p >> uint_p) >> *blank_p >> eol_p)[&lex::echo::val_print] >>
      (+blank_p >> +alnum_p >> !(+blank_p >> +alnum_p) >> *blank_p >> eol_p)[&lex::echo::val_print] >>
      *(anychar_p - eol_p) >> eol_p >> // " x y z x y z" string
      repeat_p(3,boost::spirit::more) 
      [
      +blank_p >> !( uint_p[assign_a(num_y)][var(y) = var(num_y)*3-3] >> +blank_p >> +alnum_p >> +blank_p )[&lex::echo::val_print]
               >> (ch_p('X') ^ 'Y' ^ 'Z')[increment_a(y)][var(x) = var(num_x)*3-3]
               >> (repeat_p(3,6)[ !blank_p >> strict_real_p[increment_a(x)][ins_elem(var(mtx),var(x)-1,var(y)-1, arg1)] ])
               >> eol_p
      ]
    )
    // ending block
    >> +soms;
 parse_info<lex::iterator> info;
 //BOOST_SPIRIT_DEBUG_NODE( common );    
 try {
   info = parse(first,last, common);
 } catch (parser_error_base &e) {
      cout << "Parse error. Check file format (GAMESS OUTPUT, RUNTYP=HESSIAN).\n";
 }
 cout << format("%1$4d %2$4d %3$4d %4$4d\n") % x % y % num_x % num_y; 
 cout << "Current hesse matrix is: \n";

 if (!info.full) {
	    cout << format("\
Error in parsing file '%1%' catched:  \n\
---------------------------------\n") % fname;
        lex::iterator err_it(first);
        std::advance(err_it, info.length);
	std::copy(first, err_it,
                 std::ostream_iterator<lex::iterator::value_type>(cout)
                );
        cout << "*\n\
---------------------------------\n\
* - the point of error\n";
       t_input_params pars0;
       PARAM_ADD(pars0, "error", "Parsing error");
       throw t_exception("GAMESS output parsing error!", pars0);
 }
 
 // symmetry matrix ;))
 for (int i=0; i<mtx.size1(); ++i)
  for (int j=0; j<i; ++j)
   mtx(i,j) = mtx(j,i);
 //
 ostringstream os;
 os << "Matrix hessian from GAMESS-file:\n";
 for (int i=0; i<mtx.size1(); ++i)
   os << ublas::row(mtx,i) << endl; 
 runtime.log_write(os.str());
 return;
}

// loading topology parameters from lack-file
extern void load_lack(t_topology &tp, const char *fname) throw (t_exception) {
   {
  runtime.log_write(string("Trying to read lack-file into topology from '")+fname+"'.\n");
  fstream inf(fname, ios::in);
  if (!inf.is_open()) { 
    runtime.log_write("Fail to open file for read.\n");
    BOOST_CHECK(0);
  }
  inf.close(); 
   }
  lex::iterator first(fname);
  lex::iterator last = first.make_end();
  lex::rule_r clrs, coms, define, common;
  t_top_coord _tc;
  
  coms = *blank_p >> ch_p(';') >> (*(anychar_p - eol_p))[&lex::echo::val_print] >> eol_p;
  clrs = *blank_p >> eol_p;
  //define string
  define  = *blank_p >> str_p("#define") >> +blank_p 
                     >> (+(alnum_p | '_'))[assign_a(_tc.defname)][&lex::echo::val_print] >> +blank_p
                     >> uint_p[assign_a(_tc.f,-1)] >> +blank_p 
                     >> real_p[assign_a(_tc.c0)] >> +blank_p 
                     >> real_p[assign_a(_tc.c1)] >> +blank_p 
                     >> !(real_p[assign_a(_tc.c2)] >> +blank_p 
                       >>   !( real_p[assign_a(_tc.c3)] >> +blank_p 
                            >> real_p[assign_a(_tc.c4)] >> +blank_p 
                            >> real_p[assign_a(_tc.c5)] >> +blank_p 
                           )
                         ) >> ch_p(';') >> uint_p[lex::assign_cast_a(_tc.type)] >> *blank_p
                  >> eol_p[lex::insert_mi_a(tp.parameters,_tc)];
 common = +(clrs ^ coms ^ define);

 parse_info<lex::iterator> info;
 
 BOOST_SPIRIT_DEBUG_NODE( common );    
 try {
   info = parse(first,last, common);
 } catch (parser_error_base &e) {
      cout << "Parse error. Check file format.\n";
 }
 cout << format("\
%1$4d define-parameters found\n") % tp.parameters.size() << endl;
 if (!info.full) {
	    cout << format("\
Error in parsing file '%1%' catched:  \n\
---------------------------------\n") % fname;
        lex::iterator err_it(first);
        std::advance(err_it, info.length);
	std::copy(first, err_it,
                 std::ostream_iterator<lex::iterator::value_type>(cout)
                );
        cout << "*\n\
---------------------------------\n\
* - the point of error\n";
        cout << "Parsed parameters: " << endl;
        cout << lex::outs.str() << endl;
       t_input_params pars0;
       PARAM_ADD(pars0, "error", "Parsing error");
       throw t_exception("Topology lack parsing error!", pars0);
 }
 return;

} // end load_lack



void load_topology(t_topology &tp, const char *fname) throw (t_exception) {
  // test if file exists
  {
  runtime.log_write(string("Trying to read topology from '")+fname+"'.\n");
  fstream inf(fname, ios::in);
  if (!inf.is_open()) { 
    runtime.log_write("Fail to open file for read.\n");
    BOOST_CHECK(0);
  }
  inf.close();
  }
  // header
  lex::iterator first(fname);
  lex::iterator last = first.make_end();
  lex::rule_r clrs, coms, define, atom, bond, angl, dihe, header, 
    pair, atoms, moltype, bonds, angles, dihedrals, pairs, common;
  t_top_coord _tc;
  t_atom _at;
  t_top_element _te;
  
  coms = *blank_p >> ch_p(';') >> (*(anychar_p - eol_p))[&lex::echo::val_print] >> eol_p;
  clrs = *blank_p >> eol_p;
  //define string
  define  = *blank_p >> str_p("#define") >> +blank_p 
                     >> (+(alnum_p | '_'))[assign_a(_tc.defname)][&lex::echo::val_print] >> +blank_p
                     >> uint_p[assign_a(_tc.f)] >> +blank_p 
                     >> real_p[assign_a(_tc.c0)] >> +blank_p 
                     >> real_p[assign_a(_tc.c1)] >> +blank_p 
                     >> !(real_p[assign_a(_tc.c2)] >> +blank_p 
                       >>   !( real_p[assign_a(_tc.c3)] >> +blank_p 
                            >> real_p[assign_a(_tc.c4)] >> +blank_p 
                            >> real_p[assign_a(_tc.c5)] >> +blank_p 
                           )
                         ) >> ch_p(';') >> uint_p[lex::assign_cast_a(_tc.type)] >> *blank_p
                  >> eol_p[lex::insert_mi_a(tp.parameters,_tc)];
  // atom string
  // if atoms already loaded from structure file
  if (!tp.atoms.size())
  atom = *blank_p >> uint_p[assign_a(_at.index)] >> +blank_p // atom index
                  >> (+(alnum_p | '_' | '='))[assign_a(_at.atom_type)] >> +blank_p // atom type OPLS
                  >> uint_p >> +blank_p // chain
                  >> (+alnum_p) >> +blank_p // residue
                  >> (+alnum_p)[assign_a(_at.atom_name)] >> +blank_p
                  >> uint_p[assign_a(_at.c_gnr)] >> +blank_p 
                  >> real_p[assign_a(_at.charge)] >> +blank_p
                  >> real_p[assign_a(_at.mass)] >> +blank_p
                  >> ch_p(';') >> (+(anychar_p - eol_p))[assign_a(_at.comment)] 
                >> eol_p[lex::insert_mi_a(tp.atoms,_at)];
  else // if no atoms present in topology now
  atom = *blank_p >> uint_p[assign_a(_at.index)] >> +blank_p // atom index
                  >> (+(alnum_p | '_' | '='))[assign_a(_at.atom_type)] >> +blank_p // atom type OPLS
                  >> uint_p >> +blank_p // chain
                  >> (+alnum_p) >> +blank_p // residue
                  >> (+alnum_p)[assign_a(_at.atom_name)] >> +blank_p
                  >> uint_p[assign_a(_at.c_gnr)] >> +blank_p 
                  >> real_p[assign_a(_at.charge)] >> +blank_p
                  >> real_p[assign_a(_at.mass)] >> +blank_p
                  >> ch_p(';') >> (+(anychar_p - eol_p))[assign_a(_at.comment)] 
                >> eol_p[lex::update_mi_a(tp.atoms,_at)];
  // bond string
  bond = *blank_p >> uint_p[assign_a(_te.i)] >> +blank_p
                  >> uint_p[assign_a(_te.j)] >> +blank_p
                  >> (+(alnum_p | '_'))[assign_a(_te.defname)] >> *blank_p
              >> eol_p[lex::insert_mi_a(tp.elements.get<1>(), _te)];
  // angle string
  angl = *blank_p >> uint_p[assign_a(_te.i)] >> +blank_p
                  >> uint_p[assign_a(_te.j)] >> +blank_p
                  >> uint_p[assign_a(_te.k)] >> +blank_p
                  >> (+(alnum_p | '_'))[assign_a(_te.defname)] >> *blank_p
              >> eol_p[lex::insert_mi_a(tp.elements.get<1>(), _te)];
  // dihedral string
  dihe = *blank_p >> uint_p[assign_a(_te.i)] >> +blank_p
                  >> uint_p[assign_a(_te.j)] >> +blank_p
                  >> uint_p[assign_a(_te.k)] >> +blank_p
                  >> uint_p[assign_a(_te.l)] >> +blank_p
                  >> (+(alnum_p | '_'))[assign_a(_te.defname)] >> *blank_p
              >> eol_p[lex::insert_mi_a(tp.elements.get<1>(), _te)];
  pair = *blank_p >> uint_p[assign_a(_te.i)] >> +blank_p
                  >> uint_p[assign_a(_te.j)] >> +blank_p
                  >> ch_p('1')[assign_a(_te.defname,"ONE_PAIR")] >> *blank_p
                  >> eol_p[lex::insert_mi_a(tp.elements.get<1>(), _te)];
  // moleculetype block
  moltype = *blank_p >> ch_p('[') >> *blank_p >> str_p("moleculetype") >> *blank_p >> ch_p(']') >> *blank_p >> eol_p
    >> *blank_p >> (+alnum_p)[assign_a(tp.res_name)] >> +blank_p >> uint_p[assign_a(tp.nrexcl)] >> *blank_p >> eol_p;
  atoms =  *blank_p >> ch_p('[') >> *blank_p >> str_p("atoms") >> *blank_p >> ch_p(']') >> *blank_p >> eol_p
        >> +atom;
  bonds =  *blank_p >> ch_p('[') >> *blank_p >> str_p("bonds") >> *blank_p >> ch_p(']') >> *blank_p >> eol_p
    >> +bond;
  angles = *blank_p >> ch_p('[') >> *blank_p >> str_p("angles") >> *blank_p >> ch_p(']') >> *blank_p >> eol_p
    >> +angl;
  dihedrals =  *blank_p >> ch_p('[') >> *blank_p >> str_p("dihedrals") >> *blank_p >> ch_p(']') >> *blank_p >> eol_p
    >> +dihe;
  pairs =  *blank_p >> ch_p('[') >> *blank_p >> str_p("pairs") >> *blank_p >> ch_p(']') >> *blank_p >> eol_p
    >> +pair;
  header = *blank_p >> str_p("#include") >> +blank_p >> ch_p('<') 
    >> (+(anychar_p - '>' - eol_p))[assign_a(tp.ffinclude)][&lex::echo::val_print]  
    >> '>' >> *blank_p >> eol_p;
 common = *(clrs ^ coms)
        >> +header 
        >> *(clrs ^ coms)
        >> moltype
        >> *(clrs ^ coms)
        >> *(define)
        >> *(clrs ^ coms) 
        >> atoms
        >> *(clrs ^ coms)
        >> !bonds
        >> *(clrs ^ coms)
        >> !angles
        >> *(clrs ^ coms)
        >> !dihedrals
        >> *(clrs ^ coms)
        >> !(pairs
              [assign_a(_tc.type, TPP_TTYPE_PAIR)]
              [assign_a(_tc.defname, "ONE_PAIR")]
              [assign_a(_tc.f, 1)]
              [lex::insert_mi_a(tp.parameters, _tc)]
             )
        >> *(clrs ^ coms);

 parse_info<lex::iterator> info;
 
 BOOST_SPIRIT_DEBUG_NODE( common );    
 try {
   info = parse(first,last, common);
 } catch (parser_error_base &e) {
      cout << "Parse error. Check file format.\n";
 }
 cout << format("\
%1$4d define-parameters found\n\
%2$4d topology elements found\n\
%3$4d atoms found\n") % tp.parameters.size() % tp.elements.size() % tp.atoms.size() << endl;
 if (!info.full) {
	    cout << format("\
Error in parsing file '%1%' catched:  \n\
---------------------------------\n") % fname;
        lex::iterator err_it(first);
        std::advance(err_it, info.length);
	std::copy(first, err_it,
                 std::ostream_iterator<lex::iterator::value_type>(cout)
                );
        cout << "*\n\
---------------------------------\n\
* - the point of error\n";
        cout << "Parsed parameters: " << endl;
        cout << lex::outs.str() << endl;
       t_input_params pars0;
       PARAM_ADD(pars0, "error", "Parsing error");
       throw t_exception("Topology parsing error!", pars0);
 }
 {
 ostringstream os;
 os << "Bonds readed: " << tp.parameters.get<1>().count(TPP_TTYPE_BON) << endl;
 os << "Angles readed: " << tp.parameters.get<1>().count(TPP_TTYPE_ANG) << endl; 
 os << "RB dihedrals readed: " << tp.parameters.get<1>().count(TPP_TTYPE_RBDIH) << endl; 
 os << "Pairs readed: " << tp.parameters.get<1>().count(TPP_TTYPE_PAIR) << endl;
 runtime.log_write(os.str());
 }
 return;
}

void check_topology(t_topology &tp) throw (t_exception) {

  // header

  ;
}

/*
 * LOADING STRUCTURE FROM DIFFERENT FILE FORMATS
 * */
void load_struct(t_topology &tp, t_iformat ifm, const char *fname) throw (t_exception) {
 try {
  // test if file exists
  runtime.log_write(string("Trying to read structure from '")+fname+"'.\n");
  fstream inf(fname, ios::in);
  if (!inf.is_open()) { 
    BOOST_CHECK(0);
    runtime.log_write("Fail to open file for read.\n");
    t_input_params params;
    PARAM_ADD(params, "procname", "tpp::load_struct");
    PARAM_ADD(params, "error", "invalid filename");
    PARAM_ADD(params, "filename", fname);
    throw t_exception("Can't open specified file for read.", params);
  }
  // test if tp variable contains structure
  if (!tp.atoms.empty()) {
    runtime.log_write("Replacing your current structure.\n");
    tp.atoms.clear();
  }

// Reading molecule in OpenBabel format
// -----------------------------------
  OBConversion conv(&inf);
  switch (ifm) {
    case TPP_IF_PDB: conv.SetInFormat("PDB"); break;
    case TPP_IF_GRO: conv.SetInFormat("GRO"); break;
    case TPP_IF_G96: conv.SetInFormat("G96"); break;
    case TPP_IF_GAMOPT: ;
    case TPP_IF_GAMHESS: ;
    case TPP_IF_GAMSP: conv.SetInFormat("GAMOUT"); break;
    default: BOOST_FAIL(0);
  }
  // reading from file with OpenBabel function
  OBMol mol;
  if ( (!conv.Read(&mol)) || (!mol.NumAtoms()) ) {
               t_input_params params;
               PARAM_ADD(params, "procname", "tpp::load_struct");
               PARAM_ADD(params, "error", "OpenBabel: parsing error");
               throw t_exception("Can't read file format.", params);
  }

  tp.mol = mol;
  inf.close();
 
// Reading molecule in t_atom_array format
// ---------------------------------------
  switch(ifm) {
    case TPP_IF_PDB: {
     inf.clear();
     inf.open(fname, ios::in);
     BOOST_CHECK(inf.is_open());
     char *s0  = new char[300], *SEL = new char[7], *NAM = new char[5], *RES = new char[5], *qat = new char[2]; 
     int  res, strc;
     std::pair<t_atom_array::iterator, bool> at_it;
     t_atom cur0;
     float __x,__y,__z;
     strc = 0;
     while (! inf.eof() ) {
         inf.getline(s0, 300);
         strc++;
//         cerr << strc << "|";

         res = 0;
         res += sscanf(s0, "%6s", SEL);
//         cerr << SEL << ".";
         if ( strcmp(SEL, "ATOM") && strcmp(SEL, "HETATM") ) continue;

         res += sscanf(s0 + 6, "%5u", &(cur0.index));
//         cur0.atom_id = lexical_cast<unsigned>(strcpy(s0, 
         res += sscanf(s0 + 11, "%5s",  NAM);
         res += sscanf(s0 + 16, "%4s",  RES);
         // pass chain letter 21 -> 23
         res += sscanf(s0 + 23,"%4u", &(cur0.mol_id) );
         res += sscanf(s0 + 30, "%f", &__x );
         res += sscanf(s0 + 38, "%f", &__y );
         res += sscanf(s0 + 46, "%f", &__z );

         strcpy(qat, "un");
         if (strlen(s0) > 76)
           res += sscanf(s0 + 76, "%2s", qat);

         if ( res < 8 ) {
               BOOST_CHECK(0);
               t_input_params params;
               PARAM_ADD(params, "procname", "tpp::load_struct");
               PARAM_ADD(params, "error", "PDB parsing error");
               PARAM_ADD(params, "line", lexical_cast<string>(strc).c_str());
               throw t_exception("Invalid ATOM string in your PDB file.", params);
         }
         
//         cerr << strc << ")";
         cur0.atom_name = string(NAM);
         cur0.res_name  = string(RES);
         cur0.coord(0)  = numeric_cast<double>(__x);
         cur0.coord(1)  = numeric_cast<double>(__y);
         cur0.coord(2)  = numeric_cast<double>(__z);
         cur0.comment   = string("QMname: ") + qat;

         runtime.log_write( (format("%s - %s:%d [%8.3f,%8.3f,%8.3f] %s \n") % cur0.res_name % cur0.atom_name % cur0.index % 
               cur0.coord(0) % cur0.coord(1) % cur0.coord(2) % cur0.comment).str() );
          at_it = tp.atoms.insert( cur0 );
//         cerr << strc << "@";
         if (! at_it.second) {
               runtime.log_write("ERROR: bad inserting..\n");
               t_input_params params;
               PARAM_ADD(params, "procname", "tpp::load_struct");
               PARAM_ADD(params, "error", "PDB parsing error");
               PARAM_ADD(params, "line", lexical_cast<string>(strc).c_str());
               throw t_exception("Repeat index or something else.", params);
         }
//         cerr << strc << "$";
         
     } // end-while
     delete[] s0; delete[] SEL; delete[] NAM; delete[] RES;
     /* FINISH PARSIGN PDB */
     inf.close();
      } // end-case PDB
      break;
    case TPP_IF_G96:   mol_to_atoms(tp); // #TODO 1 
      break;
    case TPP_IF_GRO:   mol_to_atoms(tp); // #TODO 2 
      break;
    case TPP_IF_GAMOPT: ;
    case TPP_IF_GAMHESS: ;
    case TPP_IF_GAMSP:   mol_to_atoms(tp); // #TODO 3
      break;
    default: BOOST_FAIL(0);
      break;
  };

  // setting up residue name
  if ( strutil::trim(tp.atoms.begin()->res_name) != "") {
    tp.res_name = tp.atoms.begin()->res_name;
    // checking res_name correctness    
    for (int i=0; i<tp.res_name.size(); ++i) {
      if (!isalnum(tp.res_name[i])) {
        runtime.log_write("Incorrect residue name in PDB file, using 'RES' instead.\n");
        tp.res_name = "RES";
        for (t_atom_array::iterator it = tp.atoms.begin(); it != tp.atoms.end(); ++it) {
          t_atom at(*it);
          at.res_name = "RES";
          tp.atoms.replace(it, at);
        }
      }
    }
  } else {
    tp.res_name = "VMO";
  }

     if ( tp.atoms.empty() ) {
               t_input_params params;
               PARAM_ADD(params, "procname", "tpp::load_struct");
               PARAM_ADD(params, "error", "no atoms");
               throw t_exception("No atoms in structure file or bad format.", params);
     } else {
       runtime.log_write(string("Successfully readed ")+lexical_cast<string>(tp.atoms.size())+ " atoms!\n");
     }
  } catch(t_exception e) { e.fix_log(); throw e;  }
}

 void save_struct(t_topology &tp, t_oformat ofm, const char *fname) throw (t_exception) {
  try {
  // test if file exists
  runtime.log_write(string("Trying to write structure into '")+fname+"'.\n");
  fstream out(fname, ios::out);
  if (!out.is_open()) { 
    runtime.log_write("Fail to open file for write.\n");
    BOOST_CHECK(0);
    t_input_params params;
    PARAM_ADD(params, "procname", "tpp::load_struct");
    PARAM_ADD(params, "error", "invalid filename");
    PARAM_ADD(params, "filename", fname);
    throw t_exception("Can't open specified file for write.", params);
  }
  switch (ofm) {
    case TPP_OF_PDB:
      out << format("TITLE  Written by TPP: %1$s (topoplogy for residue %2$-4s)\n") % tp.name % tp.res_name;
      for (t_atom_array::iterator it = tp.atoms.begin(); it != tp.atoms.end(); ++it) {
        out << format("%1$-6s%2$5d %3$4s%4$4s %5$c%6$4d    %7$8.3f%8$8.3f%9$8.3f\n")
          % "ATOM" % (int)it->index % it->atom_name % tp.res_name % 'A' 
          % (int)it->mol_id % it->coord(0) % it->coord(1) % it->coord(2);
      }
      out << "END\n";
      break;
    case TPP_OF_GRO: /* #TODO 4*/
/*       os << format("%1$5d%2$-3s    %3$-3s%4$5d%5$8.3f%6$8.3f%7$8.3f\n");*/
      break;
    case TPP_OF_G96: /* #TODO 5 */
/*       os << format("%1$5d %2$-4s  %3$-4s %4$7d%5$15.9f%6$15.9f%7$15.9f\n")*/
      break;
    case TPP_OF_GAMIN:
      out << format("\n\
 $DATA\n\
 %1$s\n\
 C1\n") % tp.name;
      for (t_atom_array::iterator it = tp.atoms.begin(); it != tp.atoms.end(); ++it) {
        out << format("%1$-4s %2$2.1f  %3$15.10f %4$15.10f %5$15.10f\n")
          % it->atom_name % numeric_cast<float>(it->ncharge) % it->coord(0) % it->coord(1) % it->coord(2);
      }
      out << " $END\n";
      break;
  };
  out.close();
  // header
  } catch (t_exception e) {
     e.fix_log();
     throw e;
  }

}

// also serialization should be included

} // end namespace

