#include "trick/VariableServer.hh"
#include "trick/exec_proto.h"

int Trick::VariableServer::copy_and_write_freeze() {
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        for (auto it = var_server_sessions.begin(); it != var_server_sessions.end(); ++it)
        {
            (*it).second->copy_and_write_freeze(exec_get_freeze_frame_count());
        }
    }

    return 0 ;
}
