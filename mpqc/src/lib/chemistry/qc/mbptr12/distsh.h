//
// distsh.h
// based on mbpt/distsh.h
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Ida Nielsen <ida@kemi.aau.dk>
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

#ifndef _chemistry_qc_mbptr12_distsh_h
#define _chemistry_qc_mbptr12_distsh_h

#ifdef __GNUC__
#pragma interface
#endif

#include <util/misc/regtime.h>
#include <util/group/message.h>
#include <util/group/thread.h>
#include <chemistry/qc/basis/basis.h>

namespace sc {

/// Distributes shell pairs either statically or dynamically.
class DistShellPairR12 {
  private:
    Ref<MessageGrp> msg_;
    int nthread_;
    Ref<ThreadLock> lock_;
    Ref<GaussianBasisSet> bs1_;
    Ref<GaussianBasisSet> bs2_;
    bool bs1_eq_bs2_;
    int dynamic_;
    int debug_;
    int print_percent_;

    // for dynamic load balancing
    int req_type_;
    int ans_type_;
    int ncpu_less_0_;
    void serve_tasks();

    // for static load balancing
    int S_, R_;
    int ncpu_;
    int incS_, incR_;
    int mythread_;
    int ntask_;
    int print_interval_;
    int print_index_;
  public:
    /** The DistShellPairR12 class is used to distribute shell pair indices among tasks.

        Both static (round-robin) and dynamic methods are supported. The only difference
        between DistShellPairR12 and DistShellPair is that the latter assumes both shell indices
        belong to the same GaussianBasisSet and thus PQ is equivalent to QP. */
    DistShellPairR12(const Ref<MessageGrp> &, int nthread, int mythread,
                  const Ref<ThreadLock>& lock,
                  const Ref<GaussianBasisSet>& bs1, const Ref<GaussianBasisSet>& bs2);
    ~DistShellPairR12();
    /// Resets to the first shell pair.
    void init();
    /// Whether or not to use dynamic load balancing.
    void set_dynamic(int d);
    /// How much stuff to print out.
    void set_debug(int d) { debug_ = d; }
    /// How often to print status from node 0.
    void set_print_percent(int p) { print_percent_ = p; }
    /** Puts the current PQ shell pair into P and Q and returns 1.
        When there are no more shell pairs to be processed by this processor,
        0 is returned.  Once we start doing get_tasks, we have to go to the
        end if dynamic load balancing is used.

        P belongs to bs2, and Q belongs to bs1. If (bs1 == bs2) then
        P is greater or equal to Q. */
    int get_task(int &P, int &Q);
};

}

#endif

// //////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End:
