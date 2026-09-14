#include "trick/VariableServerResources.hh"

#include "trick/memorymanager_c_intf.h"

#include <stdlib.h>
#include <udunits2.h>

void Trick::Ref2Deleter::operator()(REF2* ref) const
{
    if (ref != NULL)
    {
        // Releases the allocations inside the REF2; does not free the REF2 itself.
        ref_free(ref);
        free(ref);
    }
}

void Trick::CvConverterDeleter::operator()(cv_converter* converter) const
{
    if (converter != NULL)
    {
        cv_free(converter);
    }
}
