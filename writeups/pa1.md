Programming Assignment 1 Writeup
====================

My name: Zhengnan Zhu

My UTORID : zhuzhe25

I collaborated with: nobody

I would like to credit/thank these classmates for their help: nobody

This programming assignment took me about 5 hours to do.

Program Structure and Design of the NetworkInterface:
new structure TimedEvent, which includes:
1. time: record the time when the Timedevent is created
2. message: the infomation about the data and its type (ARP reply, ARP request or IPv4 packet)
3. target_addr: the target ip address that we want to send the message


the class NetworkInterface has the following additional attributes:
1. arp_table: it is a map that stores the MAC address and the time when writing the record with respect to the key (the ip address)
2. ready_queue: a ready-to-be-sent queue as stated in the Assignment description
3. waiting_queue: a queue that stores the waiting frame (e.g. IPv4 packet that is waiting for the MAC address of destination, ARP waiting frame that is waiting for a specific reply, ARP request frame that is waiting for the previous request to be expired)
4. request_table: store the last time we sent the ARP request, the key is the IP address of the destination and the value is the time.

Three helper functions:
1. arp_table_lookup: check if we know the MAC address given destination's IP address
2. request_table_lookup: check if we have sent the request for a specific IP address in 5 seconds
3. create_request_frame: simply create the ARP request message and frame. Fill in the correct information



Implementation Challenges:
look at different .hh files to figure out what is in a specific structure (e.g. EthernetAddress, Address, EthernetFrame, EthernetHeader etc.)

Remaining Bugs:
no bugs, I passed all the test cases.

- Optional: I had unexpected difficulty with: [describe]

- Optional: I think you could make this lab better by: [describe]

- Optional: I was surprised by: [describe]

- Optional: I'm not sure about: [describe]
