

#ifdef __GNUC__
#pragma implementation
#endif

#include <math.h>

#include <util/class/scexception.h>
#include <util/misc/regtime.h>
#include <util/misc/formio.h>
#include <util/state/stateio.h>

#include <chemistry/qc/basis/petite.h>

#include <chemistry/qc/scf/fbclhf.h>
#include <chemistry/qc/scf/fockbuild.h>
#include <chemistry/qc/scf/clhfcontrib.h>

using namespace std;
using namespace sc;

static ClassDesc FockBuildCLHF_cd(
  typeid(FockBuildCLHF),"FockBuildCLHF",1,"public CLHF",
  0, create<FockBuildCLHF>, create<FockBuildCLHF>);

FockBuildCLHF::FockBuildCLHF(StateIn& s) :
  SavableState(s),
  CLHF(s)
{
  fockdist_ << SavableState::restore_state(s);
  s.get(fockbuildmatrixtype_);
  s.get(prefetch_blocks_);
}

FockBuildCLHF::FockBuildCLHF(const Ref<KeyVal>& keyval) :
  CLHF(keyval)
{
  fockdist_ << keyval->describedclassvalue("fockdist");
  if (fockdist_.null()) {
      fockdist_ = new FockDistribution;
    }
  KeyValValuestring deffbm("replicated");
  fockbuildmatrixtype_ = keyval->stringvalue("fockbuildmatrixtype",deffbm);
  if (fockbuildmatrixtype_ != "replicated"
      && fockbuildmatrixtype_ != "distributed"
      && fockbuildmatrixtype_ != "prefetched_distributed") {
      throw InputError("fockbuildmatrixtype must be \"replicated\","
                           "\"distributed\", or \"prefetched_distributed\".",
                           __FILE__,
                           __LINE__,
                           "fockbuildmatrixtype",
                           fockbuildmatrixtype_.c_str(),
                           class_desc());
    }
  if (fockbuildmatrixtype_ == "prefetched_distributed") {
      prefetch_blocks_ = true;
  }
  else {
      prefetch_blocks_ = false;
    }
}

FockBuildCLHF::~FockBuildCLHF()
{
}

void
FockBuildCLHF::save_data_state(StateOut& s)
{
  CLHF::save_data_state(s);
  SavableState::save_state(fockdist_.pointer(),s);
  s.put(fockbuildmatrixtype_);
  s.put(prefetch_blocks_);
}

void
FockBuildCLHF::init_threads()
{
  Ref<GaussianBasisSet> gbs(basis());
  Ref<FockContribution> fc
      = new CLHFContribution(gbs,gbs,gbs,fockbuildmatrixtype_);
  fb_ = new FockBuild(fockdist_,
                      fc,
                      prefetch_blocks_,
                      gbs, gbs, gbs,
                      scf_grp_, threadgrp_, integral());
}

void
FockBuildCLHF::done_threads()
{
  fb_ = 0;
}

void
FockBuildCLHF::ao_fock(double accuracy)
{
  Timer routine_tim("ao_fock");
  Timer step_tim("misc");
  int nthread = threadgrp_->nthread();

  Ref<GaussianBasisSet> gbs = basis();
  Ref<PetiteList> pl = integral()->petite_list(gbs);
  
  // transform the density difference to the AO basis
  RefSymmSCMatrix dd = cl_dens_diff_;
  cl_dens_diff_ = pl->to_AO_basis(dd);

  // These two lines are needed to get the same cl_dens_diff_
  // as the old CLHF class has.
  //cl_dens_diff_->scale(2.0);
  //cl_dens_diff_->scale_diagonal(0.5);

  double gmat_accuracy = accuracy;
  if (min_orthog_res() < 1.0) { gmat_accuracy *= min_orthog_res(); }

  if (debug_>1) {
    cl_dens_diff_.print("cl_dens_diff before set_pmat");
  }

  fb_->contrib()->set_fmat(0, cl_gmat_);
  fb_->contrib()->set_pmat(0, cl_dens_diff_);
  fb_->set_accuracy(gmat_accuracy);

  if (debug_>1) {
    cl_gmat_.print("cl_gmat before build");
    cl_dens_diff_.print("cl_dens_diff before build");
  }

  step_tim.change("build");
  fb_->build();

  ExEnv::out0() << indent << scprintf("%20.0f integrals\n",
                                                  fb_->contrib()->nint());

  step_tim.change("misc");

  // get rid of the AO basis density difference
  cl_dens_diff_ = dd;

  // now symmetrize the skeleton G matrix, placing the result in dd
  RefSymmSCMatrix skel_gmat = cl_gmat_.copy();
  skel_gmat.scale(1.0/(double)pl->order());
  if (debug_>1) {
    skel_gmat.print("skel_gmat before symmetrize");
  }
  dd = cl_dens_diff_.clone();
  pl->symmetrize(skel_gmat,dd);
  if (debug_>1) {
    dd.print("dd after symmetrize");
  }

  // F = H+G
  cl_fock_.result_noupdate().assign(hcore_);
  cl_fock_.result_noupdate().accumulate(dd);
  accumddh_->accum(cl_fock_.result_noupdate());
  cl_fock_.computed()=1;
}

void
FockBuildCLHF::print(std::ostream&o) const
{
  CLHF::print(o);
  ExEnv::out0() << indent
                << "fockbuildmatrixtype = " << fockbuildmatrixtype_
                << std::endl;
  fockdist_->print(o);
}