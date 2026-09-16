#include "trick/VariableReference.hh"

#include "trick/ReferenceUtils.hh"
#include "trick/UdUnits.hh"
#include "trick/bitfield_proto.h"
#include "trick/map_trick_units_to_udunits.hh"
#include "trick/memorymanager_c_intf.h"
#include "trick/message_proto.h"
#include "trick/message_type.h"
#include "trick/trick_byteswap.h"
#include "trick/wcs_ext.h"

#include <iomanip> // for setprecision
#include <iostream>
#include <math.h> // for fpclassify
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <udunits2.h>

// The unit conversion in effect, published as an immutable whole. See VariableReference.hh.
struct Trick::VariableReferenceUnits
{
    CvConverterPtr converter;
    std::string    requested_units;
};

// Publish a new conversion. Readers already holding the previous one keep it alive.
// Takes the converter by owning value: the caller's converter is adopted before this
// allocates, so a throw from the allocation releases it instead of leaking it.
static std::shared_ptr<const Trick::VariableReferenceUnits> make_units(Trick::CvConverterPtr converter,
                                                                       const std::string& label)
{
    return std::shared_ptr<const Trick::VariableReferenceUnits>(
        new Trick::VariableReferenceUnits{std::move(converter), label});
}

// Static variables to be addresses that are known to be the error ref address
int Trick::VariableReference::_bad_ref_int = 0 ;
int Trick::VariableReference::_do_not_resolve_bad_ref_int = 0 ;

// ATTRIBUTES for the synthesized REF2s below.
//
// ref_free() does not release attr, because for a REF2 from ref_attributes() the attr
// is shared and owned by the MemoryManager. Allocating a private attr per synthesized
// ref would therefore leak it on every release. These values are constant and nothing
// outside these factories ever writes through attr, so one shared instance serves every
// error ref.
//
// These are constant-initialized: the initializers are constant expressions, so there is
// no dynamic initialization and no thread-safe-static guard to pay for. They are
// deliberately not constexpr, because REF2::attr is a non-const ATTRIBUTES*. constexpr
// would force a const_cast at every assignment and place these in a read-only section,
// turning any write through ref->attr into a crash.
static constexpr ATTRIBUTES make_bad_ref_attributes()
{
    ATTRIBUTES a { };
    a.type  = TRICK_NUMBER_OF_TYPES;
    a.units = "--";
    a.size  = sizeof(int);
    return a;
}

static constexpr ATTRIBUTES make_time_ref_attributes()
{
    ATTRIBUTES a { };
    a.type  = TRICK_DOUBLE;
    a.units = "s";
    a.size  = sizeof(double);
    return a;
}

static ATTRIBUTES bad_ref_attr  = make_bad_ref_attributes();
static ATTRIBUTES time_ref_attr = make_time_ref_attributes();

Trick::Ref2Ptr Trick::VariableReference::make_error_ref(std::string in_name)
{
    Ref2Ptr new_ref((REF2*)calloc(1, sizeof(REF2)));
    new_ref->reference = strdup(in_name.c_str()) ;
    new_ref->units = NULL ;
    new_ref->address = (char *)&_bad_ref_int ;
    new_ref->attr      = &bad_ref_attr;
    return new_ref;
}

Trick::Ref2Ptr Trick::VariableReference::make_do_not_resolve_ref(std::string in_name)
{
    Ref2Ptr new_ref((REF2*)calloc(1, sizeof(REF2)));
    new_ref->reference = strdup(in_name.c_str()) ;
    new_ref->units = NULL ;
    new_ref->address = (char *)&_do_not_resolve_bad_ref_int ;
    new_ref->attr      = &bad_ref_attr;
    return new_ref;
}

// Helper function to deal with time variable
static Trick::Ref2Ptr make_time_ref(double* time)
{
    Trick::Ref2Ptr new_ref((REF2*)calloc(1, sizeof(REF2)));
    new_ref->reference = strdup("time") ;
    new_ref->units = strdup("s") ;
    new_ref->address = (char *)time ;
    new_ref->attr      = &time_ref_attr;
    return new_ref;
}

Trick::VariableReference::VariableReference(std::string var_name, double* time) : _staged(false), _write_ready(false) {
    if (var_name != "time") {
        ASSERT(0);
    }

    _var_info = make_time_ref(time);

    // Set up member variables
    _address = _var_info->address;
    _size = _var_info->attr->size ;
    _deref = false;

    // Deal with weirdness around string vs wstring
    _trick_type = _var_info->attr->type ;

    // Allocate stage and write buffers
    _stage_buffer.assign(_size, 0);
    _write_buffer.assign(_size, 0);

    _units = make_units(CvConverterPtr(cv_get_trivial()), "s");
    _base_units = _var_info->attr->units;
    _name = _var_info->reference;
}

Trick::VariableReference::VariableReference(std::string var_name) : _staged(false), _write_ready(false) {

    if (var_name == "time") {
        ASSERT(0);
    } else {
        // get variable attributes from memory manager
        _var_info.reset(ref_attributes(var_name.c_str()));
    }

    // Handle error cases
    if (_var_info == nullptr)
    {
        // TODO: ERROR LOGGER sendErrorMessage("Variable Server could not find variable %s.\n", var_name);
        // PRINTF IS NOT AN ERROR LOGGER @me
        message_publish(MSG_ERROR, "Variable Server could not find variable %s.\n", var_name.c_str());
        _var_info = make_error_ref(var_name);
    }
    else if (_var_info->attr)
    {
        if ( _var_info->attr->type == TRICK_STRUCTURED ) {
            // sendErrorMessage("Variable Server: var_add cant add \"%s\" because its a composite variable.\n", var_name);
            message_publish(MSG_ERROR, "Variable Server: var_add cant add \"%s\" because its a composite variable.\n", var_name.c_str());

            _var_info = make_do_not_resolve_ref(var_name);
        }
    }
    else
    {
        // sendErrorMessage("Variable Server: BAD MOJO - Missing ATTRIBUTES.");
        message_publish(MSG_ERROR, "Variable Server: BAD MOJO - Missing ATTRIBUTES.");

        _var_info = make_error_ref(var_name);
    }

    // Set up member variables
    _var_info->units = NULL;
    _address = _var_info->address;
    _size = _var_info->attr->size ;
    _deref = false;

    // Use ReferenceUtils for STL-aware type and size resolution.
    // Handles cases such as: vec[0], xxx[2].yyy.zzz[3].www, xxx[2].yyy.zzz[3].aaa[0]
    _trick_type        = Trick::ReferenceUtils::effective_trick_type(_var_info.get());
    _used_stl_indexing = Trick::ReferenceUtils::is_stl_ref(_var_info.get());

    if (_used_stl_indexing)
    {
        // effective_trick_size returns the correct element byte size for every STL case
        _size = (int)Trick::ReferenceUtils::effective_trick_size(_var_info.get());
        // address already points to the correct element from ref_dim; treat as single value
    }
    else
    {
        // Non-STL: apply the original array dimension size calculation
        if ( _var_info->num_index == _var_info->attr->num_index ) {
            // single value - nothing else necessary
        } else if ( _var_info->attr->index[_var_info->attr->num_index - 1].size != 0 ) {
            // Constrained array
            for ( int i = _var_info->attr->num_index-1;  i > _var_info->num_index-1 ; i-- ) {
                _size *= _var_info->attr->index[i].size ;
            }
        } else {
            // Unconstrained array
            if ((_var_info->attr->num_index - _var_info->num_index) > 1 ) {
                message_publish(MSG_ERROR, "Variable Server Error: var_add(%s) requests more than one dimension of dynamic array.\n", _var_info->reference);
                message_publish(MSG_ERROR, "Data is not contiguous so returned values are unpredictable.\n") ;
            }
            if ( _var_info->attr->type == TRICK_CHARACTER ) {
                _trick_type = TRICK_STRING ;
                _deref = true;
            } else if ( _var_info->attr->type == TRICK_WCHAR ) {
                _trick_type = TRICK_WSTRING ;
                _deref = true;
            } else {
                _deref = true ;
                _size *= get_size((char*)_address) ;
            }
        }
    }
    // handle strings: set a max buffer size, the copy size may vary so will be set in copy_sim_data
    if (( _trick_type == TRICK_STRING ) || ( _trick_type == TRICK_WSTRING )) {
        _size = MAX_ARRAY_LENGTH ;
    }

    // Allocate stage and write buffers
    _stage_buffer.assign(_size, 0);
    _write_buffer.assign(_size, 0);

    _units = make_units(CvConverterPtr(cv_get_trivial()), "");
    _base_units = _var_info->attr->units;
    _name = _var_info->reference;

    // Done!
}


std::string Trick::VariableReference::getName() const {
    return _name;
}

int Trick::VariableReference::getSizeBinary() const {
    return _size;
}

TRICK_TYPE Trick::VariableReference::getType() const {
    return _trick_type;
}

std::string Trick::VariableReference::getBaseUnits() const {
    return _base_units;
}

int Trick::VariableReference::setRequestedUnits(std::string units_name) {
    // Some error logging lambdas - these should probably go somewhere else
    // But I do kinda like them
    auto publish = [](MESSAGE_TYPE type, const std::string& message) {
        std::ostringstream oss;
        oss << "Variable Server: " << message << std::endl;
        message_publish(type, oss.str().c_str());
    };

    auto publishError = [&](const std::string& units) {
        std::ostringstream oss;
        oss << "units error for [" << getName() << "] [" << units << "]";
        publish(MSG_ERROR, oss.str());
    };

    // If the units_name parameter is "xx", set it to the current units.
    if (!units_name.compare("xx")) {
        units_name = getBaseUnits();
    }

    // Don't try to convert units for a bad ref
    if (_var_info->address == &_bad_ref_int) {
        return -1 ;
    }

    // if unitless ('--') then do not convert to udunits
    if (units_name.compare("--")) {
        // Check to see if this is an old style Trick unit that needs to be converted to new udunits
        std::string new_units = map_trick_units_to_udunits(units_name) ;
        // Warn if a conversion has taken place
        if ( units_name.compare(new_units) ) {
            // TODO: MAKE BETTER SYSTEM FOR ERROR LOGGING
            std::ostringstream oss;
            oss << "[" << getName() << "] old-style units converted from ["
                << units_name << "] to [" << new_units << "]";
            publish(MSG_WARNING, oss.str());
        }

        // Interpret base unit
        ut_unit * from = ut_parse(Trick::UdUnits::get_u_system(), getBaseUnits().c_str(), UT_ASCII) ;
        if ( !from ) {
            std::cout << "Error in interpreting base units" << std::endl;
            publishError(getBaseUnits());
            ut_free(from) ;
            return -1 ;
        }

        // Interpret requested unit
        ut_unit * to = ut_parse(Trick::UdUnits::get_u_system(), new_units.c_str(), UT_ASCII) ;
        if ( !to ) {
            std::cout << "Error in interpreting requested units" << std::endl;
            publishError(new_units);
            ut_free(from) ;
            ut_free(to) ;
            return -1 ;
        }

        // Create a converter from the base to the requested
        auto new_conversion_factor = ut_get_converter(from, to) ;
        ut_free(from) ;
        ut_free(to) ;
        if ( !new_conversion_factor ) {
            std::ostringstream oss;
            oss << "[" << getName() << "] cannot convert units from [" << getBaseUnits()
                << "] to [" << new_units << "]";
            publish(MSG_ERROR, oss.str());
            return -1 ;
        }

        // Publish the converter and its label together, as one immutable replacement.
        //
        // This deliberately does not reset an owning pointer in place. writeValueAscii() can
        // be running on the simulation thread -- VS_COPY_SCHEDULED with VS_WRITE_WHEN_COPIED
        // calls write_data() from a sim job, and write_data() drops _copy_mutex before
        // formatting -- so freeing the old converter underneath such a reader is a
        // use-after-free. Updating the label separately would also let a packet convert with
        // one factor and advertise another. Readers take a reference to the whole snapshot.
        std::atomic_store(&_units, make_units(CvConverterPtr(new_conversion_factor), new_units));
    }
    return 0;
}

