//
// compute_tbint_tensor.h
//
// Copyright (C) 2005 Edward Valeev
//
// Author: Edward Valeev <edward.valeev@chemistry.gatech.edu>
// Maintainer: EV
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifdef __GNUG__
#pragma interface
#endif

#include <cmath>
#include <util/misc/timer.h>
#include <chemistry/qc/mbptr12/pairiter.h>
#include <chemistry/qc/mbptr12/r12int_eval.h>

#ifndef _chemistry_qc_mbptr12_computetbinttensor_h
#define _chemistry_qc_mbptr12_computetbinttensor_h

namespace sc {
  
  template <typename DataProcess,
            bool CorrFactorInBra,
            bool CorrFactorInKet>
    void
    R12IntEval::compute_tbint_tensor(RefSCMatrix& T,
                                     int tbint_type,
                                     const Ref<MOIndexSpace>& space1_bra,
                                     const Ref<MOIndexSpace>& space1_ket,
                                     const Ref<MOIndexSpace>& space2_bra,
                                     const Ref<MOIndexSpace>& space2_ket,
                                     bool antisymmetrize,
                                     const std::vector< Ref<TwoBodyMOIntsTransform> >& transvec,
                                     const std::vector< Ref<TwoBodyIntDescr> >& intdescrs)
    {
      // are particles 1 and 2 equivalent?
      const bool part1_equiv_part2 = (space1_bra==space2_bra && space1_ket==space2_ket);
      // Check correct semantics of this call : if antisymmetrize then particles must be equivalent
      const bool correct_semantics = (antisymmetrize && part1_equiv_part2) ||
                                     !antisymmetrize;
      if (!correct_semantics)
        throw ProgrammingError("R12IntEval::compute_tbint_tensor_() -- incorrect call semantics",
                               __FILE__,__LINE__);
      
      const bool CorrFactorInBraKet = CorrFactorInBra && CorrFactorInKet;
      
      const unsigned int nbrasets = (CorrFactorInBra ? corrfactor()->nfunctions() : 1);
      const unsigned int nketsets = (CorrFactorInKet ? corrfactor()->nfunctions() : 1);
      const unsigned int nsets = (CorrFactorInBraKet ? -1 : nbrasets*nketsets);
      
      // create transforms, if needed
      typedef std::vector< Ref<TwoBodyMOIntsTransform> > tformvec;
      tformvec transforms = transvec;
      if (transforms.empty()) {
        if (CorrFactorInBraKet) {
          unsigned int fbraket = 0;
          for(unsigned int fbra=0; fbra<nbrasets; ++fbra) {
            for(unsigned int fket=0; fket<nketsets; ++fket, ++fbraket) {
              std::string tlabel(transform_label(space1_bra,space1_ket,space2_bra,space2_ket,fbra,fket));
              try {
                transforms.push_back(get_tform_(tlabel));
              }
              catch (TransformNotFound& a){
                Ref<MOIntsTransformFactory> tfactory = r12info()->tfactory();
                tfactory->set_spaces(space1_bra,space1_ket,space2_bra,space2_ket);
                Ref<TwoBodyMOIntsTransform> tform = tfactory->twobody_transform(
                                                      MOIntsTransformFactory::StorageType_13,
                                                      tlabel,
                                                      intdescrs[fbraket]
                                                    );
                transforms.push_back(tform);
              }
            }
          }
        }
        else {
          for(int f=0; f<nsets; f++) {
            std::string tlabel(transform_label(space1_bra,space1_ket,space2_bra,space2_ket,f));
            try {
              transforms.push_back(get_tform_(tlabel));
            }
            catch (TransformNotFound& a){
              Ref<MOIntsTransformFactory> tfactory = r12info()->tfactory();
              tfactory->set_spaces(space1_bra,space1_ket,space2_bra,space2_ket);
              Ref<TwoBodyMOIntsTransform> tform = tfactory->twobody_transform(
                                                    MOIntsTransformFactory::StorageType_13,
                                                    tlabel,
                                                    intdescrs[f]
                                                  );
              transforms.push_back(tform);
            }
          }
        }
      }
      
      tim_enter("Generic tensor");
      std::ostringstream oss;
      oss << "<" << space1_bra->id() << " " << space2_bra->id() << (antisymmetrize ? "||" : "|")
      << space1_ket->id() << " " << space2_ket->id() << ">";
      const std::string label = oss.str();
      ExEnv::out0() << endl << indent
      << "Entered generic tensor (" << label << ") evaluator" << endl;
      ExEnv::out0() << incindent;
      
      //
      // WARNING: Assuming all transforms are over same spaces!!!
      //
      Ref<MOIndexSpace> tspace1_bra = transforms[0]->space1();
      Ref<MOIndexSpace> tspace1_ket = transforms[0]->space2();
      Ref<MOIndexSpace> tspace2_bra = transforms[0]->space3();
      Ref<MOIndexSpace> tspace2_ket = transforms[0]->space4();
      
      // maps spaceX to spaceX of the transform
      std::vector<unsigned int> map1_bra, map1_ket, map2_bra, map2_ket;
      // maps space2_ket to space1_ket of transform
      std::vector<unsigned int> map12_ket;
      // maps space1_ket to space2_ket of transform
      std::vector<unsigned int> map21_ket;
      map1_bra = *tspace1_bra<<*space1_bra;
      map1_ket = *tspace1_ket<<*space1_ket;
      map2_bra = *tspace2_bra<<*space2_bra;
      map2_ket = *tspace2_ket<<*space2_ket;
      if (antisymmetrize) {
        if (tspace1_ket == tspace2_ket) {
          map12_ket = map1_ket;
          map21_ket = map2_ket;
        }
        else {
          map12_ket = *tspace1_ket<<*space2_ket;
          map21_ket = *tspace2_ket<<*space1_ket;
        }
      }
      
      const unsigned int tblock_ncols = tspace2_ket->rank();
      const RefDiagSCMatrix evals1_bra = space1_bra->evals();
      const RefDiagSCMatrix evals1_ket = space1_ket->evals();
      const RefDiagSCMatrix evals2_bra = space2_bra->evals();
      const RefDiagSCMatrix evals2_ket = space2_ket->evals();
      
      // Using spinorbital iterators means I don't take into account perm symmetry
      // More efficient algorithm will require generic code
      const SpinCase2 S = (antisymmetrize ? AlphaAlpha : AlphaBeta);
      SpinMOPairIter iterbra(space1_bra,space2_bra,S);
      SpinMOPairIter iterket(space1_ket,space2_ket,S);
      // size of one block of <space1_bra space2_bra|
      const unsigned int nbra = iterbra.nij();
      // size of one block of |space1_ket space2_ket>
      const unsigned int nket = iterket.nij();
      
      unsigned int fbraket = 0;
      unsigned int fbraoffset = 0;
      for(unsigned int fbra=0; fbra<nbrasets; ++fbra,fbraoffset+=nbra) {
        
        unsigned int fketoffset = 0;
        for(unsigned int fket=0; fket<nketsets; ++fket,fketoffset+=nket,++fbraket) {
          
          Ref<TwoBodyMOIntsTransform> tform = transforms[fbraket];
          
          if (debug_ > 0)
            ExEnv::out0() << indent << "Using transform " << tform->name() << std::endl;
          
          Ref<R12IntsAcc> accum = tform->ints_acc();
          // if transforms have not been computed yet, compute
          if (accum.null() || !accum->is_committed()) {
            tform->compute();
          }
          if (!accum->is_active())
            accum->activate();
          
          // split work over tasks which have access to integrals
          vector<int> proc_with_ints;
          const int nproc_with_ints = tasks_with_ints_(accum,proc_with_ints);
          const int me = r12info()->msg()->me();
          
          if (accum->has_access(me)) {
            for(iterbra.start(); iterbra; iterbra.next()) {
              const int ij = iterbra.ij();
              
              const int ij_proc = ij%nproc_with_ints;
              if (ij_proc != proc_with_ints[me])
                continue;
              
              const unsigned int i = iterbra.i();
              const unsigned int j = iterbra.j();
              const unsigned int ii = map1_bra[i];
              const unsigned int jj = map2_bra[j];
              
              if (debug_)
                ExEnv::outn() << indent << "task " << me << ": working on (i,j) = " << i << "," << j << " " << endl;
              tim_enter("MO ints retrieve");
              const double *ij_buf = accum->retrieve_pair_block(ii,jj,tbint_type);
              tim_exit("MO ints retrieve");
              if (debug_)
                ExEnv::outn() << indent << "task " << me << ": obtained ij blocks" << endl;
              
              for(iterket.start(); iterket; iterket.next()) {
                const unsigned int a = iterket.i();
                const unsigned int b = iterket.j();
                const unsigned int aa = map1_ket[a];
                const unsigned int bb = map2_ket[b];
                const int AB = aa*tblock_ncols+bb;
                const int ab = iterket.ij();
                
                const double I_ijab = ij_buf[AB];
                
                if (!antisymmetrize) {
                  if (debug_ > 2) {
                    ExEnv::out0() << "i = " << i << " j = " << j << " a = " << a << " b = " << b
                    << " <ij|ab> = " << I_ijab << endl;
                  }
                  const double T_ijab = DataProcess::I2T(I_ijab,i,j,a,b,evals1_bra,evals1_ket,evals2_bra,evals2_ket);
                  T.accumulate_element(ij+fbraoffset,ab+fketoffset,T_ijab);
                }
                else {
                  const int aa = map21_ket[a];
                  const int bb = map12_ket[b];
                  const int BA = bb*tblock_ncols+aa;
                  const double I_ijba = ij_buf[BA];
                  if (debug_ > 2) {
                    ExEnv::out0() << "i = " << i << " j = " << j << " a = " << a << " b = " << b
                    << " <ij|ab> = " << I_ijab << " <ij|ba> = " << I_ijba << endl;
                  }
                  const double I_anti = I_ijab - I_ijba;
                  const double T_ijab = DataProcess::I2T(I_anti,i,j,a,b,evals1_bra,evals1_ket,evals2_bra,evals2_ket);
                  T.accumulate_element(ij+fbraoffset,ab+fketoffset,T_ijab);
                }
                
              } // ket loop
              accum->release_pair_block(ii,jj,tbint_type);
              
            } // bra loop
          } // loop over tasks with access
          
        }
      }
      
      ExEnv::out0() << decindent;
      ExEnv::out0() << indent << "Exited generic tensor (" << label << ") evaluator" << endl;
      tim_exit("Generic tensor");
    }
  
#if 1
  /// Contains classes used to compute many-body tensors
  namespace ManyBodyTensors {
    
