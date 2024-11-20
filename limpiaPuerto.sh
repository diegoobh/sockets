#!/bin/bash

# Verifica que se pase el número de puerto como argumento
if [ "$#" -ne 1 ]; then
    echo "Uso: $0 <puerto>"
    exit 1
fi

PORT=$1

# Encuentra los procesos que usan el puerto
PIDS=$(lsof -t -i :$PORT)

# Verifica si se encontraron procesos
if [ -z "$PIDS" ]; then
    echo "No hay procesos usando el puerto $PORT."
    exit 0
fi

# Muestra los procesos encontrados
echo "Procesos usando el puerto $PORT:"
echo "$PIDS"

# Mata los procesos encontrados
echo "Matando procesos..."
for PID in $PIDS; do
    kill -9 $PID && echo "Proceso $PID terminado."
done

echo "Todos los procesos en el puerto $PORT han sido eliminados."