int Trick::VariableReference::stageValue(bool validate_address) {
    _write_ready = false;

    // Copy <size> bytes from <address> to staging_point.

    // Try to recreate connection if it has been broken
    if (_var_info->address == &_bad_ref_int) {
        REF2 *new_ref = ref_attributes(_var_info->reference);
        if (new_ref != NULL) {
            _var_info.reset(new_ref);
            _address = _var_info->address;
            // _requested_units = "";
        }
    }

    // if there's a pointer somewhere in the address path, follow it in case pointer changed
    // BUT: if we've indexed into an STL container, then the address already points to the correct location
    // and follow_address_path would incorrectly recalculate it (it doesn't understand STL indexing)
    // Use the _used_stl_indexing flag that was set during construction when we detected STL indexing
    // (when num_index > attr->num_index and attr->type == TRICK_STL)
    if ( _var_info->pointer_present == 1 && !_used_stl_indexing ) {
        _address = follow_address_path(_var_info.get());
        if (_address == NULL) {
            tagAsInvalid();
        } else if ( validate_address ) {
            validate();
        } else {
            _var_info->address = _address ;
        }
    }

    // if this variable is a string we need to get the raw character string out of it.
    if (( _trick_type == TRICK_STRING ) && !_deref) {
        std::string * str_ptr = (std::string *)_var_info->address ;
        // Get a pointer to the internal character array
        _address = (void *)(str_ptr->c_str()) ;
    }

    // if this variable is a wstring we need to get the raw wide character string out of it.
    if ((_trick_type == TRICK_WSTRING) && !_deref)
    {
        std::wstring *wstr_ptr = (std::wstring *)_var_info->address;
        // Get a pointer to the internal wide character array
        _address = (void *)(wstr_ptr->c_str());
    }

    // if this variable itself is a pointer, dereference it
    if ( _deref ) {
        _address = *(void**)_var_info->address ;
    }

    // handle c++ string and char*
    if ( _trick_type == TRICK_STRING ) {
        if (_address == NULL) {
            _size = 0 ;
        } else {
            _size = strlen((char*)_address) + 1 ;
        }
    }
    // handle c++ wstring and wchar_t*
    if ( _trick_type == TRICK_WSTRING ) {
        if (_address == NULL) {
            _size = 0 ;
        } else {
            _size = (wcslen((wchar_t *)_address) + 1) * sizeof(wchar_t);
        }
    }
    if(_address != NULL) {
        memcpy(_stage_buffer.data(), _address, _size);
    }

    _staged = true;
    return 0;
}

bool Trick::VariableReference::validate() {
    // The address is not NULL.
    // Should be called by VariableServer Session if validateAddress is on.
    // check the memory manager if the address falls into
    // any of the memory blocks it knows of.  Don't do this if we have a std::string or
    // wstring type, or we already are pointing to a bad ref.
    if ( (_trick_type != TRICK_STRING) and
            (_trick_type != TRICK_WSTRING) and
            (_var_info->address != &_bad_ref_int) and
            (get_alloc_info_of(_address) == NULL) ) {
        // This variable is broken, make it into an error ref
        tagAsInvalid();
        return false;
    }

    // Everything is fine
    return true;
}

