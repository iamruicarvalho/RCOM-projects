# 1. Connect eth1 from Tux44 to port 4 of the switch. Configure eth1 from Tux44
> ifconfig eth1 up
> ifconfig eth1 172.16.41.253/24

# 2. Remove the ports to which TUX42 is connected by default, and then add a new one
> /interface bridge port remove [find interface=ether4]
> /interface bridge port add bridge=bridge41 interface=ether4

# 3. Activate ip forwarding and stop ICMP no Tux44
# Ip forwarding t4
echo 1 > /proc/sys/net/ipv4/ip_forward

# Disable ICMP echo ignore broadcast t4
echo 0 > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts

# 4. See MAC and IP addresses in tux44.eth0 and tux44.eth1
ifconfig
MAC address: 00.21.5a.5a.7b.ea --> eth0
MAC address: 00.c0.df.25.1a.f4 --> eth1

# 5. Reconfigure tux43 and tux42 so that each of them can reach the other through tux44
route add -net  172.16.40.0/24 gw 172.16.41.253 # no Tux42
route add -net  172.16.41.0/24 gw 172.16.40.254 # no Tux43

# 6. Verify routes
route -n

# 7. Start capture at tux43

# 8. From tux43, ping the other network interfaces (172.16.40.254, 172.16.41.253, 172.16.41.1) and verify if there is connectivity
ping 172.16.40.254
ping 172.16.41.253
ping 172.16.41.1
#All good, network is correct

# 9. Stop the capture and save the logs

# 10. Start capture of eth0 e eth1 from Tux44 

# 11. Clean ARP tables in all Tux
arp -d 172.16.41.253 #Tux42
arp -d 172.16.40.254 #Tux43
arp -d 172.16.40.1 #Tux44
arp -d 172.16.41.1 #Tux44

# 12. In tux43, ping tux42 for a few seconds.
ping 172.16.41.1

# 13. Stop captures in tux44 and save logs