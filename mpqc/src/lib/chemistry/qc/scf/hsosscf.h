//
// hsosscf.h --- definition of the high-spin open shell SCF class
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Edward Seidl <seidl@janed.com>
// Maintainer: LPS
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

#ifndef _chemistry_qc_scf_hsosscf_h
#define _chemistry_qc_scf_hsosscf_h

#ifdef __GNUC__
#pragma interface
#endif

#include <chemistry/qc/scf/scf.h>

////////////////////////////////////////////////////////////////////////////

class HSOSSCF: public SCF {
#   define CLASSNAME HSOSSCF
#   define HAVE_KEYVAL_CTOR
#   define HAVE_STATEIN_CTOR
#   include <util/state/stated.h>
#   include <util/class/classd.h>
  protected:
    int user_occupations_;
    int tndocc_;
    int tnsocc_;
    int nirrep_;
    int *ndocc_;
    int *nsocc_;

    ResultRefSymmSCMatrix cl_fock_;
    ResultRefSymmSCMatrix op_fock_;

  public:
    HSOSSCF(StateIn&);
    HSOSSCF(const RefKeyVal&);
    ~HSOSSCF();

    void save_data_state(StateOut&);

    void print(ostream&o=cout);

    double occupation(int irrep, int vectornum);

    int n_fock_matrices() const;
    RefSymmSCMatrix fock(int);
    RefSymmSCMatrix effective_fock();

    int value_implemented();
    int gradient_implemented();
    int hessian_implemented();

  protected:
    // these are temporary data, so they should not be checkpointed
    RefTwoBodyInt tbi_;
    
    RefSymmSCMatrix cl_dens_;
    RefSymmSCMatrix cl_dens_diff_;
    RefSymmSCMatrix cl_gmat_;
    RefSymmSCMatrix op_dens_;
    RefSymmSCMatrix op_dens_diff_;
    RefSymmSCMatrix op_gmat_;

    RefSymmSCMatrix cl_hcore_;

    void set_occupations(const RefDiagSCMatrix& evals);

    // scf things
    void init_vector();
    void done_vector();
    void reset_density();
    double new_density();
    double scf_energy();

    void ao_fock();

    RefSCExtrapData extrap_data();
    
    // gradient things
    void init_gradient();
    void done_gradient();

    RefSymmSCMatrix lagrangian();
    RefSymmSCMatrix gradient_density();
    void two_body_deriv(double*);

    // hessian things
    void init_hessian();
    void done_hessian();
    
};
SavableState_REF_dec(HSOSSCF);

#endif

// Local Variables:
// mode: c++
// eval: (c-set-style "ETS")
// End:
