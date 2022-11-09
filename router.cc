#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    DUMMY_CODE(route_prefix, prefix_length, next_hop, interface_num);
    // Your code here.
	struct _route new_route;
	new_route._route_prefix = route_prefix;
	new_route._prefix_length = prefix_length;
	new_route._next_hop = next_hop;
	new_route._interface_num = interface_num;
	_route_table.push_back(new_route);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    DUMMY_CODE(dgram);
    // Your code here.
	if(dgram.header().ttl <= 1) return;
	auto res_route = _route_table.end();
	uint8_t max_prefix_length = 0;
	uint32_t dgram_des = dgram.header().dst;
	for(auto i = _route_table.begin(); i != _route_table.end(); i++){
		uint32_t mask = (*i)._prefix_length ? (0xffffffff << (32 - (*i)._prefix_length)) : 0;
		if((dgram_des & mask) == ((*i)._route_prefix & mask)){
			if((*i)._prefix_length >= max_prefix_length){
				max_prefix_length = (*i)._prefix_length;
				res_route = i;
			}
		}
	}
	if(res_route != _route_table.end()){
		dgram.header().ttl--;
		auto next_hop = (*res_route)._next_hop;
		if(next_hop.has_value()) _interfaces[(*res_route)._interface_num].send_datagram(dgram, next_hop.value());
		else _interfaces[(*res_route)._interface_num].send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
	}
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
