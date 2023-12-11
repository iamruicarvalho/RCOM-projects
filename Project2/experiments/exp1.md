# Connect E0 from Tux53 and Tux54 in one the entries of the switch

# 1. configure IPs
$ ifconfig eth0 up
$ ifconfig eth0 <IP>
    - 172.16.40.1/24 for Tux43
    - 172.16.40.254/24 for Tux44
# NOTE: The MAC adress is in the ether field when typing ifconfig

# 2. ping 
$ ping <IP>
    - 172.16.40.254 for Tux43
    - 172.16.40.1 for Tux44

# 3. ARP tables
$ arp -a

# 4. delete Tux43 entry from ARP table 
$ arp -d 172.16.40.254/24
$ arp -a  # empty

# 5. start to get data packets from wireshark

# 6. In tuxY3, ping tuxY4 for a few seconds
$ ping 172.16.40.254/24

# 7. stop to get data packets from wireshark