static void write_escaped_string( std::ostream& os, const char* s) {
    for (int ii=0 ; ii<strlen(s) ; ii++) {
        if (isprint(s[ii])) {
            os << s[ii];
        } else {
            switch ((s)[ii]) {
                case '\n': os << "\\n"; break;
                case '\t': os << "\\t"; break;
                case '\b': os << "\\b"; break;
                case '\a': os << "\\a"; break;
                case '\f': os << "\\f"; break;
                case '\r': os << "\\n"; break;
                case '\v': os << "\\v"; break;
                case '\"': os << "\\\""; break;
                default  : {
                    // Replicating behavior from original vs_format_ascii
                    char temp_s[6];
                    sprintf(temp_s, "\\x%02x", s[ii]);
                    os << temp_s;
                    break;
                }
            }
        }
    }
}

int Trick::VariableReference::getSizeAscii() const {
    std::stringstream ss;
    writeValueAscii(ss);
    return ss.str().length();
}


int Trick::VariableReference::writeValueAscii( std::ostream& out ) const {
    // This is copied and modified from vs_format_ascii

    if (!isWriteReady()) {
        return -1;
    }

    // Take one reference to the conversion for the whole of this format. It stays valid
    // even if the session thread replaces the units underneath us, and the factor and the
    // label printed below are guaranteed to come from the same replacement.
    const std::shared_ptr<const VariableReferenceUnits> units = std::atomic_load(&_units);

    // local_type is set to the type of the attribute, but if it's a STL type, we need to use the element type.
    TRICK_TYPE local_type = _trick_type;

    if (_trick_type == TRICK_STL) {
        // If the variable is an STL type, use the STL element type for writting value
        local_type = _var_info->attr->stl_elem_type;
    }

    int bytes_written = 0;
    const char* buf_ptr = _write_buffer.data();
    while (bytes_written < _size) {
        bytes_written += _var_info->attr->size ;

        switch (local_type) {

        case TRICK_CHARACTER:
            if (_var_info->attr->num_index == _var_info->num_index) {
                // Single char
                out << (int)cv_convert_double(units->converter.get(), *(char*)buf_ptr);
            } else {
                // All but last dim specified, leaves a char array
                write_escaped_string(out, (const char *) buf_ptr);
                bytes_written = _size ;
            }
            break;
        case TRICK_UNSIGNED_CHARACTER:
            if (_var_info->attr->num_index == _var_info->num_index) {
                // Single char
                out << (unsigned int)cv_convert_double(units->converter.get(), *(unsigned char*)buf_ptr);
            } else {
                // All but last dim specified, leaves a char array
                write_escaped_string(out, (const char *) buf_ptr);
                bytes_written = _size ;
            }
            break;

        case TRICK_WCHAR:{
                if (_var_info->attr->num_index == _var_info->num_index) {
                    out << *(wchar_t *) buf_ptr;
                } else {
                    // convert wide char string char string
                    size_t len = wcs_to_ncs_len((wchar_t *)buf_ptr) + 1 ;

                    char temp_buf[len];
                    wcs_to_ncs((wchar_t *) buf_ptr, temp_buf, len);
                    out << temp_buf;
                    bytes_written = _size ;
                }
            }
            break;

        case TRICK_STRING:
            if ((char *) buf_ptr != NULL) {
                write_escaped_string(out, (const char *) buf_ptr);
                bytes_written = _size ;
            } else {
                out << '\0';
            }
            break;

        case TRICK_WSTRING:
            if ((wchar_t *) buf_ptr != NULL) {
                // convert wide char string char string
                size_t len = wcs_to_ncs_len( (wchar_t *)buf_ptr) + 1 ;

                char temp_buf[len];
                wcs_to_ncs(  (wchar_t *) buf_ptr, temp_buf, len);
                out << temp_buf;
                bytes_written = _size ;
            } else {
                out << '\0';
            }
            break;
        case TRICK_SHORT:
            out << (short)cv_convert_double(units->converter.get(), *(short*)buf_ptr);
            break;

        case TRICK_UNSIGNED_SHORT:
            out << (unsigned short)cv_convert_double(units->converter.get(), *(unsigned short*)buf_ptr);
            break;

        case TRICK_INTEGER:
        case TRICK_ENUMERATED:
            out << (int)cv_convert_double(units->converter.get(), *(int*)buf_ptr);
            break;

        case TRICK_BOOLEAN:
            out << (int)cv_convert_double(units->converter.get(), *(bool*)buf_ptr);
            break;

        case TRICK_BITFIELD:
            out << (GET_BITFIELD(buf_ptr, _var_info->attr->size, _var_info->attr->index[0].start, _var_info->attr->index[0].size));
            break;

        case TRICK_UNSIGNED_BITFIELD:
            out << (GET_UNSIGNED_BITFIELD(buf_ptr, _var_info->attr->size, _var_info->attr->index[0].start, _var_info->attr->index[0].size));
            break;

        case TRICK_UNSIGNED_INTEGER:
            out << (unsigned int)cv_convert_double(units->converter.get(), *(unsigned int*)buf_ptr);
            break;

        case TRICK_LONG: {
            long l = *(long *)buf_ptr;
            if (units->converter.get() != cv_get_trivial())
            {
                l = (long)cv_convert_double(units->converter.get(), l);
            }
            out << l;
            break;
        }

        case TRICK_UNSIGNED_LONG: {
            unsigned long ul = *(unsigned long *)buf_ptr;
            if (units->converter.get() != cv_get_trivial())
            {
                ul = (unsigned long)cv_convert_double(units->converter.get(), ul);
            }
            out << ul;
            break;
        }

        case TRICK_FLOAT:
            out << std::setprecision(8) << cv_convert_float(units->converter.get(), *(float*)buf_ptr);
            break;

        case TRICK_DOUBLE:
            out << std::setprecision(16) << cv_convert_double(units->converter.get(), *(double*)buf_ptr);
            break;

        case TRICK_LONG_LONG: {
            long long ll = *(long long *)buf_ptr;
            if (units->converter.get() != cv_get_trivial())
            {
                ll = (long long)cv_convert_double(units->converter.get(), ll);
            }
            out << ll;
            break;
        }

        case TRICK_UNSIGNED_LONG_LONG: {
            unsigned long long ull = *(unsigned long long *)buf_ptr;
            if (units->converter.get() != cv_get_trivial())
            {
                ull = (unsigned long long)cv_convert_double(units->converter.get(), ull);
            }
            out << ull;
            break;
        }

        case TRICK_NUMBER_OF_TYPES:
            out << "BAD_REF";
            break;

        default:{

            break;
        }
        } // end switch

        if (bytes_written < _size) {
        // if returning an array, continue array as comma separated values
            out << ",";
            buf_ptr += _var_info->attr->size;
        }
    } //end while

    if (units->requested_units != "") {
        if ( _var_info->attr->mods & TRICK_MODS_UNITSDASHDASH ) {
            out << " {--}";
        } else {
            out << " {" << units->requested_units << "}";
        }
    }

    return 0;
}

