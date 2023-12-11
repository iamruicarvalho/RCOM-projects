# Connect Tux42_S0 in T3;
# Connect T2 in switch CONSOLE;
# Open the GTK on Tux42 and set the baudrate to 115200;

# reset switch configuration
> admin
> /system reset-configuration

# 1. Connect TUX42_E0 to the switch
$ ifconfig eth0 up
$ ifconfig eth0 172.16.41.1/24

# 2. Create bridges in the swicth
> /interface bridge add name=bridge40
> /interface bridge add name=bridge41

# 3. Remove the ports to which TUX42/43/44 are connected to 
> /interface bridge port remove [find interface =ether1]  # TUX43
> /interface bridge port remove [find interface =ether2]  # TUX44
> /interface bridge port remove [find interface =ether3]  # TUX42

# 4. Add new ports
> /interface bridge port add bridge=bridge40 interface=ether1
> /interface bridge port add bridge=bridge40 interface=ether2 
> /interface bridge port add bridge=bridge41 interface=ether3

# 5. Check the added ports
/interface bridge port print
