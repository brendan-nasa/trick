#include "trick/exec_proto.h"
#include "trick/VariableServer.hh"

int Trick::VariableServer::copy_and_write_top() {

    // Reap finished session threads here: this is a main-thread job, and confining
    // destruction to the main thread is what makes raw registry pointers safe for the
    // checkpoint suspend/resume snapshots.
    reap_retired_threads();
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        for (auto it = var_server_sessions.begin(); it != var_server_sessions.end(); ++it)
        {
            (*it).second->copy_and_write_top(exec_get_frame_count());
        }
    }

    return 0 ;
}
