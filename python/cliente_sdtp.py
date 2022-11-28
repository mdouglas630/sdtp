
import socket

# importando as constantes e funcoes de sdtp.py
from sdtp import *

# criando um pacote SDTP
pout = sdtphdr(0, 0, 0, TH_SYN, 0)

# imprimindo o pacote
print("Pacote enviado:")
print_packet(pout)

# criando um socket UDP
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# enviando o pacote SDTP para o servidor
s.sendto(pout, (IP, PORTA))

# recebendo um pacote pelo socket 's' e aguardando 2 segundos
pin = recvtimeout(s, 2000)

if (pin == -2):
    print("Erro de timeout - reenviar o pacote")
else:
    print("Pacote recebido:")
    print_packet(pin)
    
# references: 
# 1. https://wiki.python.org/moin/UdpCommunication

