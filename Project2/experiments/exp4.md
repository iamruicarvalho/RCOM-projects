# 1. Ligar eth1 do Router à porta 5.1 da régua

# 2. Ligar eth2 to Router ao Switch

# 3. Eliminar as portas default do ether5 do switch e ligar o ether5 à bridge41
/interface bridge port remove [find interface=ether5]
/interface bridge port add bridge=bridge41 interface=ether5

# 4. Trocar o cabo ligado a consola do Switch para o Router MT

# 5. No Tux52, conectar ao router desde o GTK com:
Serial Port: /dev/ttyS0
Baudrate: 115200
Username: admin
Password: <ENTER>

# 6. Resetar as configuraçoes do router
/system reset-configuration

# 7. Configurar ip do Router pela consola do router no GTK do Tux42
/ip address add address=172.16.1.59/24 interface=ether1
/ip address add address=172.16.41.254/24 interface=ether2

# 8. Configurar as rotas default nos Tuxs e no Router:
route add default gw 172.16.41.254 # Tux42
route add default gw 172.16.40.254 # Tux43
route add default gw 172.16.41.254 # Tux44

/ip route add dst-address=172.16.50.0/24 gateway=172.16.51.253  
#Router console
/ip route add dst-address=0.0.0.0/0 gateway=172.16.1.254       
#Router console

# 9. No Tux43, começar captura do Wireshark e fazer ping de todas as interfaces. Todas deverão funcionar. Guardar o resultado obtido:
ping 172.16.50.254 # Figura 1
ping 172.16.51.1   # Figura 2
ping 172.16.51.254 # Figura 3

# 10. No Tux42, desativar o accept_redirects
echo 0 > /proc/sys/net/ipv4/conf/eth0/accept_redirects
echo 0 > /proc/sys/net/ipv4/conf/all/accept_redirects

# 11. Remover a rota que liga Tux52 ao Tux54
route del -net 172.16.40.0 gw 172.16.41.253 netmask 255.255.255.0

# 12. No Tux42, começar captura do wireshark e fazer ping do Tux43. A ligação é estabelecida, usando o Rc como router em vez do Tux44. Comandos:
ping 172.16.50.1 # Figura 4

# 13. Fazendo traceroute, conseguimos verificar o percurso da ligação
traceroute -n 172.16.40.1
traceroute to 172.16.40.1 (172.16.40.1), 30 hops max, 60 byte packets
1  172.16.41.254 (172.16.41.254)  0.200 ms  0.204 ms  0.224 ms
2  172.16.41.253 (172.16.41.253)  0.354 ms  0.345 ms  0.344 ms
3  tux51 (172.16.40.1)  0.596 ms  0.587 ms  0.575 ms

# 14. Adicionar de novo a rota que liga Tux42 ao Tux44
route add -net 172.16.50.0/24 gw 172.16.51.253 

# 15. No Tux42, traceroute para o Tux43
   traceroute -n 172.16.50.1
traceroute to 172.16.50.1 (172.16.50.1), 30 hops max, 60 byte packets
1  172.16.51.253 (172.16.51.253)  0.196 ms  0.180 ms  0.164 ms
2  tux51 (172.16.50.1)  0.414 ms  0.401 ms  0.375 ms

# 16. No Tux42, reativar o accept_redirects
echo 0 > /proc/sys/net/ipv4/conf/eth0/accept_redirects
echo 0 > /proc/sys/net/ipv4/conf/all/accept_redirects

# 17. No Tux43, fazer ping do router do lab I.320 para verificar a ligação
# verify lab router
ping 172.16.1.254 

# 18. Desativar NAT do Router
/ip firewall nat disable 0

# 19. No Tux43, fazer de novo ping do router do lab I.321 para verificar a ligação. Verifica-se que não há ligação:
ping 172.16.1.254

# 20. Reativar Nat do Router
/ip firewall nat enable 0