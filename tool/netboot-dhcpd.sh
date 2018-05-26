#!/bin/bash
#
# DHCP server
#

#set -x
set -e

    DEFAULT_NETDEV=enp0s25
    DEFAULT_PREFIX=10.0.0
DEFAULT_DHCPD_CONF=netboot-dhcpd.conf

if [ "$1" == "-h" ]; then
	cat <<- EOF
	The script configures and starts dhcpd on the network device NETDEV,
	Therefore, it must be run with root privileges. By default NETDEV is
	$DEFAULT_NETDEV and network PREFIX $DEFAULT_PREFIX. You may change this
	with:

	! sudo NETDEV=eth0 PREFIX=10.0.0 ./netboot-dhcpd.sh

	The script uses DHCPD_CONF for dhcp server configuration. DHCPD_CONF is set
	to $DEFAULT_DHCPD_CONF by default.

	Under Debian/Ubuntu needed tools can be installed as follows

	! sudo apt install isc-dhcp-server iproute2

	You may also have to disable, purge or configure AppArmor if dhcpd
	complains about unreadable configuration files.

	Qemu example:

	! qemu-system-i386 -m 128 -nographic -cdrom var/run/lwip.iso     \\
	                   -net nic,model=e1000                          \\
	                   -net tap,script=no,downscript=no,ifname=tap42

	EOF
	exit 1
fi

if [ "$UID" != "0" ]; then
	echo "Must be run as root... "
	sudo NETDEV="$NETDEV" PREFIX="$PREFIX" DHCPD_CONF="$DHCPD_CONF" $0 "$@"
	exit
fi


#
# Configuration
#

NETDEV=${NETDEV:-$DEFAULT_NETDEV}
PREFIX=${PREFIX:-$DEFAULT_PREFIX}
DHCPD_CONF=${DHCPD_CONF:-$DEFAULT_DHCPD_CONF}

if nmcli -t -f GENERAL.NM-MANAGED dev show enp0s25 | grep -q ':yes'; then
	NETDEV_MANAGED="yes"
else
	NETDEV_MANAGED="no"
fi

nmcli dev set $NETDEV managed no

trap "nmcli dev set $NETDEV managed $NETDEV_MANAGED" INT

ip address flush dev "$NETDEV"
ip address add "$PREFIX.1/24" brd "$PREFIX.255" dev "$NETDEV"
ip link set "$NETDEV" up


#
# DHCP daemon
#

DHCPD="/usr/sbin/dhcpd"
DHCPD_CONF_FILE=$(readlink -e "$DHCPD_CONF")
DHCPD_PID_FILE="/tmp/dhcpd.pid"
DHCPD_LEASE_FILE="/tmp/dhcpd.lease"

echo -n "" > $DHCPD_LEASE_FILE
chown dhcpd:dhcpd $DHCPD_LEASE_FILE

$DHCPD -d -f -cf $DHCPD_CONF_FILE -pf $DHCPD_PID_FILE -lf $DHCPD_LEASE_FILE "$NETDEV"

