/*
    PURPOSE:
        (Ownership wrappers for the C resources used by the variable server.)
*/

#ifndef VARIABLESERVERRESOURCES_HH
#define VARIABLESERVERRESOURCES_HH

#include "trick/reference.h"

#include <memory>

union cv_converter;

namespace Trick
{

    /**
     * Releases a REF2, whether it came from ref_attributes() or from one of the
     * VariableReference factories.
     *
     * ref_free() releases the allocations held *within* the REF2 -- the reference
     * string, the units string, ref_attr, and the address path list -- but not the
     * REF2 itself, so both calls are required. Pairing them in a single deleter is
     * the whole point: every hand-written release site except var_set_base() called
     * free() alone and leaked the reference string and the entire address path.
     *
     * ref_free() deliberately leaves attr alone. For a REF2 from ref_attributes()
     * the attr is shared and owned by the MemoryManager, so freeing it here would be
     * a double free. The VariableReference factories therefore point their synthesized
     * REF2s at static ATTRIBUTES rather than allocating their own.
     */
    struct Ref2Deleter
    {
            void operator()(REF2* ref) const;
    };

    using Ref2Ptr = std::unique_ptr<REF2, Ref2Deleter>;

    /**
     * Releases a udunits converter. cv_get_trivial() returns a static singleton, but
     * udunits documents it as safe to pass to cv_free(), so no special case is needed.
     */
    struct CvConverterDeleter
    {
            void operator()(cv_converter* converter) const;
    };

    using CvConverterPtr = std::unique_ptr<cv_converter, CvConverterDeleter>;

}

#endif
