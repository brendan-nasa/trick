#include "trick/VariableServer.hh"

#include "trick/TCPClientListener.hh"
#include "trick/TCPConnection.hh"
#include "trick/UDPConnection.hh"
#include "trick/message_proto.h"
#include "trick/message_type.h"

#include <memory>
#include <stdio.h>

int Trick::VariableServer::create_tcp_socket(const char * address, unsigned short in_port ) {
    // Open a VariableServerListenThread to manage this server

    auto listener = std::make_unique<TCPClientListener>();
    int status = listener->initialize(address, in_port);

    if (status != 0) {
        message_publish(MSG_ERROR, "ERROR: Could not establish additional listen port at address %s and port %d for Variable Server.\n", address, in_port);
        return 0;
    }

    std::string set_address = listener->getHostname();
    int set_port = listener->getPort();

    auto new_listen_thread = std::make_unique<Trick::VariableServerListenThread>(std::move(listener));

    new_listen_thread->copy_cpus(listen_thread.get_cpus()) ;
    new_listen_thread->create_thread() ;
    additional_listen_threads[new_listen_thread->get_pthread_id()] = std::move(new_listen_thread) ;

    message_publish(MSG_INFO, "Created TCP variable server %s: %d\n", set_address.c_str(), set_port);

    return 0 ;
}

int Trick::VariableServer::create_udp_socket(const char * address, unsigned short in_port ) {
    // UDP sockets are created without a listen thread, and represent only 1 session
    // Create a VariableServerSessionThread to manage this session

    auto udp_conn = std::make_unique<UDPConnection>();
    int status = udp_conn->initialize(address, in_port);
    if ( status != 0 ) {
        message_publish(MSG_ERROR, "ERROR: Could not establish UDP port at address %s and port %d for Variable Server.\n", address, in_port);
        return 0;
    }

    std::string set_address = udp_conn->getHostname();
    int set_port = udp_conn->getPort();

    auto owned_vst = std::make_unique<Trick::VariableServerSessionThread>() ;
    owned_vst->set_connection(std::move(udp_conn));
    owned_vst->copy_cpus(listen_thread.get_cpus()) ;

    Trick::VariableServerSessionThread * vst = adopt_vst(std::move(owned_vst)) ;
    vst->create_thread() ;

    message_publish(MSG_INFO, "Created UDP variable server %s: %d\n", set_address.c_str(), set_port);

    return 0 ;
}

int Trick::VariableServer::create_multicast_socket(const char * mcast_address, const char * address, unsigned short in_port ) {

    // Multicast sockets are created without a listen thread, and represent only 1 session
    // Create a VariableServerSessionThread to manage this session

    if (mcast_address == NULL || mcast_address[0] == '\0') {
        message_publish(MSG_ERROR, "Multicast address must be defined.\n");
        return -1;
    }

    auto multicast = std::make_unique<MulticastGroup>();
    message_publish(MSG_INFO, "Created UDP variable server %s: %d\n", address, in_port);

    int status = multicast->initialize_with_receiving(address, mcast_address, in_port);
    if ( status != 0 ) {
        message_publish(MSG_ERROR, "ERROR: Could not establish Multicast port at address %s and port %d for Variable Server.\n", address, in_port);
        return 0;
    }

    std::string set_address = multicast->getHostname();
    int set_port = multicast->getPort();

    auto owned_vst = std::make_unique<Trick::VariableServerSessionThread>() ;
    owned_vst->set_connection(std::move(multicast));
    owned_vst->copy_cpus(listen_thread.get_cpus()) ;

    Trick::VariableServerSessionThread * vst = adopt_vst(std::move(owned_vst)) ;
    vst->create_thread() ;

    message_publish(MSG_INFO, "Multicast variable server output %s:%d\n", mcast_address, set_port) ;

    return 0 ;
}

