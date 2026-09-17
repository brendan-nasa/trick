
#include "trick/VariableServer.hh"

#include "trick/Message_proto.hh"
#include "trick/message_proto.h"
#include "trick/tc_proto.h"

#include <algorithm>
#include <iostream>
#include <netdb.h>

Trick::VariableServer * the_vs ;

Trick::VariableServer::VariableServer()
    : enabled(false)
    , info_msg(false)
    , log(false)
    , allow_connections(false)
    , bypass_ip_check(false)
{
    the_vs = this ;
}

Trick::VariableServer::~VariableServer() {
    // Join what we own before destroying it. A thread that has not finished is released
    // rather than joined: blocking in a destructor would be worse than the leak, and it is
    // the same bargain abandon_thread() already makes.
    reap_retired_threads();

    std::vector<std::unique_ptr<VariableServerSessionThread>> remaining;
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        for (auto& entry : var_server_threads) {
            remaining.push_back(std::move(entry.second));
        }
        var_server_threads.clear();
        std::move(retired_threads.begin(), retired_threads.end(), std::back_inserter(remaining));
        retired_threads.clear();
        std::move(pending_threads.begin(), pending_threads.end(), std::back_inserter(remaining));
        pending_threads.clear();
    }

    for (auto& thread : remaining) {
        if (thread->thread_has_exited() && thread->get_pthread_id() != 0) {
            thread->join_thread();
        } else if (!thread->thread_has_exited()) {
            (void) thread.release();
        }
    }

    the_vs = NULL;
}

void Trick::VariableServer::shutdownConnections() {
    listen_thread.shutdownConnections();
}

std::ostream& Trick::operator<< (std::ostream& s, Trick::VariableServer& vs) {
    std::map < pthread_t , std::unique_ptr<VariableServerSessionThread> >::iterator it ;

    s << "{\"variable_server_connections\":[\n";
    int count = 0;
    int n_connections = (int)vs.var_server_threads.size();
    for ( it = vs.var_server_threads.begin() ; it != vs.var_server_threads.end() ; ++it ) {
        s << "{\n";
        s << *((*it).second);
        s << "}";
        if ((n_connections-count)>1) {
            s << "," ;
        }
        s << "\n";
        count ++;
    }
    s << "]}" << std::endl;
    return s;
}

bool Trick::VariableServer::get_enabled() {
    return enabled ;
}

void Trick::VariableServer::set_enabled(bool on_off) {
    if (!enabled && on_off)
    {
        message_publish(MSG_INFO,
            "Trick VariableServer: Enabling the Variable Server. See Trick documentation for associated security "
            "concerns.\n");
    }

    enabled = on_off ;
}

bool Trick::VariableServer::get_info_msg() {
    return info_msg ;
}

void Trick::VariableServer::set_var_server_info_msg_on() {
    info_msg = true;
    for ( auto& session_it : var_server_sessions ) {
        session_it.second->set_info_message(info_msg);
    }
}

void Trick::VariableServer::set_var_server_info_msg_off() {
    info_msg = false;
    for ( auto& session_it : var_server_sessions ) {
        session_it.second->set_info_message(info_msg);
    }
}

bool Trick::VariableServer::get_log() {
    return log ;
}

bool Trick::VariableServer::get_session_log() {
    return session_log ;
}

void Trick::VariableServer::set_var_server_log_on() {
    log = true;
    // turn log on for all current vs clients
    for ( auto& session_it : var_server_sessions ) {
        session_it.second->set_log(true);
    }
}

void Trick::VariableServer::set_var_server_log_off() {
    log = false;
    // turn log off for all current vs clients
    for ( auto& session_it : var_server_sessions ) {
        session_it.second->set_log(false);
    }
}

void Trick::VariableServer::set_var_server_session_log_on() {
    session_log = true;
    // turn log on for all current vs clients
    for ( auto& session_it : var_server_sessions ) {
        session_it.second->set_session_log(true);
    }
}

void Trick::VariableServer::set_var_server_session_log_off() {
    session_log = false;
    // turn log off for all current vs clients
    for ( auto& session_it : var_server_sessions ) {
        session_it.second->set_session_log(false);
    }
}

const char * Trick::VariableServer::get_hostname() {
    return listen_thread.get_hostname();
}

Trick::VariableServerListenThread & Trick::VariableServer::get_listen_thread() {
    return listen_thread ;
}

Trick::VariableServerSessionThread * Trick::VariableServer::adopt_vst(
    std::unique_ptr<VariableServerSessionThread> in_vst) {
    VariableServerSessionThread * observer = in_vst.get() ;
    std::lock_guard<std::mutex> lock(map_mutex);
    pending_threads.push_back(std::move(in_vst)) ;
    return observer ;
}

void Trick::VariableServer::add_vst(pthread_t in_thread_id, VariableServerSessionThread * in_vst) {
    std::lock_guard<std::mutex> lock(map_mutex);
    // Move this thread from the pending list to its keyed entry. Ownership never leaves
    // the variable server, so registering cannot fail part-way and strand the object.
    auto pending = std::find_if(pending_threads.begin(), pending_threads.end(),
                                [in_vst](const std::unique_ptr<VariableServerSessionThread>& candidate) {
                                    return candidate.get() == in_vst ;
                                }) ;
    if (pending != pending_threads.end()) {
        var_server_threads[in_thread_id] = std::move(*pending) ;
        pending_threads.erase(pending) ;
    }
}

void Trick::VariableServer::reap_retired_threads() {
    std::vector<std::unique_ptr<VariableServerSessionThread>> reapable ;
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        auto finished = std::partition(retired_threads.begin(), retired_threads.end(),
                                       [](const std::unique_ptr<VariableServerSessionThread>& thread) {
                                           return !thread->thread_has_exited() ;
                                       }) ;
        std::move(finished, retired_threads.end(), std::back_inserter(reapable)) ;
        retired_threads.erase(finished, retired_threads.end()) ;
    }

    // Join outside map_mutex: a thread on its way out takes it in delete_session().
    for (auto& thread : reapable) {
        if (thread->get_pthread_id() == 0) {
            // Abandoned by shutdown() and possibly still running. Destroying it would pull
            // the ground out from under it, so leak it deliberately.
            (void) thread.release() ;
        } else {
            thread->join_thread() ;
        }
    }
}

void Trick::VariableServer::add_session(pthread_t in_thread_id, VariableServerSession * in_session) {
    std::lock_guard<std::mutex> lock(map_mutex);
    var_server_sessions[in_thread_id] = in_session ;
}

Trick::VariableServerSessionThread * Trick::VariableServer::get_vst(pthread_t thread_id) {
    Trick::VariableServerSessionThread * ret = NULL ;
    std::lock_guard<std::mutex> lock(map_mutex);
    auto it = var_server_threads.find(thread_id) ;
    if ( it != var_server_threads.end() ) {
        ret = (*it).second.get() ;
    }
    return ret ;
}

Trick::VariableServerSession * Trick::VariableServer::get_session(pthread_t thread_id) {
    Trick::VariableServerSession * ret = NULL ;
    std::lock_guard<std::mutex> lock(map_mutex);
    auto it = var_server_sessions.find(thread_id) ;
    if ( it != var_server_sessions.end() ) {
        ret = (*it).second ;
    }
    return ret ;
}

void Trick::VariableServer::delete_vst(pthread_t thread_id) {
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        auto it = var_server_threads.find(thread_id) ;
        if ( it != var_server_threads.end() ) {
            // Retire rather than destroy: this runs on the exiting thread itself, which is
            // still executing inside the object. The main thread reaps it after joining.
            retired_threads.push_back(std::move(it->second)) ;
            var_server_threads.erase(it) ;
        }
        if ( ! var_server_threads.empty() ) {
            return ;
        }
    }
    // The last session has deregistered; release anyone waiting in shutdown().
    all_sessions_gone.notify_all() ;
}

