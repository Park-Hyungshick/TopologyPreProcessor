#include "pdbutils.hpp"
#include "runtime.hpp"

#include <openbabel/obiter.h>
#include <openbabel/atom.h>
#include <openbabel/mol.h>

#include <set>
#include <sstream>
#include <algorithm>

#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/numeric/ublas/vector_proxy.hpp>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

using std::string;
using std::ostringstream;
using std::cout;
using std::endl;

using namespace boost::numeric;
using boost::format;
using boost::lexical_cast;

namespace tpp {
using OpenBabel::OBAtom;
using OpenBabel::OBMol;
using OpenBabel::OBMolBondIter;
using OpenBabel::OBMolAtomIter;
using OpenBabel::OBAtomAtomIter;
using namespace boost::numeric::ublas;

string an2str(int a, string def) {
	switch (a) {
	case 1:
		return string("H");
	case 6:
		return string("C");
	case 7:
		return string("N");
	case 8:
		return string("O");
	case 15:
		return string("P");
	case 16:
		return string("S");
	case 17:
		return string("Cl");
	case 78:
		return string("Pt");
	default:
		return def;
	}
	return string("-");
}

// main chain private numbering function
string mc_numerer(int id) {
	char *rrr = new char[4];
	if ((id > 255)
			|| ( (cmdline.read("hex_flag") == "off") && (id > 99))) {
		Parameters params;
		params.add("procname", "tpp::mc_numerer");
		params.add("error",
				string("Too many atoms to number. Try to turn HEX mode. "));
		throw Exception("Bad connection atoms", params);
	}
	if (cmdline.read("hex_flag") == "on") {
		sprintf(rrr, "%X", id);
	} else {
		sprintf(rrr, "%d", id);
	}
	return string(rrr);
}

BondMatrix::BondMatrix(const OBMol &src) {
	int dim = src.NumAtoms();
	mtx = zero_matrix<bool>(dim + 1, dim + 1);
	FOR_BONDS_OF_MOL(it,const_cast<OBMol&>(src)){
	mtx(it->GetBeginAtom()->GetIdx(), it->GetEndAtom()->GetIdx()) = true;
	mtx(it->GetEndAtom()->GetIdx(), it->GetBeginAtom()->GetIdx()) = true;
}
	check();
}

void BondMatrix::print(std::ostream &os) {
	; // echo function
}

void BondMatrix::rec_check(int _cur) {
	for (int i = 1; i < mtx.size1(); ++i) {
		if (mtx(i, _cur) && !(curset.count(i))) {
			curset.insert(i);
			rec_check(i);
		}
	}
}

bool BondMatrix::check() {
	int cur = 1;
	curset.clear();
	curset.insert(1);
	rec_check(1);
	if (curset.size() != mtx.size1() - 1) {
		ostringstream os;
		os
				<< format(
						"Connected group contains %d atoms. Molecule contains %d atoms.")
						% curset.size() % (mtx.size1() - 1) << endl;
		for (std::set<int>::iterator it = curset.begin(); it != curset.end();
				++it)
			os << (*it) << ",";
		os << " - This atoms are isolated" << endl;
		Parameters params;
		params.add("procname", "tpp::BondMatrix::check");
		params.add("error", os.str());
		throw Exception("Bad connection atoms", params);
	}
	return true;
}

// temporary function (not needed later)
template<typename T>
void resize_matrix(T &mat, int x, int y) {
	matrix<int> tmpmt(zero_matrix<int>(x, y));
	subrange(tmpmt, 0, mat.size1(), 0, mat.size2()) = mat;
	mat = tmpmt;
}

void recurse_mol_scan(OBMol &mol, std::vector<unsigned> &tail, unsigned cur,
		std::vector<unsigned> &maxtail, std::set<unsigned> &excluded) {
	int id;
	FOR_NBORS_OF_ATOM(pA, &*(mol.GetAtom(cur)) ){
	id = pA->GetIdx();
	if ( (pA->GetAtomicNum() == 1) ||
			(std::count(tail.begin(), tail.end(), id)) ||
			(std::count(excluded.begin(), excluded.end(), id))
	) continue;
	tail.push_back(pA->GetIdx());
	recurse_mol_scan(mol, tail, id, maxtail, excluded);
}
// TODO: prefer tail which contains more carbon atoms
#ifdef CDB
cout << "CDB:RMS: " << std::flush;
cout << tail;
cout << " --]] " << std::endl;
#endif
	if (tail.size() > maxtail.size()) {
		maxtail.clear();
		maxtail.insert(maxtail.begin(), tail.begin(), tail.end());
	}
	tail.pop_back();
}

/*
 * GENERATE LONGTAIL COMCON1 ALGORYTHM
 */
ublas::vector<int> generate_long_tail1(OBMol &mol, std::set<unsigned> &excluded,
		int startpoint) {
	std::vector<unsigned> maxtail;
	std::vector<unsigned> curtail;
	int id = 0;
	// TODO: i do not know it BM is really need here??
	BondMatrix bm(mol);

	runtime.log_write("Starting c1/openbabel longtail alrogithm.\n");

	if (startpoint == -1) {
		// start recursion from every heavy atom
		FOR_ATOMS_OF_MOL(pA, mol){ // p
		if (pA->GetAtomicNum() == 1) continue;
		id = pA->GetIdx();
		curtail.clear();
		curtail.push_back(id);
		recurse_mol_scan(mol, curtail, id, maxtail, excluded);
	} // pA cycle over molecule

} else {
	recurse_mol_scan(mol, curtail, startpoint, maxtail, excluded);
}

// generate a vector of longtail
	ublas::vector<unsigned> tail_long(maxtail.size() + 1);
	for (int i = 1; i <= maxtail.size(); ++i)
		tail_long(i) = maxtail[i - 1];

	// output section
	runtime.log_write("Longest subchain is: \n");
	{
		std::ostringstream os;
		os << subrange(tail_long, 1, tail_long.size()) << std::flush;
		runtime.log_write(os.str());
		if ((cmdline.read("verbose_flag") == "on")
				&& (tail_long.size() > 1)) {
			cout << "At. subchain: " << os.str() << endl;
		}
	}
	runtime.log_write("\nlongtail algroithm finished its work.\n");
	return tail_long;
}

// wrapper to simplier exported variant
ublas::vector<int> generate_long_tail1(OBMol &mol) {
	std::set<unsigned> emptyset;
	return generate_long_tail1(mol, emptyset, -1);
}

/*
 * GENERATE LONGTAIL ZOIDBERG ALGORYTHM
 */
ublas::vector<int> generate_long_tail(OBMol &mol) {
	/*#define resize_matrix(mat,x,y)\
  { tmpmt = zero_matrix<int>(x,y);\
    subrange(tmpmt,0,mat.size1(),0,mat.size2()) = mat;\
    mat = tmpmt;\
  }*/
	runtime.log_write("Starting Zoidberg longtail alrogithm.\n");
	BondMatrix bm(mol);
	matrix<int> tail(1, 1), tmpmt(1, 1);
	tail *= 0;
	ublas::vector<int> rem(1);
	ublas::vector<int> S(2), Sl(2);
	ublas::vector<int> tail_long(1);
	{
		std::ostringstream os;
		runtime.log_write("Bounded matrix:\n");
		for (int i = 1; i < (*bm).size1(); i++)
			os << subrange(row((*bm), i), 1, (*bm).size2()) << endl;
		runtime.log_write(os.str());
	}
	Sl(0) = 1;
	Sl(1) = 1;
	for (int p = 1; p < (*bm).size1(); p++) {
		int n = 0;
		for (int q = 1; q < (*bm).size2(); q++) {
			if ((*bm)(p, q)) {
				n++;
				resize_matrix(tail, n + 1, 3);
				tail(n, 1) = p;
				tail(n, 2) = q;
			}
		}
		/* debug */
		/*   cout << "Tail of version 1.0" << endl;
		 cout << subrange(tail,1,tail.size1(),1,tail.size2() ) << endl; */
		/* debug */
		int neig = n;
		int stop = 0;
		n = 2;
		while (!stop) {
			n++;
			int m = 0;
			int ins = 0, q;
			for (q = 1; q <= neig; q++) {
				for (int r = 1; r < (*bm).size1(); r++) {
					BOOST_CHECK(n - 1 < tail.size2());
					if ((q + ins < tail.size1())
							&& (*bm)(tail(q + ins, n - 1), r)
							&& !std::count(row(tail, q + ins).begin(),
									row(tail, q + ins).end(), r)) {
						m++;
						if (m != 1) {
							if (neig + 2 > tail.size1())
								resize_matrix(tail, neig + 2, tail.size2());
							if ((q + m < tail.size1()) && (q + m < neig + 2)) // fixed??
								subrange(tail, q + m, neig + 2, 0, tail.size2()) =
										subrange(tail, q + m - 1, neig + 1, 0,
												tail.size2());
							if (q + m - 1 < tail.size1())
								row(tail, q + m - 1) = row(tail, q + m - 2);
							neig++;
							ins++;
						}
						if (n >= tail.size2())
							resize_matrix(tail, tail.size1(), n + 1);
						if (q + m - 1 >= tail.size1())
							resize_matrix(tail, q + m, tail.size2());
						tail(q + m - 1, n) = r;
						/*           {std::ostringstream os;
						 runtime.log_write("Tail added to list\n");
						 os << subrange(tail,1,tail.size1(),1,tail.size2() ) << endl;
						 runtime.log_write(os.str());} */
					}
				} // endfor
				if (!m && (q == neig)) {
					stop = 1;
				}
			} //endfor
			if (!stop) {
				int s = 0;
				rem(0) = 0;
				for (int r = 1; (r < q + m) && (r < tail.size1()); r++) {
					if (!tail(r, n) && m) {
						s++;
						rem.resize(s + 1);
						rem(s) = r;
					} //endif
				} //endfor r
				for (int r = 0; r < s; r++) {
					subrange(tail, rem(s - r), tail.size1() - 1, 0,
							tail.size2()) = subrange(tail, rem(s - r) + 1,
							tail.size1(), 0, tail.size2());
					tail.resize(tail.size1() - 1, tail.size2());
				}
			} //endif not stop
			rem.clear();
			rem.resize(1);
			/*   runtime.log_write("Tail after removing:\n");
			 {std::ostringstream os;
			 os << subrange(tail, 1, tail.size1(), 1, tail.size2() ) << endl;
			 runtime.log_write(os.str());}*/
			/* ------------------- */
			S(0) = tail.size1();
			S(1) = tail.size2();
			neig = S(1);

		} //endwhile
		if (S(1) > Sl(1)) {
			tail_long.resize(tail.size2());
			tail_long = row(tail, 1);
			/*     {std::ostringstream os;
			 os << "Longest tail now is:" << endl;
			 os << subrange(tail_long, 1, tail_long.size()) << endl;
			 runtime.log_write(os.str());}*/
			Sl(0) = tail.size1();
			Sl(1) = tail.size2();
		}
		tail.clear();
		tail.resize(1, 1);
		S.clear();
		S.resize(2);
//   cout << S << endl;
		n = 0;
		neig = 0;
		stop = 0;
	}  //endmacrofor
	   // removing forward and back hydrogens
	if (mol.GetAtom(tail_long(tail_long.size() - 1))->GetAtomicNum() == 1)
		tail_long.resize(tail_long.size() - 1);
	if (mol.GetAtom(tail_long(1))->GetAtomicNum() == 1) {
		subrange(tail_long, 0, tail_long.size() - 1) = subrange(tail_long, 1,
				tail_long.size());
		tail_long.resize(tail_long.size() - 1);
	}
	runtime.log_write("The longest tail is: \n");
	{
		std::ostringstream os;
		os << subrange(tail_long, 1, tail_long.size()) << endl;
		runtime.log_write(os.str());
	}
	runtime.log_write("longtail algroithm finished its work.\n");
	return tail_long;
}

std::pair<matrix<int>, matrix<int> > top_neig(OBMol &X, matrix<int> M, int a,
		int b, int c, int flag) {
	matrix<int> tmpmt(1, 1);
	matrix<int> neig(flag + 1, 2);
	matrix<int> funct = zero_matrix<int>(flag + 1, 7);
	/*--------------------------------
	 Numbers of neighbours serching
	 --------------------------------*/
	int n = 0, m = 0, k = 0, l = 0, h = 0;
	switch (flag) {
	case 1:
		FOR_NBORS_OF_ATOM(pQ, &*(X.GetAtom(a)) ){
		int id = pQ->GetIdx();
		if ( (id == b) || (id == c) ) continue;
		n++;
		if ( n >= neig.size2() )
		resize_matrix(neig, neig.size1(), n+1);
		neig(1,n) = id;
	}
	break;
	case 2:
	FOR_NBORS_OF_ATOM(pQ, &*(X.GetAtom(a)) ) {
		int id = pQ->GetIdx();
		if ( (id == b) || (id == c) ) continue;
		n++;
		if ( n >= neig.size2() )
		resize_matrix(neig, neig.size1(), n+1);
		neig(1,n) = id;
		FOR_NBORS_OF_ATOM(pR, &*pQ) {
			int id1 = pR->GetIdx();
			if (id1 == a) continue;
			m++;
			if ( m >= neig.size2() )
			resize_matrix(neig, neig.size1(), m+1);
			neig(2,m) = id1;
		}
	}
	break;
	case 3:
	for (int p=1; p < M.size2(); p++) {
		if ( M(a,p) && (p != b) & (p != c) ) {
			n++;
			if ( n >= neig.size2() )
			resize_matrix(neig, neig.size1(), n+1);
			neig(1,n) = p;
			for (int q=1; q < M.size2(); q++) {
				if ( M(p,q) && (q != a) ) {
					m++;
					if ( m >= neig.size2() )
					resize_matrix(neig, neig.size1(), m+1);
					neig(2,m) = q;
					for (int r=1; r < M.size2(); r++) {
						if ( M(q,r) && (r != p) ) {
							k++;
							if ( k >= neig.size2() )
							resize_matrix(neig, neig.size1(), k+1);
							if ( 3 >= neig.size1() )
							resize_matrix(neig, 4, neig.size2());
							neig(3,k) = r;
						}
					}
				}
			}
		}
	}
	break;
	/*
	 COMMENTED 4 and 5 neighbour analysing
	 */
	default:
	BOOST_CHECK(0);
} // end switch
/*
 *-----------------------------------------------------------
 * Creation of node typoe matrix. In format H C O N P S
 *-----------------------------------------------------------
 */
	ublas::vector<int> S(2);
	S(0) = neig.size1();
	S(1) = neig.size2();
	if (S(1) > 27) {
		Parameters params;
		params.add("procname", "tpp::top_neig");
		params.add("error",
				(string("Too many neighbours of atom ")
						+ lexical_cast<string>(a)).c_str());
		throw Exception("Bad connection atoms", params);
	}
	for (int p = 1; p <= flag; p++) {
		for (int q = 1; q < S(1); q++) {
			if (!neig(p, q))
				;
			else if (X.GetAtom(neig(p, q))->GetAtomicNum() == 1)
				funct(p, 1) = funct(p, 1) + 1;
			else if (X.GetAtom(neig(p, q))->GetAtomicNum() == 6)
				funct(p, 2) = funct(p, 2) + 1;
			else if (X.GetAtom(neig(p, q))->GetAtomicNum() == 8)
				funct(p, 3) = funct(p, 3) + 1;
			else if (X.GetAtom(neig(p, q))->GetAtomicNum() == 7)
				funct(p, 4) = funct(p, 4) + 1;
			else if (X.GetAtom(neig(p, q))->GetAtomicNum() == 15)
				funct(p, 5) = funct(p, 5) + 1;
			else if (X.GetAtom(neig(p, q))->GetAtomicNum() == 16)
				funct(p, 6) = funct(p, 6) + 1;
			else { /*
			 Parameters params;
			 params.add("procname", "tpp::top_neig");
			 params.add("error", (
			 string("Strange atom, algorythm can't work with atom #")+
			 lexical_cast<string>(X.GetAtom(neig(p,q))->GetAtomicNum())+ ", ID=" +
			 lexical_cast<string>(neig(p,q))
			 ).c_str() );
			 throw Exception("Your system has strange atoms", params); */
				std::ostringstream os;
				os << "WARNING: in tpp::top_neig\n";
				os
						<< format(
								"Strange atom, algorythm can't work with atom #%d ID=%d\n")
								% X.GetAtom(neig(p, q))->GetAtomicNum()
								% neig(p, q);
				runtime.log_write(os.str());
			}
		}
	}
	return std::pair<matrix<int>, matrix<int> >(funct, neig);
}

AtomArray mol_renum(OBMol &mol, AtomArray &ar, ublas::vector<int> tail) {

	int mode = 0, // turn topcreator prerun - create a file for atomtypes definition (0), or run - make a full topology file(1).
			nneig = 1,       // difines the order of neighbourhood to consider
			renum = 1,       // when is set to 1, it will rearrange atoms
			conhir = 1, // when is set to 1, all chiral centers and ammonium nitrogens will be constrained whith impropers
			remHdih = 0, // when is set to 1, all dihedrals with hydrogens will be rejected
			flood = 1, // when is set to 1, it will be very loud, and will print a lot of stuff
			exclon = 1; // whin is set to 1, it will find out close charges and move them to exclusions
	double excltol = 0.01; // tolerance barier for exlusion of pair interaction,

	runtime.log_write("Starting Zoidberg renumerator alrogithm.\n");
#ifdef TPP_UNIT_TEST
	FOR_ATOMS_OF_MOL(pt, mol) {
		cout << format(" %1$3d %2$4s %3$3d\n") %pt->GetIdx() % pt->GetType() % pt->GetAtomicNum();
	}
	cout << "================================" << endl;
#endif
	BondMatrix bm(mol);
	matrix<int> tmpmt(1, 1);

	// check molecule valence of some atoms
	runtime.log_write("Checking bond matrix of molecule..\n");
	FOR_ATOMS_OF_MOL(pt, mol){
	if (pt->GetValence() > 4) {
		runtime.log_write(string("Atom ") + lexical_cast<string>(pt->GetIdx()) + " has high valence!!\n");
		FOR_NBORS_OF_ATOM(b, &*pt ) {
			runtime.log_write(string("--Neighbour #") + lexical_cast<string>(b->GetIdx()) + "\n");
		}
	} else if ( (pt->GetValence() > 1) && (pt->GetAtomicNum() == 1) ) {
		runtime.log_write(string("Atom ") + lexical_cast<string>(pt->GetIdx()) + " is hydrogen with high valence!!\n");
		FOR_NBORS_OF_ATOM(b, &*pt ) {
			runtime.log_write(string("--Neighbour #") + lexical_cast<string>(b->GetIdx()) + "\n");
		}
	} else if ( (pt->GetValence() > 2) && (pt->GetAtomicNum() == 8) ) {
		runtime.log_write(string("Atom ") + lexical_cast<string>(pt->GetIdx()) + " is oxygen with high valence!!\n");
		FOR_NBORS_OF_ATOM(b, &*pt ) {
			runtime.log_write(string("--Neighbour #") + lexical_cast<string>(b->GetIdx()) + "\n");
		}
	}
}
	runtime.log_write("Checking bond matrix of molecule finished.\n");
	AtomArray A; // final atom array with new names and numbers
	Atom tat;
	std::set<int> count; // set of
	ublas::vector<int> schain(1);
	/* ATOM RENUMBERING SECTION */
	int n = 0; // total atom counter (new index)
	int h = 0; // hard atom counter
	for (int p = 1; p < tail.size(); ++p) { // cycle over longtail head atoms
		OBAtom *pA = mol.GetAtom(tail(p));
		tat = *(ar.find(tail(p)));
		n++;
		h++;
		tat.atom_name = an2str(pA->GetAtomicNum(), tat.atom_name)
				+ mc_numerer(h);
		tat.index = n;
		tat.coord(0) = pA->GetX();
		tat.coord(1) = pA->GetY();
		tat.coord(2) = pA->GetZ();
		tat.ncharge = pA->GetAtomicNum();
		BOOST_CHECK(A.insert(tat).second);
		BOOST_CHECK(count.insert(tail(p)).second);
		int k = 0; // hydrogen counter
		// arrange hydrogens
		FOR_NBORS_OF_ATOM(pQ, &*pA){
		if (pQ->GetAtomicNum() != 1) continue;
		BOOST_CHECK(count.insert(pQ->GetIdx()).second);
		n++;
		k++;
		tat.ncharge = pQ->GetAtomicNum();
		tat.index = n;
		tat.coord(0) = pQ->GetX();
		tat.coord(1) = pQ->GetY();
		tat.coord(2) = pQ->GetZ();
		tat.atom_name = lexical_cast<string>(k)+"H"+mc_numerer(h);
		BOOST_CHECK(A.insert(tat).second);
	}
		int stop = 0, m = 1, sh = 0;
		schain.resize(2);
		schain(1) = tail(p);
		while (!stop) {
			sh++;
			std::pair<matrix<int>, matrix<int> > fun_neig;
			fun_neig = top_neig(mol, *bm, schain(sh), 0, 0, 1);
			for (int q = 1; q < fun_neig.second.size2(); q++) {
				if (!count.count(fun_neig.second(1, q))
						&& !std::count(tail.begin(), tail.end(),
								fun_neig.second(1, q))
						&& !std::count(schain.begin(), schain.end(),
								fun_neig.second(1, q))) {
					int summ = 0;
					for (int ii = 1; ii < schain.size(); ii++)
						if (mol.GetAtom(fun_neig.second(1, q))->IsConnected(
								mol.GetAtom(schain(ii))))
							summ++;
					if ((summ == 1)
							&& (mol.GetAtom(fun_neig.second(1, q))->GetAtomicNum()
									!= 1)) {
						m++;
						if (m >= schain.size())
							schain.resize(m + 1);
						schain(m) = fun_neig.second(1, q);
					}
				}
				stop = (sh == m);
			}
		} // end while
		if (m != 1) {
			// deleting 1st element of schain
			if (schain.size() > 2)
				subrange(schain, 1, schain.size() - 1) = subrange(schain, 2,
						schain.size());
			schain.resize(schain.size() - 1);
			for (int q = 1; q < schain.size(); q++) {
				BOOST_CHECK(count.insert(schain(q)).second);
				n++;
				h++;
				tat = *(ar.find(schain(q)));
				tat.ncharge = mol.GetAtom(schain(q))->GetAtomicNum();
				tat.index = n;
				tat.coord(0) = mol.GetAtom(schain(q))->GetX();
				tat.coord(1) = mol.GetAtom(schain(q))->GetY();
				tat.coord(2) = mol.GetAtom(schain(q))->GetZ();
				tat.atom_name = an2str(mol.GetAtom(schain(q))->GetAtomicNum(),
						tat.atom_name) + mc_numerer(h);
				BOOST_CHECK(A.insert(tat).second);
				k = 0;
				FOR_NBORS_OF_ATOM(pR, &*(mol.GetAtom(schain(q))) ){
				if (pR->GetAtomicNum() == 1) {
					BOOST_CHECK(count.insert(pR->GetIdx()).second);
					n++;
					k++;
					tat.ncharge = pR->GetAtomicNum();
					tat.index = n;
					tat.coord(0) = pR->GetX();
					tat.coord(1) = pR->GetY();
					tat.coord(2) = pR->GetZ();
					tat.atom_name = lexical_cast<string>(k) + "H" + mc_numerer(h);
					BOOST_CHECK(A.insert(tat).second);
				}
			} //end for r
		} // end for q
	} // end if m != 1
		schain.resize(1);
	} // end for tail (p)

	  // creating of secondary molecule complete!
	  // ----------------------------------------
	{
		ostringstream os;
		os << "New atom names and numbers:\n";
		for (AtomArray::iterator ait = A.begin(); ait != A.end(); ++ait) {
			os
					<< format(" %1$3d %2$4s %3$3d\n") % (int) ait->index
							% ait->atom_name % (int) ait->ncharge;
		}
		os << "Table of converting numbers:\n";
		std::set<int>::iterator cit = count.begin();
		for (int ii = 1; ii < count.size(); ii++) {
			os << format("%1$3d -> %2$3d\n") % ii % *cit;
			++cit;
		}
		os << "Zoidberg renumeration alogrythm finished its work.\n";
		runtime.log_write(os.str());
	}
	return A;
} // end pdb_renum)

/* 
 *
 * COMCON1 RENUMERATION ALGORYTM 
 *
 */

// recursion part of mol_renum1
void __mol_renum1(OBMol &_mol, AtomArray &_ar, ublas::vector<int> &_tail,
		std::set<unsigned> &_tailed, AtomArray &_A, int &_n, int &_h) {

	// append <tailed> array
	for (int p = 1; p < _tail.size(); ++p)
		BOOST_CHECK(_tailed.insert(_tail(p)).second);
	// cycle over current tail
	for (int p = 1; p < _tail.size(); ++p) {
		{ // area of defining local variables
			OBAtom *pA = _mol.GetAtom(_tail(p));
			BOOST_CHECK(_ar.count(_tail(p)) == 1);
			Atom tat = *(_ar.find(_tail(p)));
			_n++;
			_h++;
			tat.atom_name = an2str(pA->GetAtomicNum(), tat.atom_name)
					+ mc_numerer(_h);
			tat.index = _n;
			tat.coord(0) = pA->GetX();
			tat.coord(1) = pA->GetY();
			tat.coord(2) = pA->GetZ();
			tat.ncharge = pA->GetAtomicNum();
			BOOST_CHECK(_A.insert(tat).second);
			int k = 0; // hydrogen local counter
			// arrange hydrogens
			FOR_NBORS_OF_ATOM(pQ, &*pA){
			if (pQ->GetAtomicNum() != 1) continue;
			//     BOOST_CHECK(count.insert(pQ->GetIdx()).second);
			_n++;
			k++;
			tat = * ( _ar.find(pQ->GetIdx()) );
			tat.ncharge = pQ->GetAtomicNum();
			tat.index = _n;
			tat.coord(0) = pQ->GetX();
			tat.coord(1) = pQ->GetY();
			tat.coord(2) = pQ->GetZ();
			tat.atom_name = lexical_cast<string>(k)+"H"+mc_numerer(_h);
			BOOST_CHECK(_A.insert(tat).second);
		}
	} // end of local variables area
	  // start LONGTAIL from the point and drawn deep into recursion
	while (true) {
		ublas::vector<int> newtail = generate_long_tail1(_mol, _tailed, _tail(p));
		if (newtail.size() == 1) break;
		__mol_renum1(_mol, _ar, newtail, _tailed, _A, _n, _h);
	}
} // end cycle over longtail

} // end of recursion function

// main function
AtomArray mol_renum1(OBMol &mol, AtomArray &ar, ublas::vector<int> tail) {

	runtime.log_write("Starting C1 renumerator alrogithm.\n");
#ifdef TPP_UNIT_TEST
	FOR_ATOMS_OF_MOL(pt, mol) {
		cout << format(" %1$3d %2$4s %3$3d\n") %pt->GetIdx() % pt->GetType() % pt->GetAtomicNum();
	}
	cout << "================================" << endl;
#endif
	BondMatrix bm(mol);
	matrix<int> tmpmt(1, 1);

	// check molecule valence of some atoms
	runtime.log_write("Checking bond matrix of molecule..\n");
	FOR_ATOMS_OF_MOL(pt, mol){
	if (pt->GetValence() > 4) {
		runtime.log_write(string("Atom ") + lexical_cast<string>(pt->GetIdx()) + " has high valence!!\n");
		FOR_NBORS_OF_ATOM(b, &*pt ) {
			runtime.log_write(string("--Neighbour #") + lexical_cast<string>(b->GetIdx()) + "\n");
		}
	} else if ( (pt->GetValence() > 1) && (pt->GetAtomicNum() == 1) ) {
		runtime.log_write(string("Atom ") + lexical_cast<string>(pt->GetIdx()) + " is hydrogen with high valence!!\n");
		FOR_NBORS_OF_ATOM(b, &*pt ) {
			runtime.log_write(string("--Neighbour #") + lexical_cast<string>(b->GetIdx()) + "\n");
		}
	} else if ( (pt->GetValence() > 2) && (pt->GetAtomicNum() == 8) ) {
		runtime.log_write(string("Atom ") + lexical_cast<string>(pt->GetIdx()) + " is oxygen with high valence!!\n");
		FOR_NBORS_OF_ATOM(b, &*pt ) {
			runtime.log_write(string("--Neighbour #") + lexical_cast<string>(b->GetIdx()) + "\n");
		}
	}
}
	runtime.log_write("Checking bond matrix of molecule finished.\n");

	/* ATOM RENUMBERING SECTION. preparing to recursion */
	AtomArray A; // final atom array with new names and numbers
	std::set<unsigned> tailed; // set of atoms ALREADY in tails
	int n = 0; // total atom counter (new index)
	int h = 0; // hard atom counter

	// STARTING RECURSION
	__mol_renum1(mol, ar, tail, tailed, A, n, h);

	{ // output block
		ostringstream os;
		os << "New atom names and numbers:\n";
		for (AtomArray::iterator ait = A.begin(); ait != A.end(); ++ait) {
			os
					<< format(" %1$3d %2$4s %3$3d\n") % (int) ait->index
							% ait->atom_name % (int) ait->ncharge;
		}
		os << "Table of converting numbers:\n";
		for (AtomArray::iterator ait = A.begin(); ait != A.end(); ++ait) {
			os << format("%1$3d -> %2$3d\n") % ait->oldindex % ait->index;
		}
		os << "Table of converting names:\n";
		for (AtomArray::iterator ait = A.begin(); ait != A.end(); ++ait) {
			os << format("%1$s -> %2$s\n") % ait->old_aname % ait->atom_name;
		}
		os << "COMCON1 renumeration alogrythm finished its work.\n";
		runtime.log_write(os.str());
	} // end. output block
	return A;
} // end mol_renum1

}