    enum Sign {
      Minus = -1,
      Plus = +1
    };
    
    /// Tensor elements are <pq||rs>
    template <Sign sign = Plus>
    class Apply_Identity {
    public:
      static double I2T(double I, int i1, int i3, int i2, int i4,
      const RefDiagSCMatrix& evals1,
      const RefDiagSCMatrix& evals2,
      const RefDiagSCMatrix& evals3,
      const RefDiagSCMatrix& evals4)
      {
        if (sign == Plus)
          return I;
        else
          return -I;
      }
    };
    
    /// Applies (H0 - E0)
    template <Sign sign = Plus>
    class Apply_H0minusE0 {
    public:
      static double I2T(double I, int i1, int i3, int i2, int i4,
      const RefDiagSCMatrix& evals1,
      const RefDiagSCMatrix& evals2,
      const RefDiagSCMatrix& evals3,
      const RefDiagSCMatrix& evals4)
      {
        const double ediff = (- evals1(i1) - evals3(i3) + evals2(i2) + evals4(i4));
        if (sign == Plus)
          return I*ediff;
        else
          return -I*ediff;
      }
    };
    
    /// Applies (H0 - E0)^{-1}, e.g. MP2 T2 tensor elements are <ij||ab> /(e_i + e_j - e_a - e_b)
    template <Sign sign = Plus>
    class Apply_Inverse_H0minusE0 {
    public:
      static double I2T(double I, int i1, int i3, int i2, int i4,
      const RefDiagSCMatrix& evals1,
      const RefDiagSCMatrix& evals2,
      const RefDiagSCMatrix& evals3,
      const RefDiagSCMatrix& evals4)
      {
        const double ediff = (- evals1(i1) - evals3(i3) + evals2(i2) + evals4(i4));
        if (sign == Plus)
          return I/ediff;
        else
          return -I/ediff;
      }
    };
    
    /// Applies 1.0/sqrt(H0-E0)
    /// MP2 pseudo-T2 (S2) tensor elements are <ij||ab> /sqrt(|e_i + e_j - e_a - e_b|) such
    /// that MP2 pair energies are the diagonal elements of S2 * S2.t()
    template <Sign sign = Plus>
    class Apply_Inverse_Sqrt_H0minusE0 {
    public:
      static double I2T(double I, int i1, int i3, int i2, int i4,
                 const RefDiagSCMatrix& evals1,
                 const RefDiagSCMatrix& evals2,
                 const RefDiagSCMatrix& evals3,
                 const RefDiagSCMatrix& evals4)
      {
        const double ediff = (- evals1(i1) - evals3(i3) + evals2(i2) + evals4(i4));
        if (sign == Plus)
          return I/std::sqrt(std::fabs(ediff));
        else
          return -I/std::sqrt(std::fabs(ediff));
      }
    };
    
    typedef Apply_Identity<Plus> I_to_T;
    typedef Apply_Identity<Minus> I_to_mT;
    typedef Apply_Inverse_Sqrt_H0minusE0<Plus> ERI_to_S2;
    typedef Apply_Inverse_H0minusE0<Minus> ERI_to_T2;
  }
#endif

}

#endif
