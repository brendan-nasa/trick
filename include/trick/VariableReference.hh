/*************************************************************************
PURPOSE: (A variable server variable reference. Refactor of VariableReference
 based on the VariableServerVariable class previously in the webserver.)
**************************************************************************/
#ifndef VARIABLE_REFERENCE_HH
#define VARIABLE_REFERENCE_HH

#include "trick/VariableServerResources.hh"
#include "trick/reference.h"

#include <iostream>
#include <memory>
#include <time.h>
#include <vector>

#define MAX_ARRAY_LENGTH 4096

union cv_converter ;
namespace Trick {

    /**
     * The unit conversion in effect for a VariableReference: the converter together with
     * the label describing it. Defined in VariableReference.cpp -- it is only ever held
     * behind a shared_ptr, which does not require a complete type here.
     *
     * Replacement publishes a whole new instance rather than mutating this one. A reader
     * that has taken a reference therefore keeps its converter alive for as long as it is
     * formatting, and can never pair one replacement's factor with another's label.
     */
    struct VariableReferenceUnits;

    class VariableReference {

    public:
        friend std::ostream& operator<< (std::ostream& s, const Trick::VariableReference& ref);

        VariableReference(std::string var_name);

        // Special constructor to deal with time
        VariableReference(std::string var_name, double* time);

        std::string getName() const;
        TRICK_TYPE getType() const;

        std::string getBaseUnits() const;
        int setRequestedUnits(std::string new_units);

        // These variables have 2 staging buffers that we can swap between to allow for different copy and write-out modes
        // stageValue copies data from the simulation into one of the buffers (stage_buffer)
        // prepareForWrite swaps the pointers for stage_buffer and write_buffer
        // This way we can have data ready for writing, but also be copying out from the sim at the same time
        // stageValue must be called first, and then prepare for write, and then writeValue* can be called.
        int stageValue(bool validate_address = false);
        int prepareForWrite();
        bool isStaged() const;
        bool isWriteReady() const;

        // Write out the value to the given outstream.
        // write_ready must be true
        int getSizeAscii() const;
        int getSizeBinary() const;
        int writeValueAscii( std::ostream& out ) const;
        int writeValueBinary( std::ostream& out , bool byteswap = false) const;
        int writeNameBinary( std::ostream& out, bool byteswap = false) const;
        int writeNameLengthBinary( std::ostream& out, bool byteswap = false) const;
        int writeSizeBinary( std::ostream& out, bool byteswap = false) const;
        int writeTypeBinary( std::ostream& out, bool byteswap = false) const;

        bool validate();
        void tagAsInvalid();

        // Helper method for byteswapping
        static void byteswap_var (char * out, char * in, const VariableReference& ref);

    private:
        VariableReference();
        void byteswap_var(char * out, char * in) const;

        // Error refs
        static Ref2Ptr make_error_ref(std::string in_name);
        static Ref2Ptr make_do_not_resolve_ref(std::string in_name);

        static int _bad_ref_int;
        static int _do_not_resolve_bad_ref_int;

        Ref2Ptr _var_info;                    // ** reference info, released via ref_free + free
        void * _address;                      // -- address of data copied to buffer
        int    _size;                         // -- size of data copied to buffer
        bool   _deref;                        // -- indicates whether variable is pointer that needs to be dereferenced
        // Read via std::atomic_load, replaced via std::atomic_store: setRequestedUnits()
        // runs on the client session thread while writeValueAscii() can run concurrently on
        // the simulation thread under VS_COPY_SCHEDULED + VS_WRITE_WHEN_COPIED.
        std::shared_ptr<const VariableReferenceUnits> _units; // ** conversion factor + label
        TRICK_TYPE _trick_type ;             // -- Trick type of this variable
        bool _used_stl_indexing;             // -- indicates if reference involved STL container indexing

        bool _staged;
        bool _write_ready;

        std::vector<char> _stage_buffer; // ** staging buffer, swapped with _write_buffer
        std::vector<char> _write_buffer; // ** write buffer, swapped with _stage_buffer

        std::string _base_units;
        std::string _name;
    };

    std::ostream& operator<< (std::ostream& s, const Trick::VariableReference& ref);
}
#endif
