
#include <iostream>
#include <string.h>

#include "trick/VariableServerSession.hh"
#include "trick/memorymanager_c_intf.h"
#include "trick/exec_proto.h"


// These actually do the copying

int Trick::VariableServerSession::copy_sim_data() {
    return copy_sim_data(_session_variable_view, true);
}

int Trick::VariableServerSession::copy_sim_data(const std::vector<VariableReference *>& given_vars, bool cyclical) {

    if (given_vars.size() == 0) {
        return 0;
    }

    std::unique_lock<std::mutex> lock(_copy_mutex, std::try_to_lock) ;
    if ( lock.owns_lock() ) {
        // Get the simulation time we start this copy
        _time = (double)exec_get_time_tics() / exec_get_time_tic_value() ;
        

        for (VariableReference * curr_var : given_vars ) {
            curr_var->stageValue();
        }
    }

    return 0;
}
