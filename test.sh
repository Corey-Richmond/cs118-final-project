#!/bin/sh
ping -c3 10.0.1.100 > /dev/null && echo Test 1 - ping: Success
ping -c3 10.0.1.1 > /dev/null && echo Test 2 - ping: Success
ping -c3 192.168.2.2 > /dev/null && echo Test 3 - ping: Success
ping -c3 172.64.3.10 > /dev/null && echo Test 4 - ping: Success
traceroute -m4 10.0.1.100 > /dev/null && echo Test 5 - traceroute: Success
traceroute -m4 10.0.1.1 > /dev/null && echo Test 6 - traceroute: Success
traceroute -m4 192.168.2.2 > /dev/null && echo Test 7 - traceroute: Success
traceroute -m4 172.64.3.10 > /dev/null && echo Test 8 - traceroute: Success
wget --quiet http://192.168.2.2 > /dev/null && echo Test 9 - wget: Success
wget --quiet http://172.64.3.10 > /dev/null && echo Test 10 - wget: Success
