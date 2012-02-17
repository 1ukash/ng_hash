#!/bin/sh

kldload ng_ether
kldload ./ng_hash.ko

 if [ -z "$1" ]; then
        echo "HASH no interface name specified."
		exit 1
 fi

ifconfig ${1} up
ngctl mkpeer ${1}: hash lower A
ngctl name ${1}:lower HASH
ngctl msg ${1}: setpromisc 1
ngctl msg ${1}: setautosrc 0

 if [ -z "$2" ]; then
		exit 0
 fi

ifconfig ${2} up
ngctl connect HASH: ${2}: B lower
ngctl msg ${2}: setpromisc 1
ngctl msg ${2}: setautosrc 0 

 if [ -z "$3" ]; then
		exit 0
 fi

ifconfig ${3} up
ngctl connect HASH: ${3}: C lower
ngctl msg ${3}: setpromisc 1
ngctl msg ${3}: setautosrc 0 

 if [ -z "$4" ]; then
		exit 0
 fi

ifconfig ${4} up
ngctl connect HASH: ${4}: D lower
ngctl msg ${4}: setpromisc 1
ngctl msg ${4}: setautosrc 0 