void Trick::VariableServer::delete_session(pthread_t thread_id) {
    std::lock_guard<std::mutex> lock(map_mutex);
    var_server_sessions.erase(thread_id) ;
}

void Trick::VariableServer::set_copy_data_job( Trick::JobData * in_job ) {
    copy_data_job = in_job ;
}

void Trick::VariableServer::set_copy_and_write_freeze_job( Trick::JobData * in_job ) {
    copy_and_write_freeze_job = in_job ;
}

bool Trick::VariableServer::set_allow_connections(const bool& b)
{
    if (b)
    {
        message_publish(MSG_WARNING, "Trick VariableServer: Enabling Variable Server Connectivity\n");
        add_ip("127.0.0.1");
    }

    return allow_connections = b;
}

bool Trick::VariableServer::get_allow_connections() { return allow_connections; }

bool Trick::VariableServer::set_bypass_ip_check(const bool& b)
{
    if (b)
    {
        message_publish(MSG_WARNING,
            "Trick VariableServer:\nBYPASSING IP SECURITY CHECK.\nANYONE ON THE NETWORK CAN CONNECT TO YOUR PYTHON "
            "INTERPRETER!\nYOU BETTER KNOW WHAT YOU'RE DOING!\n");
    }

    return bypass_ip_check = b;
}

bool Trick::VariableServer::get_bypass_ip_check() { return bypass_ip_check; }

const std::set<std::string>& Trick::VariableServer::get_ip_allowlist() { return ip_allowlist; }

void Trick::VariableServer::add_ip(const std::string& ip)
{
    std::string msg = "Trick VariableServer: Adding " + ip + " to the allowlist.\n";
    message_publish(MSG_INFO, msg.c_str());

    ip_allowlist.insert(ip);
}

void Trick::VariableServer::remove_ip(const std::string& ip)
{
    std::string msg = "Trick VariableServer: Removing " + ip + " from the allowlist.\n";
    message_publish(MSG_INFO, msg.c_str());

    ip_allowlist.erase(ip);
}

bool Trick::VariableServer::check_ip(const std::string& ip)
{
    bool valid = false;
    for (std::set<std::string>::iterator it = ip_allowlist.begin(); it != ip_allowlist.end(); ++it)
    {
        std::string entry = *it;
        size_t slash;

        struct in_addr addr;
        if (inet_pton(AF_INET, ip.c_str(), &addr) != 1)
        {
            return false;
        }
        uint32_t ipVal = ntohl(addr.s_addr);

        if ((slash = entry.find('/')) != std::string::npos)
        {
            std::string network = entry.substr(0, slash);
            int prefix          = std::stoi(entry.substr(slash + 1));

            if (inet_pton(AF_INET, network.c_str(), &addr) != 1)
            {
                continue;
            }
            uint32_t netVal = ntohl(addr.s_addr);

            uint32_t mask = (prefix == 0) ? 0 : (~0u << (32 - prefix));

            valid = (ipVal & mask) == (netVal & mask);
        }
        else
        {
            valid = (ip == entry);
        }

        if (valid)
            break;
    }

    return valid;
}

void Trick::VariableServer::resolve_hostname()
{
    if (enabled)
    {
        // Add your network IPs to the allowlist
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) != 0)
        {
            return;
        }

        struct addrinfo hints { }, *res, *p;
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        int status = getaddrinfo(hostname, nullptr, &hints, &res);
        if (status != 0)
        {
            return;
        }

        char ipstr[INET6_ADDRSTRLEN];
        for (p = res; p != nullptr; p = p->ai_next)
        {
            if (p->ai_family == AF_INET)
            {
                struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
                inet_ntop(p->ai_family, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
                add_ip(ipstr);
            }
        }

        freeaddrinfo(res);
    }
}
