//
// bc_contrib.cc
//
// Copyright (C) 2007 Edward Valeev
//
// Author: Edward Valeev <evaleev@vt.edu>
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

#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include <mpqc_config.h>
#include <util/misc/formio.h>
#include <util/misc/regtime.h>
#include <util/class/class.h>
#include <util/state/state.h>
#include <util/state/state_text.h>
#include <util/state/state_bin.h>
#include <math/scmat/local.h>
#include <math/scmat/matrix.h>
#include <chemistry/molecule/molecule.h>
#include <chemistry/qc/basis/integral.h>
#include <math/distarray4/distarray4.h>
#include <chemistry/qc/mbptr12/r12wfnworld.h>
#include <math/mmisc/pairiter.h>
#include <chemistry/qc/mbptr12/r12int_eval.h>
#include <chemistry/qc/mbptr12/creator.h>
#include <chemistry/qc/mbptr12/container.h>
#include <chemistry/qc/mbptr12/compute_tbint_tensor.h>
#include <chemistry/qc/mbptr12/twoparticlecontraction.h>
#include <chemistry/qc/lcao/utils.h>
#include <chemistry/qc/lcao/utils.impl.h>
#include <util/misc/print.h>

using namespace std;
using namespace sc;

void
R12IntEval::compute_B_fX_()
{
  if (evaluated_)
    return;

  Ref<MessageGrp> msg = r12world()->world()->msg();
  int me = msg->me();
  int ntasks = msg->n();
  const bool vbs_eq_obs = r12world()->obs_eq_vbs();

  Timer tim("B(fX) intermediate");
  ExEnv::out0() << endl << indent << "Entered B(fX) intermediate evaluator"
      << endl;
  ExEnv::out0() << incindent;

  for (int s=0; s<nspincases2(); s++) {
    const SpinCase2 spincase2 = static_cast<SpinCase2>(s);
    const SpinCase1 spin1 = case1(spincase2);
    const SpinCase1 spin2 = case2(spincase2);
    Ref<OrbitalSpace> GGspace1 = GGspace(spin1);
    Ref<OrbitalSpace> GGspace2 = GGspace(spin2);

    if (dim_oo(spincase2).n() == 0)
      continue;

    RefSCMatrix Q = B_[s].clone();
    Q.assign(0.0);
    // if Brillouin condition does not hold or VBS!=OBS, compute X_{kl}^{ij_F} explicitly
    if (!this->bc() || !vbs_eq_obs) {
      // compute Q = X_{xy}^{xy_{F}}
      if (vbs_eq_obs) { // if VBS == OBS: X_{kl}^{ip} F_p^j
        Ref<OrbitalSpace> F_x2 = F_x_p(spin2);
        compute_X_(Q, spincase2, GGspace1, GGspace2, GGspace1, F_x2);
      } else { // if VBS != OBS: X_{kl}^{im} F_m^j + X_{kl}^{ia} F_a^j
        Ref<OrbitalSpace> F_x2 = F_x_m(spin2);
        compute_X_(Q, spincase2, GGspace1, GGspace2, GGspace1, F_x2);
        F_x2 = F_x_a(spin2);
        compute_X_(Q, spincase2, GGspace1, GGspace2, GGspace1, F_x2);
      }

      if (GGspace1 != GGspace2) {
        if (vbs_eq_obs) { // if VBS == OBS: X_{kl}^{ip} F_p^j
          Ref<OrbitalSpace> F_x1 = F_x_p(spin1);
          compute_X_(Q, spincase2, GGspace1, GGspace2, F_x1, GGspace2);
        } else { // if VBS != OBS: X_{kl}^{im} F_m^j + X_{kl}^{ia} F_a^j
          Ref<OrbitalSpace> F_x1 = F_x_m(spin1);
          compute_X_(Q, spincase2, GGspace1, GGspace2, F_x1, GGspace2);
          F_x1 = F_x_a(spin1);
          compute_X_(Q, spincase2, GGspace1, GGspace2, F_x1, GGspace2);
        }
      } else {
        Q.scale(2.0);
        if (spincase2 == AlphaBeta) {
          symmetrize<false>(Q, Q, GGspace1, GGspace1);
        }
      }
    } // bc = false || vbs!=obs
    else { // bc = true, vbs==obs

      const int num_f12 = corrfactor()->nfunctions();
      const int nxy = dim_GG(spincase2).n();
      RefDiagSCMatrix evals_GGspace1 = GGspace1->evals();
      RefDiagSCMatrix evals_GGspace2 = GGspace2->evals();

      // replicate X on all nodes (don't forget to zero out all instances except on node 0)
      globally_sum_scmatrix_(X_[s],true);
      const RefSCMatrix& X = X_[s];

      for(int f=0; f<num_f12; f++) {
        const int f_off = f*nxy;

        SpinMOPairIter kl_iter(GGspace1->rank(), GGspace2->rank(), spincase2);
        for(kl_iter.start(); kl_iter; kl_iter.next()) {
          const int kl = kl_iter.ij() + f_off;
          if (kl%ntasks != me)
            continue;
          const int k = kl_iter.i();
          const int l = kl_iter.j();

          for(int g=0; g<=f; g++) {
            const int g_off = g*nxy;

            SpinMOPairIter ow_iter(GGspace1->rank(), GGspace2->rank(), spincase2);
            for(ow_iter.start(); ow_iter; ow_iter.next()) {
              const int ow = ow_iter.ij() + g_off;
              const int o = ow_iter.i();
              const int w = ow_iter.j();

              const double fx = 0.5 * (evals_GGspace1(k) + evals_GGspace2(l)
                                     + evals_GGspace1(o) + evals_GGspace2(w))
                                    * X.get_element(kl, ow);

              Q.accumulate_element(kl,ow,fx);
            }
          }
        }
      }

      // remember -- X got replicated
      if (me != 0)
        X.assign(0.0);
    }

    if (debug_ >= DefaultPrintThresholds::mostO4) {
      std::string label = prepend_spincase(spincase2, "B(fX) contribution");
      ExEnv::out0() << indent << __FILE__ << ": "<<__LINE__<<"\n";
      Q.print(label.c_str());
    }
    B_[s].accumulate(Q);
    Q = 0;

    // Bra-Ket symmetrize the B(fX) contribution
    B_[s].scale(0.5);
    RefSCMatrix B_t = B_[s].t();
    B_[s].accumulate(B_t);
  }

  globally_sum_intermeds_();

  ExEnv::out0() << decindent;
  ExEnv::out0() << indent << "Exited B(fX) intermediate evaluator" << endl;
}

////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End:
