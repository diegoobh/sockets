# lanzaServidor.sh
# Lanza el servidor que es un daemon y varios clientes
./servidor
./cliente TCP &
./cliente TCP diegobh &
./cliente TCP @nogal.usal.es &
./cliente TCP zxcvb@nogal.usal.es &
./cliente TCP p1777001@nogal.usal.es &
./cliente UDP &
./cliente UDP diegobh &
./cliente UDP @nogal.usal.es &
./cliente UDP zxcvb@nogal.usal.es &
./cliente UDP p1777001@nogal.usal.es &

