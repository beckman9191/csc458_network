#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

size_t total_time = 0;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}


// helper function
bool NetworkInterface::arp_table_lookup( const std::string ip_address ) {
  return arp_table.find(ip_address) != arp_table.end();
}

bool NetworkInterface::request_table_lookup(const std::string ip_address) {
  return request_table.find(ip_address) != request_table.end();
}

EthernetFrame NetworkInterface::create_request_frame(const Address& next_hop) {

  ARPMessage arp_request; // create the ARP request
  arp_request.sender_ip_address = ip_address_.ipv4_numeric();
  arp_request.sender_ethernet_address = ethernet_address_;
  arp_request.target_ip_address = next_hop.ipv4_numeric();
  arp_request.opcode = ARPMessage::OPCODE_REQUEST;

  EthernetFrame request_frame; 
  request_frame.header.type = EthernetHeader::TYPE_ARP;
  request_frame.header.src = ethernet_address_;
  request_frame.header.dst = ETHERNET_BROADCAST;
  request_frame.payload = serialize(arp_request);

  return request_frame;
}


// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // create the ethernet frame
  EthernetFrame eframe;
  // set the type as IPv4 packet
  eframe.header.type = EthernetHeader::TYPE_IPv4; 
  // set the source MAC address
  eframe.header.src = ethernet_address_; 
  // serialize the data
  eframe.payload = serialize(dgram);

  if(arp_table_lookup(next_hop.ip())) { // if we know the MAC address
    // set the destination MAC address
    eframe.header.dst = arp_table[next_hop.ip()].first; 
    
    // put the Ethernet frame in the ready-to-be-sent queue
    ready_queue.push(eframe); 

  } else { // we do not know the MAC address

    //  a boolean value to check if we have sent the ARP request in 5 seconds
    bool sent_request_last_five_sec = false;

    // if we never send a request before, set the last request time to INT_MIN
    if(!request_table_lookup(next_hop.to_string())) { 
      request_table[next_hop.to_string()] = INT_MIN;
    }

    // check if we have sent the request in the last 5 seconds
    if(total_time - request_table[next_hop.to_string()] < 5000) {
      sent_request_last_five_sec = 1; // set the boolean value to 1
    }

    // create a ARP request frame
    EthernetFrame request_frame = create_request_frame(next_hop);

    // if we haven't send the ARP request again in last 5 seconds 
    if(!sent_request_last_five_sec) {
      // put the request frame into the ready-to-be-sent queue
      ready_queue.push(request_frame);

      // inform the table that we send the ARP message next_hop's ip address now
      request_table[next_hop.to_string()] = total_time;

      // create a waiting frame that is waiting for the ARP reply
      EthernetFrame waiting_frame;
      waiting_frame.header.type = EthernetHeader::TYPE_ARP;
      waiting_frame.header.src = ethernet_address_;
      waiting_frame.header.dst = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
      waiting_queue.push(TimedEvent{total_time, waiting_frame, next_hop.ipv4_numeric()});
      
    } else {
      waiting_queue.push(TimedEvent{ULONG_MAX, request_frame, next_hop.ipv4_numeric()});
      
    }
    
    // we need to put the IPv4 packet into the waiting queue and wait for the MAC address of that IP address
    waiting_queue.push(TimedEvent{total_time, eframe, next_hop.ipv4_numeric()});
    
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  
 
  // Check if the packet is destined for this machine or if it is broadcasted to the whole network
  if(frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST) {
    
    return std::nullopt; // discard it since it is not destined to us
  }
 
  // the packet is destined to this machine and its payload an IPv4 packet
  if(frame.header.dst == ethernet_address_ && frame.header.type == EthernetHeader::TYPE_IPv4) {
    InternetDatagram dgram;

    
    if(parse(dgram, frame.payload)) {
      // parse success, we return it
      
      return dgram;
    } else {
      return std::nullopt;
    }
  }

  // the packet is destined to this machine and its payload is an APR packet
  cerr << frame.header.type << endl;
  
  
  if(frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage arp_message;
    
    if(parse(arp_message, frame.payload)) {
      // parse success, we learn the mapping
      arp_table[Address::from_ipv4_numeric(arp_message.sender_ip_address).ip()] = make_pair(arp_message.sender_ethernet_address, total_time);
      
      // if they want our ip address (the ARP request from them)
      if(arp_message.target_ip_address == ip_address_.ipv4_numeric() && arp_message.opcode == ARPMessage::OPCODE_REQUEST) { 
        // Create ARP reply message and fill in the information
        ARPMessage arp_reply;
        arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
        arp_reply.sender_ethernet_address = ethernet_address_;
        arp_reply.target_ip_address = arp_message.sender_ip_address;
        arp_reply.target_ethernet_address = arp_message.sender_ethernet_address;
        arp_reply.opcode = ARPMessage::OPCODE_REPLY;

        // Create frame and fill in the information
        EthernetFrame reply_frame;
        reply_frame.header.src = ethernet_address_;
        reply_frame.header.dst = arp_message.sender_ethernet_address;
        reply_frame.header.type = EthernetHeader::TYPE_ARP;
        reply_frame.payload = serialize(arp_reply);

        // push the frame into the ready-to-be-sent queue
        ready_queue.push(reply_frame);
      
      // we receive the ARP reply from the destination
      } 
      
      else if(arp_message.target_ip_address == ip_address_.ipv4_numeric() && arp_message.opcode == ARPMessage::OPCODE_REPLY) {
        cout << "we received arp reply message!!!!!!!" << endl;
        queue<TimedEvent> tmp_queue;
        while(!waiting_queue.empty()) {
        // fetch the first event in the waiting queue
          TimedEvent curr_event = waiting_queue.front();
        // This packet has been waiting for ARP reply for more than 5 seconds, discard it
          if(curr_event.target_addr == arp_message.sender_ip_address && 
            curr_event.message.header.type == EthernetHeader::TYPE_IPv4) {

            curr_event.message.header.dst = arp_message.sender_ethernet_address;
            ready_queue.push(curr_event.message);

          } else if(curr_event.target_addr == arp_message.sender_ip_address &&
                    curr_event.message.header.type == EthernetHeader::TYPE_ARP) {
            // do nothing, just pop the waiting frame           
          } else {
            tmp_queue.push(curr_event);
          }
          waiting_queue.pop();
        }

        // Put the events in the temp queue back to the waiting queue
        while(!tmp_queue.empty()) {
          const TimedEvent event = tmp_queue.front();
          waiting_queue.push(event);
          tmp_queue.pop();
        }

      }


    } 
  }
  return std::nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // expire any entry learnt more than 30 secs ago
  cout << "DEBUG: tick(" << ms_since_last_tick << ") is called" << endl;
  
  total_time += ms_since_last_tick;
  cout << "DEBUG: After calling tick() total_time is " << total_time << endl;
  
  
  for(auto it = arp_table.begin(), next_it = it; it != arp_table.end(); it = next_it) {

    int duration = total_time - it->second.second;
    ++next_it;
    // expire any entry learnt more than 30 secs ago
    if(duration > 30000) { 
      
      arp_table.erase(it->first); // erase will return next iterator
    } 
  }

  // We need a temp queue to store the events that we want to keep
  queue<TimedEvent> tmp_queue;
  while(!waiting_queue.empty()) {
    // fetch the first event in the waiting queue
    const TimedEvent curr_event = waiting_queue.front();
    string ip_addr = Address::from_ipv4_numeric(curr_event.target_addr).to_string();
    // we need to pull the waiting request and send it to the ready-to-be-sent queue
    if(total_time - request_table[ip_addr] > 5000 && curr_event.message.header.dst == ETHERNET_BROADCAST) {
        ready_queue.push(curr_event.message);
        request_table[ip_addr] = total_time;
        cout << "current time is " << total_time << ", we sent the ARP request again!!!!!!!" << endl;
        
        // EthernetFrame waiting_frame;
        // waiting_frame.header.type = EthernetHeader::TYPE_ARP;
        // waiting_frame.header.src = ethernet_address_;
        // waiting_frame.header.dst = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        // waiting_queue.push(TimedEvent{total_time, waiting_frame, curr_event.target_addr});

    // This packet has been waiting for ARP reply for less than 5 seconds, we keep it
    } else if(total_time - curr_event.time <= 5000) {
      
      tmp_queue.push(curr_event);

    } 
    waiting_queue.pop();
  }

  // Put the events in the temp queue back to the waiting queue
  while(!tmp_queue.empty()) {
    const TimedEvent event = tmp_queue.front();
    waiting_queue.push(event);
    tmp_queue.pop();
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  
  if(ready_queue.empty()) {
    // if there is no packet to be sent, we simply return nothing
    cout << "DEBUG: maybe_send() is called, but nothing to send" << endl;
    return std::nullopt;

  } else { // if there is any packet

    // Fetch the oldest packet from the front of the queue
    EthernetFrame message = ready_queue.front(); 
    cout << "DEBUG: maybe_send() is called" << endl;
    
    cout << message.header.to_string() << endl;
    


    // remove the first packet
    ready_queue.pop(); 
    
    // Return the packet to be sent
    return message;
  }
}