void Trick::VariableReference::tagAsInvalid () {
    std::string save_name(getName()) ;
    _var_info = make_error_ref(save_name) ;
    _address = _var_info->address ;
}


int Trick::VariableReference::prepareForWrite() {
    if (!_staged) {
        return 1;
    }

    _stage_buffer.swap(_write_buffer);

    _staged = false;
    _write_ready = true;
    return 0;
}

bool Trick::VariableReference::isStaged() const {
    return _staged;
}

bool Trick::VariableReference::isWriteReady() const {
    return _write_ready;
}

int Trick::VariableReference::writeTypeBinary( std::ostream& out, bool byteswap ) const {
    int local_type = _trick_type;
    if (byteswap) {
        local_type = trick_byteswap_int(local_type);
    }
    out.write(const_cast<const char *>(reinterpret_cast<char *>(&local_type)), sizeof(int));

    return 0;
}

int Trick::VariableReference::writeSizeBinary( std::ostream& out, bool byteswap ) const {
    int local_size = _size;
    if (byteswap) {
        local_size = trick_byteswap_int(local_size);
    }
    out.write(const_cast<const char *>(reinterpret_cast<char *>(&local_size)), sizeof(int));

    return 0;
}

int Trick::VariableReference::writeNameBinary( std::ostream& out, bool byteswap ) const {
    std::string name = getName();
    out.write(name.c_str(), name.size());

    return 0;
}

