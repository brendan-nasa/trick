
#include "trick/VariableServer.hh"
#include "trick/message_proto.h"
#include "trick/message_type.h"
#include "trick/tc_proto.h"

int Trick::VariableServer::restart() {
    listen_thread.restart() ;
    if ( listen_thread.get_pthread_id() == 0 ) {
        listen_thread.create_thread() ;
    }

    for (const auto& listen_it : additional_listen_threads) {
        listen_it.second->restart();
        if ( listen_it.second->get_pthread_id() == 0 ) {
            listen_it.second->create_thread() ;
        }
    }
    return 0 ;
}

// =====================================================================================
// The following two functions work together to allow a MemoryManager (ASCII) checkpoint
// to be reloaded while a variable server client is connected.
// =====================================================================================

// Suspend variable server processing prior to reloading a checkpoint.
int Trick::VariableServer::suspendPreCheckpointReload() {

    // Pause listening on all listening threads
    listen_thread.pause_listening() ;
    for (const auto& listen_it : additional_listen_threads) {
        listen_it.second->pause_listening();
    }

    // Suspend session threads.
    //
    // The thread pointers are collected under map_mutex but preload_checkpoint() is called
    // without it held. preload_checkpoint() blocks in force_thread_to_pause() waiting for
    // the session thread to reach test_pause(); if that thread is instead on its way out it
    // runs exit_var_thread(), which takes map_mutex in delete_session()/delete_vst(). Holding
    // map_mutex across the wait deadlocks the two. VariableServer::shutdown() documents and
    // avoids the same trap.
    std::vector<VariableServerSessionThread*> sessions;
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        for (const auto& vst_it : var_server_threads)
        {
            sessions.push_back(vst_it.second.get());
        }
    }

    for (auto* vst : sessions)
    {
        vst->preload_checkpoint();
    }

    return 0;
}

// Resume variable server processing after reloading a MemoryManager (ASCII) checkpoint.
int Trick::VariableServer::resumePostCheckpointReload() {

    // Resume all session threads.
    //
    // Snapshot under the lock and restart outside it, mirroring the suspend path. Holding
    // map_mutex across restart() made the thread_has_exited() guard inside it useless: an
    // exiting session blocks in delete_session() waiting for this very lock, so it could
    // never reach the state that makes the guard fire. Raw pointers are safe here because
    // session threads are destroyed only by reap_retired_threads(), on this same thread.
    std::vector<VariableServerSessionThread*> sessions;
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        for (const auto& vst_it : var_server_threads)
        {
            sessions.push_back(vst_it.second.get()) ;
        }
    }

    for (auto* vst : sessions) {
        vst->restart() ;
    }

    // Restart listening on all listening threads
    listen_thread.restart_listening() ;
    for (const auto& listen_it : additional_listen_threads) {
        listen_it.second->restart_listening();
    }

    return 0;
}
