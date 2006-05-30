// 
// File:          MPQC_IntegralEvaluator4_Impl.hh
// Symbol:        MPQC.IntegralEvaluator4-v0.2
// Symbol Type:   class
// Babel Version: 0.10.12
// Description:   Server-side implementation for MPQC.IntegralEvaluator4
// 
// WARNING: Automatically generated; only changes within splicers preserved
// 
// babel-version = 0.10.12
// 

#ifndef included_MPQC_IntegralEvaluator4_Impl_hh
#define included_MPQC_IntegralEvaluator4_Impl_hh

#ifndef included_sidl_cxx_hh
#include "sidl_cxx.hh"
#endif
#ifndef included_MPQC_IntegralEvaluator4_IOR_h
#include "MPQC_IntegralEvaluator4_IOR.h"
#endif
// 
// Includes for all method dependencies.
// 
#ifndef included_Chemistry_QC_GaussianBasis_CompositeIntegralDescr_hh
#include "Chemistry_QC_GaussianBasis_CompositeIntegralDescr.hh"
#endif
#ifndef included_Chemistry_QC_GaussianBasis_DerivCenters_hh
#include "Chemistry_QC_GaussianBasis_DerivCenters.hh"
#endif
#ifndef included_Chemistry_QC_GaussianBasis_IntegralDescr_hh
#include "Chemistry_QC_GaussianBasis_IntegralDescr.hh"
#endif
#ifndef included_Chemistry_QC_GaussianBasis_Molecular_hh
#include "Chemistry_QC_GaussianBasis_Molecular.hh"
#endif
#ifndef included_MPQC_IntegralEvaluator4_hh
#include "MPQC_IntegralEvaluator4.hh"
#endif
#ifndef included_sidl_BaseException_hh
#include "sidl_BaseException.hh"
#endif
#ifndef included_sidl_BaseInterface_hh
#include "sidl_BaseInterface.hh"
#endif
#ifndef included_sidl_ClassInfo_hh
#include "sidl_ClassInfo.hh"
#endif


// DO-NOT-DELETE splicer.begin(MPQC.IntegralEvaluator4._includes)
#include "integral_evaluator.h"
using namespace sc;
using namespace Chemistry::QC::GaussianBasis;
using namespace MpqcCca;
// DO-NOT-DELETE splicer.end(MPQC.IntegralEvaluator4._includes)

namespace MPQC { 

  /**
   * Symbol "MPQC.IntegralEvaluator4" (version 0.2)
   */
  class IntegralEvaluator4_impl
  // DO-NOT-DELETE splicer.begin(MPQC.IntegralEvaluator4._inherits)

  /** IntegralEvaluator4_impl implements a class interface for
      supplying 4-center molecular integrals.

      This is an implementation of a SIDL interface.
      The stub code is generated by the Babel tool.  Do not make
      modifications outside of splicer blocks, as these will be lost.
      This is a server implementation for a Babel class, the Babel
      client code is provided by the cca-chem-generic package.
   */

  // Put additional inheritance here...
  // DO-NOT-DELETE splicer.end(MPQC.IntegralEvaluator4._inherits)
  {

  private:
    // Pointer back to IOR.
    // Use this to dispatch back through IOR vtable.
    IntegralEvaluator4 self;

    // DO-NOT-DELETE splicer.begin(MPQC.IntegralEvaluator4._implementation)

    IntegralEvaluator< TwoBodyInt, twobody_computer > eval_;
    twobody_computer computer_;

    // DO-NOT-DELETE splicer.end(MPQC.IntegralEvaluator4._implementation)

  private:
    // private default constructor (required)
    IntegralEvaluator4_impl() 
    {} 

  public:
    // sidl constructor (required)
    // Note: alternate Skel constructor doesn't call addref()
    // (fixes bug #275)
    IntegralEvaluator4_impl( struct MPQC_IntegralEvaluator4__object * s ) : 
      self(s,true) { _ctor(); }

    // user defined construction
    void _ctor();

    // virtual destructor (required)
    virtual ~IntegralEvaluator4_impl() { _dtor(); }

    // user defined destruction
    void _dtor();

    // static class initializer
    static void _load();

  public:

    /**
     * user defined non-static method.
     */
    void
    add_evaluator (
      /* in */ void* eval,
      /* in */ ::Chemistry::QC::GaussianBasis::IntegralDescr desc
    )
    throw () 
    ;

    /**
     * user defined non-static method.
     */
    void
    set_basis (
      /* in */ ::Chemistry::QC::GaussianBasis::Molecular bs1,
      /* in */ ::Chemistry::QC::GaussianBasis::Molecular bs2,
      /* in */ ::Chemistry::QC::GaussianBasis::Molecular bs3,
      /* in */ ::Chemistry::QC::GaussianBasis::Molecular bs4
    )
    throw () 
    ;

    /**
     * user defined non-static method.
     */
    void
    init_reorder() throw () 
    ;

    /**
     * Get buffer pointer for given type.
     * @return Buffer pointer. 
     */
    void*
    get_buffer (
      /* in */ ::Chemistry::QC::GaussianBasis::IntegralDescr desc
    )
    throw ( 
      ::sidl::BaseException
    );

    /**
     * user defined non-static method.
     */
    ::Chemistry::QC::GaussianBasis::DerivCenters
    get_deriv_centers() throw ( 
      ::sidl::BaseException
    );
    /**
     * user defined non-static method.
     */
    ::Chemistry::QC::GaussianBasis::CompositeIntegralDescr
    get_descriptor() throw ( 
      ::sidl::BaseException
    );

    /**
     * Compute a shell quartet of integrals.
     * @param shellnum1 Gaussian shell number 1.
     * @param shellnum2 Gaussian shell number 2.
     * @param shellnum3 Gaussian shell number 3.
     * @param shellnum4 Gaussian shell number 4.
     * @param deriv_level Derivative level. 
     */
    void
    compute (
      /* in */ int64_t shellnum1,
      /* in */ int64_t shellnum2,
      /* in */ int64_t shellnum3,
      /* in */ int64_t shellnum4
    )
    throw ( 
      ::sidl::BaseException
    );


    /**
     * Compute a shell quartet of integrals and return as a borrowed
     * sidl array.
     * @param shellnum1 Gaussian shell number 1.
     * @param shellnum2 Gaussian shell number 2.
     * @param shellnum3 Gaussian shell number 3.
     * @param shellnum4 Gaussian shell number 4.
     * @return Borrowed sidl array buffer. 
     */
    ::sidl::array<double>
    compute_array (
      /* in */ int64_t shellnum1,
      /* in */ int64_t shellnum2,
      /* in */ int64_t shellnum3,
      /* in */ int64_t shellnum4
    )
    throw ( 
      ::sidl::BaseException
    );


    /**
     * Compute integral bounds.
     * @param shellnum1 Gaussian shell number 1.
     * @param shellnum2 Gaussian shell number 2.
     * @param shellnum3 Gaussian shell number 3.
     * @param shellnum4 Gaussian shell number 4. 
     */
    double
    compute_bounds (
      /* in */ int64_t shellnum1,
      /* in */ int64_t shellnum2,
      /* in */ int64_t shellnum3,
      /* in */ int64_t shellnum4
    )
    throw ( 
      ::sidl::BaseException
    );

  };  // end class IntegralEvaluator4_impl

} // end namespace MPQC

// DO-NOT-DELETE splicer.begin(MPQC.IntegralEvaluator4._misc)
// Put miscellaneous things here...
// DO-NOT-DELETE splicer.end(MPQC.IntegralEvaluator4._misc)

#endif
