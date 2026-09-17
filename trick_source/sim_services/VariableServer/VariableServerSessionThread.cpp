
#include <iostream>
#include <stdlib.h>
#include "trick/VariableServerSessionThread.hh"
#include "trick/exec_proto.h"
#include "trick/message_proto.h"
#include "trick/message_type.h"
#include "trick/TrickConstant.hh"
#include "trick/UDPConnection.hh"
#include "trick/TCPConnection.hh"


Trick::VariableServer * Trick::VariableServerSessionThread::_vs = NULL ;

static int instance_num = 0;

Trick::VariableServerSessionThread::VariableServerSessionThread()
    : VariableServerSessionThread(std::make_unique<VariableServerSession>())
{
}

Trick::VariableServerSessionThread::VariableServerSessionThread(std::unique_ptr<VariableServerSession> session)
    : Trick::SysThread(std::string("VarServer" + std::to_string(instance_num++)))
    , _debug(0)
    , _session(std::move(session))
    , _saved_pause_cmd(false)
{
    _connection_status = CONNECTION_PENDING ;



    cancellable = false;
}

Trick::VariableServerSessionThread::~VariableServerSessionThread() {
    cleanup();
}

std::ostream& Trick::operator<< (std::ostream& s, Trick::VariableServerSessionThread& vst) {
    // Write a JSON representation of a Trick::VariableServerSessionThread to an ostream.
    s << "  \"connection\":{\n";
    s << "    \"client_tag\":\"" << vst._connection->getClientTag() << "\",\n";

    s << "    \"client_IP_address\":\"" << vst._connection->getClientHostname() << "\",\n";
    s << "    \"client_port\":\"" << vst._connection->getClientPort() << "\",\n";

    {
        std::lock_guard<std::mutex> lock(vst._connection_status_mutex);
        if (vst._connection_status == CONNECTION_SUCCESS)
        {
            s << *(vst._session);
        }
    }

    s << "  }" << std::endl;
    return s;
}

void Trick::VariableServerSessionThread::set_vs_ptr(Trick::VariableServer * in_vs) {
    _vs = in_vs ;
}

Trick::VariableServer * Trick::VariableServerSessionThread::get_vs() {
    return _vs ;
}

void Trick::VariableServerSessionThread::set_client_tag(std::string tag) {
    _connection->setClientTag(tag);
}

void Trick::VariableServerSessionThread::set_connection(std::unique_ptr<Trick::ClientConnection> in_connection)
{
    _connection = std::move(in_connection);
}

Trick::ConnectionStatus Trick::VariableServerSessionThread::wait_for_accept() {
    {
        std::unique_lock<std::mutex> lock(_connection_status_mutex);
        _connection_status_cv.wait(lock, [this] { return _connection_status != CONNECTION_PENDING; });
    }

    return _connection_status;
}

// Gets called from the main thread as a job
void Trick::VariableServerSessionThread::preload_checkpoint() {
    // Stop variable server processing at the top of the processing loop.
    //
    // A session that exited before acknowledging has nothing left to suspend: its loop is
    // gone and cleanup() has already released the session and connection. Returning here
    // rather than waiting is what keeps a checkpoint reload racing a client disconnect from
    // hanging, and it also avoids touching a session that is being torn down.
    if (!force_thread_to_pause())
    {
        return;
    }

    // Make sure that the _session has been initialized
    std::lock_guard<std::mutex> lock(_connection_status_mutex);
    if (_connection_status == CONNECTION_SUCCESS) {
        // Let the thread complete any data copying it has to do and then suspend data
        // copying until the checkpoint is reloaded. The lock releases at the end of this
        // scope, including if disconnect_references() throws.
        std::unique_lock<std::mutex> copy_lock = _session->acquire_copy_lock();

        // Save the pause state of this thread.
        _saved_pause_cmd = _session->get_pause();

        // Disallow data writing.
        _session->set_pause(true);

        // Temporarily "disconnect" the variable references from Trick Managed Memory
        // by tagging each as a "bad reference".
        _session->disconnect_references();
    }
}

// Gets called from the main thread as a job
void Trick::VariableServerSessionThread::restart() {
    {
        // Held across the whole body so this cannot interleave with cleanup(), which takes
        // the same lock to release the session and connection. Without that, a session that
        // began tearing down after the resume snapshot was taken could have its connection
        // reset out from under the restart below.
        std::lock_guard<std::mutex> lock(_connection_status_mutex);

        // Nothing to resume for a session that is on its way out.
        if (_connection == nullptr || _session == nullptr)
        {
            return;
        }

        // Set the pause state of this thread back to its "pre-checkpoint reload" state.
        _connection->restart();

        if (_connection_status == CONNECTION_SUCCESS)
        {
            _session->set_pause(_saved_pause_cmd);
        }
    }

    // Outside the lock: unpause_thread() takes _restart_pause_mutex, and preload_checkpoint()
    // acquires these two in the opposite order.
    unpause_thread();
}

void Trick::VariableServerSessionThread::cleanup() {
    // Same lock as restart(), so teardown and a concurrent checkpoint resume cannot
    // interleave. cleanup() runs from exit_var_thread() and again from the destructor, so
    // it has to tolerate being called twice and before a connection was ever set.
    std::lock_guard<std::mutex> lock(_connection_status_mutex);
    // Destroy the session before the connection it borrows. The base session destructor
    // does not touch the connection, but an injected subclass's might, and it should not
    // find a dangling pointer.
    _session.reset();

    if (_connection != nullptr)
    {
        _connection->disconnect();
        _connection.reset();
    }
}