int Trick::VariableReference::writeNameLengthBinary( std::ostream& out, bool byteswap ) const {
    int name_size = getName().size();
    if (byteswap) {
        name_size = trick_byteswap_int(name_size);
    }

    out.write(const_cast<const char *>(reinterpret_cast<char *>(&name_size)), sizeof(int));

    return 0;
}


void Trick::VariableReference::byteswap_var (char * out, char * in) const {
    byteswap_var(out, in, *this);
}


void Trick::VariableReference::byteswap_var (char * out, char * in, const VariableReference& ref) {
    ATTRIBUTES * attr = ref._var_info->attr;
    int array_size = 1;

    // Determine how many elements are in this array if it is an array
    for (int j = 0; j < ref._var_info->attr->num_index; j++) {
        array_size *= attr->index[j].size;
    }

    switch (attr->size) {
        case 1:
            // If these are just characters, no need to byteswap
            for (int j = 0; j < array_size; j++) {
                out[j] = in[j];
            }
            break;

        case 2: {
            short * short_in = reinterpret_cast<short *> (in);
            short * short_out = reinterpret_cast<short *> (out);

            for (int j = 0; j < array_size; j++) {
                short_out[j] = trick_byteswap_short(short_in[j]);
            }
            break;
        }

        case 4: {
            int * int_in = reinterpret_cast<int *> (in);
            int * int_out = reinterpret_cast<int *> (out);

            for (int j = 0; j < array_size; j++) {
                int_out[j] = trick_byteswap_int(int_in[j]);
            }
            break;
        }
        case 8: {
            // We don't actually care if this is double or long, just that it's the right size
            double * double_in = reinterpret_cast<double *> (in);
            double * double_out = reinterpret_cast<double *> (out);

            for (int j = 0; j < array_size; j++) {
                double_out[j] = trick_byteswap_double(double_in[j]);
            }
            break;
        }
    }
}



int Trick::VariableReference::writeValueBinary( std::ostream& out, bool byteswap ) const {
    // local_type is set to the type of the attribute, but if it's a STL type, we need to use the element type.
    TRICK_TYPE local_type = _trick_type;
    if (local_type == TRICK_STL) {
        // If the variable is an STL type, use the STL element type for writing value
        local_type = _var_info->attr->stl_elem_type;
    }

    if ( local_type == TRICK_BITFIELD ) {
        int temp_i = GET_BITFIELD((char*)_write_buffer.data(), _var_info->attr->size, _var_info->attr->index[0].start,
                                  _var_info->attr->index[0].size);
        out.write((char *)(&temp_i), _size);
        return _size;
    }

    if ( local_type == TRICK_UNSIGNED_BITFIELD ) {
        int temp_unsigned = GET_UNSIGNED_BITFIELD((char*)_write_buffer.data(), _var_info->attr->size,
                                                  _var_info->attr->index[0].start, _var_info->attr->index[0].size);
        out.write((char *)(&temp_unsigned), _size);
        return _size;
    }

    if (local_type ==  TRICK_NUMBER_OF_TYPES) {
        // TRICK_NUMBER_OF_TYPES is an error case
        int temp_zero = 0 ;
        out.write((char *)(&temp_zero), _size);
        return _size;
    }

    if (byteswap) {
        std::vector<char> byteswap_buf(_size, 0);
        byteswap_var(byteswap_buf.data(), (char*)_write_buffer.data());
        out.write(byteswap_buf.data(), _size);
    }
    else {
        out.write(_write_buffer.data(), _size);
    }

    return _size;
}

std::ostream& Trick::operator<< (std::ostream& s, const Trick::VariableReference& ref) {
    s << "      \"" << ref.getName() << "\"";
    return s;
}
