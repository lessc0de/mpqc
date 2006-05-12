// 
// File:          MPQC_ComponentFactory_Impl.hh
// Symbol:        MPQC.ComponentFactory-v0.2
// Symbol Type:   class
// Babel Version: 0.10.12
// Description:   Server-side implementation for MPQC.ComponentFactory
// 
// WARNING: Automatically generated; only changes within splicers preserved
// 
// babel-version = 0.10.12
// xml-url       = /home/jpkenny/src/mpqc-libint2.build-shared/src/lib/chemistry/cca/server/../../../../../lib/cca/repo/MPQC.ComponentFactory-v0.2.xml
// 

#ifndef included_MPQC_ComponentFactory_Impl_hh
#define included_MPQC_ComponentFactory_Impl_hh

#ifndef included_sidl_cxx_hh
#include "sidl_cxx.hh"
#endif
#ifndef included_MPQC_ComponentFactory_IOR_h
#include "MPQC_ComponentFactory_IOR.h"
#endif
// 
// Includes for all method dependencies.
// 
#ifndef included_MPQC_ComponentFactory_hh
#include "MPQC_ComponentFactory.hh"
#endif
#ifndef included_gov_cca_CCAException_hh
#include "gov_cca_CCAException.hh"
#endif
#ifndef included_gov_cca_Component_hh
#include "gov_cca_Component.hh"
#endif
#ifndef included_gov_cca_ComponentClassDescription_hh
#include "gov_cca_ComponentClassDescription.hh"
#endif
#ifndef included_sidl_BaseInterface_hh
#include "sidl_BaseInterface.hh"
#endif
#ifndef included_sidl_ClassInfo_hh
#include "sidl_ClassInfo.hh"
#endif


// DO-NOT-DELETE splicer.begin(MPQC.ComponentFactory._includes)
// Put additional includes or other arbitrary code here...
// DO-NOT-DELETE splicer.end(MPQC.ComponentFactory._includes)

namespace MPQC { 

  /**
   * Symbol "MPQC.ComponentFactory" (version 0.2)
   */
  class ComponentFactory_impl
  // DO-NOT-DELETE splicer.begin(MPQC.ComponentFactory._inherits)

  /** ComponentFactory implements a CCA standard component
      interface for component factories.  This class is used to
      inform the embedded framework of available components in a
      statically linked executable.

      This is an implementation of a SIDL interface.
      The stub code is generated by the Babel tool.  Do not make
      modifications outside of splicer blocks, as these will be lost.
      This is a server implementation for a Babel class, the Babel
      client code is provided by the cca-spec-babel package.
   */

  // DO-NOT-DELETE splicer.end(MPQC.ComponentFactory._inherits)
  {

  private:
    // Pointer back to IOR.
    // Use this to dispatch back through IOR vtable.
    ComponentFactory self;

    // DO-NOT-DELETE splicer.begin(MPQC.ComponentFactory._implementation)
    std::vector< gov::cca::ComponentClassDescription > descriptions;
    // DO-NOT-DELETE splicer.end(MPQC.ComponentFactory._implementation)

  private:
    // private default constructor (required)
    ComponentFactory_impl() 
    {} 

  public:
    // sidl constructor (required)
    // Note: alternate Skel constructor doesn't call addref()
    // (fixes bug #275)
    ComponentFactory_impl( struct MPQC_ComponentFactory__object * s ) : self(s,
      true) { _ctor(); }

    // user defined construction
    void _ctor();

    // virtual destructor (required)
    virtual ~ComponentFactory_impl() { _dtor(); }

    // user defined destruction
    void _dtor();

    // static class initializer
    static void _load();

  public:

    /**
     * user defined non-static method.
     */
    void
    addDescription (
      /* in */ const ::std::string& className,
      /* in */ const ::std::string& classAlias
    )
    throw () 
    ;


    /**
     *  Collect the currently obtainable class name strings from
     *  factories known to the builder and the from the
     *  already instantiated components.
     *  @return The list of class description, which may be empty, that are
     *   known a priori to contain valid values for the className
     *  argument of createInstance. 
     *  @throws CCAException in the event of error.
     */
    ::sidl::array< ::gov::cca::ComponentClassDescription>
    getAvailableComponentClasses() throw ( 
      ::gov::cca::CCAException
    );

    /**
     * the component instance returned is nil if the name is unknown
     * to the factory. The component is raw: it has been constructed
     * but not initialized via setServices.
     */
    ::gov::cca::Component
    createComponentInstance (
      /* in */ const ::std::string& className
    )
    throw () 
    ;


    /**
     * reclaim any resources the factory may have associated with
     * the port it is using. This will occur after the
     * normal component shutdown  (ala componentrelease) is finished. 
     */
    void
    destroyComponentInstance (
      /* in */ const ::std::string& className,
      /* in */ ::gov::cca::Component c
    )
    throw () 
    ;

  };  // end class ComponentFactory_impl

} // end namespace MPQC

// DO-NOT-DELETE splicer.begin(MPQC.ComponentFactory._misc)
// Put miscellaneous things here...
// DO-NOT-DELETE splicer.end(MPQC.ComponentFactory._misc)

#endif