#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
	auto i = _ipaddr_ethernetaddr_memtime_map.find(next_hop_ip);
	//when the Ethernet address of the first hop is known
	if(i != _ipaddr_ethernetaddr_memtime_map.end()){
		//send Ethernet frame directly
		EthernetFrame ethernet_frame;
		ethernet_frame.header().dst = i->second.first;
		ethernet_frame.header().src = _ethernet_address;
		ethernet_frame.header().type = EthernetHeader::TYPE_IPv4;
		ethernet_frame.payload() = dgram.serialize();
		_frames_out.push(ethernet_frame);
	}
    else{
		auto i1 = _ipaddr_arptime_map.find(next_hop_ip);
		if(i1 == _ipaddr_arptime_map.end() || _curr_time - i1->second > 5000){
			//send arp request
			//construct ARPMessage
			ARPMessage arp_msg;
			arp_msg.sender_ethernet_address = _ethernet_address;
			arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
			arp_msg.target_ip_address = next_hop_ip;
			//arp_msg.target_ethernet_address = {};
			arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
			//make frame
			EthernetFrame arp_frame;
			arp_frame.header().dst = ETHERNET_BROADCAST;
			arp_frame.header().src = _ethernet_address;
			arp_frame.header().type = EthernetHeader::TYPE_ARP;
			arp_frame.payload() = arp_msg.serialize();
			_frames_out.push(arp_frame);
			//update _ipaddr_arptime_map
			if(i1 == _ipaddr_arptime_map.end()){
				_ipaddr_arptime_map.insert(make_pair(next_hop_ip, _curr_time));
			}
			else _ipaddr_arptime_map[next_hop_ip] = _curr_time;
			//send datagram to queue
			_waiting_queue.push_back(make_pair(next_hop_ip, dgram));
		}
	}
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    optional<InternetDatagram> ret = nullopt;
	InternetDatagram dgram;
	if(frame.header().type == EthernetHeader::TYPE_IPv4 && frame.header().dst == _ethernet_address){
		if(dgram.parse(frame.payload()) == ParseResult::NoError) ret = dgram;
	}
	else if(frame.header().type == EthernetHeader::TYPE_ARP){
		ARPMessage arp_msg;
		//if arp message can be resolved
		if(arp_msg.parse(frame.payload()) == ParseResult::NoError){
			//remember the relationship between the ip_addr and ethernet_addr
			auto k = _ipaddr_ethernetaddr_memtime_map.find(arp_msg.sender_ip_address);
			if(k != _ipaddr_ethernetaddr_memtime_map.end()) _ipaddr_ethernetaddr_memtime_map[arp_msg.sender_ip_address] = std::pair<EthernetAddress, size_t>(arp_msg.sender_ethernet_address, _curr_time);
			else _ipaddr_ethernetaddr_memtime_map.insert(make_pair(arp_msg.sender_ip_address, make_pair(arp_msg.sender_ethernet_address, _curr_time)));
			//if the arp message requests for our address, reply
			if(arp_msg.target_ip_address == _ip_address.ipv4_numeric() && arp_msg.opcode == ARPMessage::OPCODE_REQUEST){
				ARPMessage arp_re;
				arp_re.sender_ethernet_address = _ethernet_address;
				arp_re.sender_ip_address = _ip_address.ipv4_numeric();
				arp_re.target_ip_address = arp_msg.sender_ip_address;
				arp_re.target_ethernet_address = arp_msg.sender_ethernet_address;
				arp_re.opcode = ARPMessage::OPCODE_REPLY;
				EthernetFrame arp_re_frame;
				arp_re_frame.header().dst = arp_msg.sender_ethernet_address;
				arp_re_frame.header().src = _ethernet_address;
				arp_re_frame.header().type = EthernetHeader::TYPE_ARP;
				arp_re_frame.payload() = arp_re.serialize();
				_frames_out.push(arp_re_frame);
				
			}
			//if the arp message is a reply to our request
			if(arp_msg.target_ethernet_address == _ethernet_address && arp_msg.opcode == ARPMessage::OPCODE_REPLY){
				for(auto i = _waiting_queue.begin(); i != _waiting_queue.end();){
					if(i->first == arp_msg.sender_ip_address){
						EthernetFrame ipv4_frame;
						ipv4_frame.header().dst = arp_msg.sender_ethernet_address;
						ipv4_frame.header().src = _ethernet_address;
						ipv4_frame.header().type = EthernetHeader::TYPE_IPv4;
						ipv4_frame.payload() = (i->second).serialize();
						_frames_out.push(ipv4_frame);
						i = _waiting_queue.erase(i);
					}
					else i++;
				}
			}
		}
		
	}
    return ret;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
	_curr_time += ms_since_last_tick;
	//erase items with time elasped > 30s
	for(auto i = _ipaddr_ethernetaddr_memtime_map.begin(); i != _ipaddr_ethernetaddr_memtime_map.end();){
		if(_curr_time - i->second.second > 30000){
			i = _ipaddr_ethernetaddr_memtime_map.erase(i);
		}
		else{
			i++;
		}
	}
}
