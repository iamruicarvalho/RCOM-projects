# Connect E0 from Tux43 and Tux44 in one the entries of the switch

# 1. configure IPs
$ ifconfig eth0 up
$ ifconfig eth0 <IP>
    - 172.16.40.1/24 for Tux43
    - 172.16.40.254/24 for Tux44
# NOTE: The MAC adress is in the ether field when typing ifconfig
MAC address: 00.21.5a.5a.7b.ea

# 2. ping 
$ ping <IP>
    - 172.16.40.254 in Tux43
    - 172.16.40.1 in Tux44

# 3. ARP tables
$ arp -a

# 4. delete Tux43 entry from ARP table 
$ arp -d 172.16.40.254/24
$ arp -a  # empty

# 5. start to get data packets from wireshark

# 6. In tuxY3, ping tuxY4 for a few seconds
$ ping 172.16.40.254/24

# 7. stop to get data packets from wireshark


