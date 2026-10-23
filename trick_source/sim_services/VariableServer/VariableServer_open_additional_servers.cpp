
#include <cstdio>
#include "trick/VariableServer.hh"
#include "trick/message_proto.h"
#include "trick/message_type.h"

#include "trick/TCPConnection.hh"
#include "trick/UDPConnection.hh"
#include "trick/MulticastGroup.hh"
#include "trick/TCPClientListener.hh"

namespace {

// UDP sessions have no peer until a datagram arrives, so the allowlist cannot be checked when
// the session starts. These connections check every datagram's sender instead.
bool sender_allowed(Trick::VariableServer * vs, const std::string& sender_ip) {
    if (vs->get_bypass_ip_check() || vs->check_ip(sender_ip)) {
        return true;
    }
    std::string err_msg = "ILLEGAL IP CONNECTION ATTEMPTED: " + sender_ip + "\n";
    message_publish(MSG_ERROR, "%s", err_msg.c_str());
    return false;
}

class AllowlistedUDPConnection : public Trick::UDPConnection {
    public:
        explicit AllowlistedUDPConnection(Trick::VariableServer * vs) : _vs(vs) {}
    protected:
        bool acceptSender(const std::string& sender_ip) override { return sender_allowed(_vs, sender_ip); }
    private:
        Trick::VariableServer * _vs;
};

class AllowlistedMulticastGroup : public Trick::MulticastGroup {
    public:
        explicit AllowlistedMulticastGroup(Trick::VariableServer * vs) : _vs(vs) {}
    protected:
        bool acceptSender(const std::string& sender_ip) override { return sender_allowed(_vs, sender_ip); }
    private:
        Trick::VariableServer * _vs;
};

}

int Trick::VariableServer::create_tcp_socket(const char * address, unsigned short in_port ) {
    // Open a VariableServerListenThread to manage this server

    TCPClientListener * listener = new TCPClientListener();
    int status = listener->initialize(address, in_port);

    if (status != 0) {
        message_publish(MSG_ERROR, "ERROR: Could not establish additional listen port at address %s and port %d for Variable Server.\n", address, in_port);
        delete listener;
        return 0;
    }

    std::string set_address = listener->getHostname();
    int set_port = listener->getPort();

    Trick::VariableServerListenThread * new_listen_thread = new Trick::VariableServerListenThread(listener) ;

    new_listen_thread->copy_cpus(listen_thread.get_cpus()) ;
    new_listen_thread->create_thread() ;
    additional_listen_threads[new_listen_thread->get_pthread_id()] = new_listen_thread ;

    message_publish(MSG_INFO, "Created TCP variable server %s: %d\n", set_address.c_str(), set_port);

    return 0 ;
}

int Trick::VariableServer::create_udp_socket(const char * address, unsigned short in_port ) {
    // UDP sockets are created without a listen thread, and represent only 1 session
    // Create a VariableServerSessionThread to manage this session

    UDPConnection * udp_conn = new AllowlistedUDPConnection(this);
    int status = udp_conn->initialize(address, in_port);
    if ( status != 0 ) {
        message_publish(MSG_ERROR, "ERROR: Could not establish UDP port at address %s and port %d for Variable Server.\n", address, in_port);
        delete udp_conn;
        return 0;
    }

    std::string set_address = udp_conn->getHostname();
    int set_port = udp_conn->getPort();

    Trick::VariableServerSessionThread * vst = new Trick::VariableServerSessionThread() ;

    vst->set_connection(udp_conn);
    vst->copy_cpus(listen_thread.get_cpus()) ;
    vst->create_thread() ;

    message_publish(MSG_INFO, "Created UDP variable server %s: %d\n", set_address.c_str(), set_port);

    return 0 ;
}

int Trick::VariableServer::create_multicast_socket(const char * mcast_address, const char * address, unsigned short in_port ) {

    // Multicast sockets are created without a listen thread, and represent only 1 session
    // Create a VariableServerSessionThread to manage this session

    if (mcast_address == nullptr || mcast_address[0] == '\0') {
        message_publish(MSG_ERROR, "Multicast address must be defined.\n");
        return -1;
    }

    MulticastGroup * multicast = new AllowlistedMulticastGroup(this);
    message_publish(MSG_INFO, "Created UDP variable server %s: %d\n", address, in_port);

    int status = multicast->initialize_with_receiving(address, mcast_address, in_port);
    if ( status != 0 ) {
        message_publish(MSG_ERROR, "ERROR: Could not establish Multicast port at address %s and port %d for Variable Server.\n", address, in_port);
        delete multicast;
        return 0;
    }

    std::string set_address = multicast->getHostname();
    int set_port = multicast->getPort();

    Trick::VariableServerSessionThread * vst = new Trick::VariableServerSessionThread() ;

    vst->set_connection(multicast);
    vst->copy_cpus(listen_thread.get_cpus()) ;
    vst->create_thread() ;

    message_publish(MSG_INFO, "Multicast variable server output %s:%d\n", mcast_address, set_port) ;

    return 0 ;
}

