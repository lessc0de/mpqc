//
// scfden.cc
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

#ifdef __GNUC__
#pragma implementation
#endif

#include <math/scmat/offset.h>
#include <math/scmat/blkiter.h>

#include <chemistry/qc/scf/scfden.h>

SCFDensity::SCFDensity(SCF *s, const RefSCMatrix&v, double o)
  : scf_(s), vec(v), occ(o)
{
}

SCFDensity::~SCFDensity()
{
}

int
SCFDensity::has_side_effects()
{
  return 1;
}

void
SCFDensity::set_occ(double o)
{
  occ=o;
}

void
SCFDensity::process(SCMatrixBlockIter& bi)
{
  int ir=0;

  RefSCMatrix vir=vec;
  
  if (BlockedSCMatrix::castdown(vec.pointer())) {
    ir=current_block();
    vir = BlockedSCMatrix::castdown(vec.pointer())->block(ir);
  }

  int nbasis=vir.ncol();

  if (!nbasis)
    return;
      
  double *ck = new double[nbasis];

  // loop over columns of the scf vector
  for (int k=0;  k < nbasis; k++) {
    double occk = scf_->occupation(ir,k);
    if (fabs(occk) < 1.0e-8)
      break;
    
    if (fabs(occk-occ) > 1.0e-8)
      continue;
    
    RefSCVector rck = vir.get_column(k);
    rck->convert(ck);

    for (bi.reset(); bi; bi++) {
      bi.set(bi.get() + ck[bi.i()]*ck[bi.j()]);
    }
  }

  delete[] ck;
}

//////////////////////////////////////////////////////////////////////////////

SCFEnergy::SCFEnergy()
  : eelec(0), deferred_(0)
{
}

SCFEnergy::~SCFEnergy()
{
}

int
SCFEnergy::has_collect()
{
  return 1;
}

void
SCFEnergy::defer_collect(int h)
{
  deferred_=h;
}

void
SCFEnergy::collect(const RefMessageGrp&grp)
{
  if (!deferred_)
    grp->sum(eelec);
}

double
SCFEnergy::result()
{
  return eelec;
}

void
SCFEnergy::reset()
{
  eelec=0.0;
}

void
SCFEnergy::process(SCMatrixBlockIter&i, SCMatrixBlockIter&j)
{
  for (i.reset(), j.reset(); i && j; i++, j++) {
    int ii=i.i(); int jj=j.j();
    eelec += (ii==jj) ? 0.5*j.get()*i.get() : i.get()*j.get();
  }
}

//////////////////////////////////////////////////////////////////////////////

LevelShift::LevelShift(SCF *s) :
  scf_(s)
{
  shift=0.0;
}

LevelShift::~LevelShift()
{
}

int
LevelShift::has_side_effects()
{
  return 1;
}

void
LevelShift::set_shift(double s)
{
  shift=s;
}

void
LevelShift::process(SCMatrixBlockIter& i)
{
  int ir=current_block();
  for (i.reset(); i; i++) {
    if (i.i() != i.j())
      continue;
    
    double occi = scf_->occupation(ir,i.i());
    
    if (occi==scf_->occupation(ir,0))
      i.set(i.get()-shift);
    else if (occi>0.0)
      i.set(i.get()-0.5*shift);
  }
}

//////////////////////////////////////////////////////////////////////////////

MOLagrangian::MOLagrangian(SCF *s) :
  scf_(s)
{
}

MOLagrangian::~MOLagrangian()
{
}

int
MOLagrangian::has_side_effects()
{
  return 1;
}

void
MOLagrangian::process(SCMatrixBlockIter& bi1, SCMatrixBlockIter& bi2)
{
  int ir=current_block();

  for (bi1.reset(), bi2.reset(); bi1 && bi2; bi1++, bi2++) {
    double occi = scf_->occupation(ir,bi1.i());
    double occj = scf_->occupation(ir,bi1.j());

    if (occi > 0.0 && occi < 2.0 && occj > 0.0 && occj < 2.0)
      bi1.set(bi2.get());
    else if (occi==0.0)
      bi1.set(0.0);
  }
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// eval: (c-set-style "ETS")
// End:
