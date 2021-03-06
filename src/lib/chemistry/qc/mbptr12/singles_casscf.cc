//
// cabs_single.cc
//

#include <mpqc_config.h>

#if defined(HAVE_MPQC3_RUNTIME)

#include <chemistry/qc/mbptr12/singles_casscf.h>
#include <TiledArray/algebra/conjgrad.h>

using namespace sc;
using namespace std;

namespace{
template<typename T>
    struct _CABS_singles {

        typedef TA::Array<T, 4> Array4;
        typedef TA::Array<T, 2> Array2;

        const Array4& Bmatrix;

        _CABS_singles(const Array4& B) : Bmatrix(B){
        }

        /**
         * @param[in] C
         * @param[out] BC
         */
        void operator()(const Array2& C, Array2& BC) {
            BC("x,B'") = Bmatrix("x,B',y,A'") * C("y,A'");
        }
    };

  /// makes a diagonal 2-index preconditioner: pc_x^y = -1/ ( <x|O1|x> - <y|O2|y> )
  template <typename T>
  struct diag_precond2 {
    typedef Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> EigenMatrixX;
    diag_precond2(const EigenMatrixX& O1_mat,
                  const EigenMatrixX& O2_mat) :
                      O1_mat_(O1_mat), O2_mat_(O2_mat) {
    }
    template <typename Index> T operator()(const Index& i) {
      return 1.0 / (- O1_mat_(i[0], i[0]) + O2_mat_(i[1], i[1]));
    }

    private:
      EigenMatrixX O1_mat_;
      EigenMatrixX O2_mat_;
  };
}

CABS_Single::CABS_Single(std::shared_ptr <SingleReference_R12Intermediates<double>> srr12intrmds) {
  srr12intrmds_ = srr12intrmds;
}

double CABS_Single::compute(const std::string &h0) {
  double cabs_singles_e = 0.0;
  if(h0 == string("dyall_1"))
    cabs_singles_e = cabs_singles_Dyall(h0);
  else if(h0 == string("dyall_2"))
    cabs_singles_e = cabs_singles_Dyall(h0);
  else if(h0 == string("fock"))
    cabs_singles_e = cabs_singles_Fock();
  else
    throw InputError("invalid value for keyword cabs_singles_h0",
                     __FILE__, __LINE__);
  return cabs_singles_e;
}

double CABS_Single::cabs_singles_Dyall(const std::string &h0)
{
  /*"Perturbative Correction for the Basis Set Incompleteness Error of CASSCF",
   * L. Kong and E.~F.~Valeev,  J. Chem. Phys. 133, 174126 (2010),
   * http://dx.doi.org/10.1063/1.3499600.
   * */
# define DEBUGG false

    ExEnv::out0() << std::endl << std::endl << indent
    << "Enter PT2R12::cabs_singles_Dyall MPQC3_RUNTIME\n";
    // set up timer
    Timer tim("cabs singles dyall");

    typedef SingleReference_R12Intermediates<double>::TArray4 TArray4;
    typedef SingleReference_R12Intermediates<double>::TArray2 TArray2;

    // density matrices
    TArray2 gamma1; gamma1("m,n") = srr12intrmds_->_2("<m|gamma|n>"); // rdm1 occ
    TArray4 gamma2; gamma2("m,n,m1,n1") = srr12intrmds_->_4("<m n|gamma|m1 n1>"); //rdm2 occ

    TArray2 I_aA; I_aA("e,B'") = srr12intrmds_->_2("<e|I|B'>"); // allvir vir

    // h_core
    TArray2 h_ij; h_ij("m,n") = srr12intrmds_->_2("<m|h|n>");  //occ occ

    // fock
    TArray2 F_AB; F_AB("A',B'") = srr12intrmds_->_2("<A'|F|B'>"); //allvir allvir

    // make B matrix
    tim.enter("compute B matrix"); // time B matrix

    TArray4 B;

    // term1
    {
      TArray4 term1;
      term1("x,B',y,A'") = F_AB("A',B'") * gamma1("x,y");
      B("x,B',y,A'") = term1("x,B',y,A'");
    }
    madness::World::get_default().gop.fence();

#if DEBUGG
    // set the cout precision to 10 decimal digits
    std::cout.setf(ios::fixed);
    std::cout.precision(10);
    // std::cout << "term1 of B: \n" << term1 << std::endl;
#endif

    // term2
    {
      TArray2 I_AB; I_AB("B',A'") = srr12intrmds_->_2("<B'|I|A'>"); //allvir allvir
      TArray4 g_ijkl; g_ijkl("n1,m1,m,n") = srr12intrmds_->_4("<n1 m1|g|m n>"); // occ
      TArray4 term2;
      term2("x,B',y,A'") = - ( gamma1("x,i") * h_ij("i,y") + gamma2("x,j,k,i") * g_ijkl("j,k,i,y") )*I_AB("A',B'");
      B("x,B',y,A'") = B("x,B',y,A'") + term2("x,B',y,A'");
    }
    madness::World::get_default().gop.fence();
#if DEBUGG
    // std::cout << "term2 of B: \n" << term2 << std::endl;
#endif

    // term3
    {
      TArray2 F_ab; F_ab("e,f") = srr12intrmds_->_2("<e|F|f>");  //vir vir
      TArray2 h_ab; h_ab("e,f") = srr12intrmds_->_2("<e|h|f>");   //vir vir
      TArray4 term3;
      term3("x,B',y,A'") = (h_ab("a,b") - F_ab("a,b"))*I_aA("b,B'")*I_aA("a,A'")*gamma1("x,y");
      B("x,B',y,A'") = B("x,B',y,A'") + term3("x,B',y,A'");
    }
    madness::World::get_default().gop.fence();

    // term4
    {
      TArray4 g_abij; g_abij("f,e,m,n") = srr12intrmds_->_4("<m f|g|n e>"); // occ vir occ vir
      TArray4 g_iajb; g_iajb("f,e,m,n") = srr12intrmds_->_4("<m e|g|f n>"); // occ vir vir occ
      TArray4 term4;
      term4("x,B',y,A'") = (g_abij("a,b,j,i")*gamma2("i,x,j,y") + g_iajb("a,b,j,i")*gamma2("i,x,y,j"))*I_aA("b,B'")*I_aA("a,A'");
      B("x,B',y,A'") = B("x,B',y,A'") + term4("x,B',y,A'");
    }
    madness::World::get_default().gop.fence();
#if DEBUGG
    std::cout << "term4 of B: \n" << term4 << std::endl;
#endif

    tim.exit();

#if DEBUGG
    std::cout << "allterm of B: \n" << B << std::endl;
    ofstream foutB("all term of B");
    foutB.setf(ios::fixed);
    foutB.precision(10);
    foutB << setprecision(10) << B << std::endl;
    foutB.close();
#endif

    //
    //  solve the linear algebra problem a(x)=b in Equation (15)
    //

    //compute b matrix in a(x) = b
    TArray2 b;
    TArray2 x;
    {
      TArray2 I_cA; I_cA("c',B'") = srr12intrmds_->_2("<c'|I|B'>"); // allvir cabs
      TArray2 F_ci; F_ci("n,c'") = srr12intrmds_->_2("<n|F|c'>");  //cabs occ

      b("x,B'") = gamma1("x,j") * F_ci("j,c") * I_cA("c,B'");
      // x we trying to solve, C

      x("x,B'") = b("x,B'");
      if (h0 == string("dyall_1")) {
        // compute b based on Equation(15)
        b("x,B'") = -b("x,B'");
      }

      if (h0 == string("dyall_2")) {
        // - gamma(j,k) * h(k, beta) - gamma(jm,kl)*g(beta m, kl)
        TArray4 g_mklB;
        g_mklB("m,m1,n1,B'") = srr12intrmds_->_4("<B' m|g|m1 n1>");//g(allvir, occ, occ, occ)
        TArray2 h_iA;
        h_iA("m,A'") = srr12intrmds_->_2("<m|h|A'>");//h(occ,allvir)
        b("j,B'") = - gamma1("j,k") * h_iA("k,B'") - gamma2("j,m,k,l") * g_mklB("m,k,l,B'");
      }
    }
    madness::World::get_default().gop.fence();

#if DEBUGG
    std::cout << "b matrix: \n" << b << std::endl;
    ofstream foutb("new_bmatrix");
    foutb.setf(ios::fixed);
    foutb.precision(10);
    foutb << setprecision(10) << b << std::endl;
#endif

    // make preconditioner: inverse of diagonal elements <A'|F|A'> - <m|h|m>
    TArray2 preconditioner;
    {

      typedef detail::diag_precond2<double> pceval_type;//!< evaluator of preconditioner
      typedef TA::Array<double, 2, LazyTensor<double, 2, pceval_type> > TArray2d;
      TArray2d Delta_iA(b.get_world(), b.trange());

      pceval_type Delta_iA_gen(TA::array_to_eigen(h_ij), TA::array_to_eigen(F_AB));
      // construct local tiles
      for (auto t = Delta_iA.trange().tiles().begin();
          t != Delta_iA.trange().tiles().end(); ++t) {
        if (Delta_iA.is_local(*t)) {
          std::array<std::size_t, 2> index;
          std::copy(t->begin(), t->end(), index.begin());
          madness::Future<typename TArray2d::value_type> tile(
              (LazyTensor<double, 2, pceval_type>(&Delta_iA, index, &Delta_iA_gen)));

          // Insert the tile into the array
          Delta_iA.set(*t, tile);
        }
      }
      preconditioner("y,A'") = Delta_iA("y,A'");
    }
    madness::World::get_default().gop.fence();

    tim.enter("conjugate gradient solver"); // time conjugate solver

    // initialize the function a(x)
    _CABS_singles<double> cabs_singles(B);
    // linear solver object
    TA::ConjugateGradientSolver<TArray2, _CABS_singles<double> > cg_solver;
    // solve the linear system, a(x) = b, cabs_singles_fock is a(x); x is x. b is b in a(x) = b
    auto resnorm = cg_solver(cabs_singles, b, x, preconditioner, 1e-12);
    //std::cout << "Converged CG to " << resnorm << std::endl;
    tim.exit();
#if DEBUGG
    std::cout << "C: \n" << x << std::endl;
#endif

    //calculate the second order energy based on Equation (16)
    double E = -1.0*dot(x("j,A'"), b("j,A'"));

    return E;
}

double CABS_Single::cabs_singles_Fock() {
  /*"Perturbative Correction for the Basis Set Incompleteness Error of CASSCF",
   * L. Kong and E.~F.~Valeev,  J. Chem. Phys. 133, 174126 (2010),
   * http://dx.doi.org/10.1063/1.3499600.
   * */
#define DEBUGG false

    ExEnv::out0() << std::endl << indent
    << "Enter PT2R12::cabs_single_Fock MPQC3_RUNTIME \n";

    Timer tim("cabs single fock");
    typedef SingleReference_R12Intermediates<double>::TArray4 TArray4;
    typedef SingleReference_R12Intermediates<double>::TArray2 TArray2;

    // go to file sr_r12intermediates.h for notation

    // density matrices
    TArray2 gamma1; gamma1("m,n") = srr12intrmds_->_2("<m|gamma|n>"); // occ
    TArray4 gamma2; gamma2("n1,m,m1,n") = srr12intrmds_->_4("<n1 m|gamma|m1 n>"); // occ

#if DEBUGG
    std::cout << "gamma1: \n" << gamma1 << std::endl;
#endif
    // fock matrices
    TArray2 F_ij; F_ij("m,n") = srr12intrmds_->_2("<m|F|n>"); // occ occ
    TArray2 F_iA; F_iA("m,A'") = srr12intrmds_->_2("<m|F|A'>"); // occ allvir

    TArray2 F_AB; F_AB("A',B'") = srr12intrmds_->_2("<A'|F|B'>"); // allvir allvir

    //delta
    TArray2 I_AB; I_AB("B',A'") = srr12intrmds_->_2("<B'|I|A'>"); // allvir allvir


    tim.enter("compute B matrix");
    // make B matrix in Equation (18)
    TArray4 B;

    // term1
    {
      TArray4 term1;
      term1("x,B',y,A'") = F_AB("A',B'") * gamma1("x,y");
      B("x,B',y,A'") =  term1("x,B',y,A'");
    }
    madness::World::get_default().gop.fence();
    // term2
    {
      TArray4 term2;
      term2("x,B',y,A'") =  F_ij("i,j") * gamma2("i,x,j,y") * I_AB("A',B'");
      B("x,B',y,A'") = B("x,B',y,A'") + term2("x,B',y,A'");
    }
    madness::World::get_default().gop.fence();
    // term3
    {
      TArray4 term3;
      term3("x,B',y,A'") = - F_ij("i,j") * gamma1("x,y") * gamma1("i,j")  * I_AB("A',B'");
      B("x,B',y,A'") = B("x,B',y,A'") + term3("x,B',y,A'");
    }
    madness::World::get_default().gop.fence();

#if DEBUGG
    std::cout << "B matrix: \n" << B << std::endl;
#endif
    tim.exit();
    //
    //  solve the linear algebra problem a(x)=b in Equation (15)
    //
    // x we trying to solve, C

    TArray2 x;
    TArray2 b;
    {
      TArray2 I_Ac; I_Ac("B',c'") = srr12intrmds_->_2("<B'|I|c'>"); // allvir cabs
      TArray2 F_ci; F_ci("c',n") = srr12intrmds_->_2("<c'|F|n>");  //cabs occ
      x("x,B'") =  I_Ac("B',c'") * F_ci("c',j") * gamma1("j,x");
      // b in a(x) = b
      b("x,B'") = -x("x,B'");
    }
    madness::World::get_default().gop.fence();

#if DEBUGG
    std::cout << "b matrix: \n" << b << std::endl;
#endif
    std::cout << " range of b matrix" << b.trange().elements() << std::endl;

    // make preconditioner: inverse of diagonal elements <A'|F|A'> - <m|F|m>
    TArray2 preconditioner;
    {
      typedef diag_precond2<double> pceval_type; //!< evaluator of preconditioner
      typedef TA::Array<double, 2, LazyTensor<double, 2, pceval_type> > TArray2d;
      TArray2d Delta_iA(b.get_world(), b.trange());

      pceval_type Delta_iA_gen(TA::array_to_eigen(F_ij), TA::array_to_eigen(F_AB));
      // construct local tiles
      for (auto t = Delta_iA.trange().tiles().begin();
          t != Delta_iA.trange().tiles().end(); ++t) {
        if (Delta_iA.is_local(*t)) {
          std::array<std::size_t, 2> index;
          std::copy(t->begin(), t->end(), index.begin());
          madness::Future<typename TArray2d::value_type> tile(
              (LazyTensor<double, 2, pceval_type>(&Delta_iA, index, &Delta_iA_gen)));

          // Insert the tile into the array
          Delta_iA.set(*t, tile);
        }
      }
      preconditioner("y,A'") = Delta_iA("y,A'");
    }


#if DEBUGG
    std::cout << "preconditioner: \n" << preconditioner << std::endl;
#endif
    tim.enter("conjugate gradient solver");
    _CABS_singles<double> cabs_singles(B); // initialize the function a(x)
    TA::ConjugateGradientSolver<TArray2, _CABS_singles<double> > cg_solver;// linear solver object

    // solve the linear system
    auto resnorm = cg_solver(cabs_singles, b, x, preconditioner, 1e-12);
    //std::cout << "Converged CG to " << resnorm << std::endl;
    tim.exit();
#if DEBUGG
    std::cout << "C: \n" << x << std::endl;
#endif

    //calculate the second order energy based on Equation (16)
    double E = -1.0* dot(x("y,A'"),b("y,A'"));
    return E;
}

#endif // MPQC3_RUNTIME
