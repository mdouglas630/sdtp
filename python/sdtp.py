import struct
import array
import socket

###########################
## CONSTANTES
###########################

# Flags segundo a RFC do TCP
TH_FIN = 0x01 # Finalize
TH_SYN = 0x02 # Synchronize
TH_RST = 0x04 # Reset
TH_PUSH= 0x08 # Push (NAO USADA)
TH_ACK = 0x10 # Acknowledgment
TH_URG = 0x20 # Urgent (NAO USADA)

# Constantes usadas pelo protocolo
IP           = "127.0.0.1" # IP do servidor
PORTA        = 21020       # Porta de conexao com o servidor
MSS          = 255         # Maximo tamanho do payload (\f$2^8-1\f$)
MAXSDTP      = 10 + MSS    # Tamanho do cabecalho + MSS
LOREMSIZE    = 6328        # Total de bytes do arquivo a ser enviado
ALPHA        = 0.125       # Valor inicial do \f$\alpha\f$
BETA         = 0.25        # Valor inicial do \f$\beta\f$
ESTIMATEDRTT = 250         # RTT estimado inicial (ms)
DEVRTT       = 0           # Desvio do RTT estimado inicial (ms)


###########################
## FUNCOES
###########################

# calculo do checksum
def compute_checksum(packet):
    if len(packet) % 2 != 0:
        packet += b'\0'
    res = sum(array.array("H", packet))
    res = (res >> 16) + (res & 0xffff)
    res += res >> 16
    return (~res) & 0xffff


# Funcao que cria um pacote SDTP
def sdtphdr(seqnum,  # Numero de sequencia
            acknum,  # Numero de confirmacao
            datalen, # Tamanho dos dados no segmento
            flags,   # Campo de flags
            window  # Tamanho da janela
            #checksum = 0 # Soma de verificacao (sera calculada)
            ):
        
        # criando o pacote
        packet = struct.pack(
            'HHBBHH', # representacao do tamanho de cada campo
            seqnum,   # H - 16 bits
            acknum,   # H - 16 bits
            datalen,  # B - 8 bits
            flags,    # B - 8 bits
            window,   # H - 16 bits
            0 # checksum # H - 16 bits
            # NOTE: o checksum sera inicialmente zero, depois calculado
        )
    
        # adicionando (concatenando) o valor calculado do checksum
        packet = packet[:8] + struct.pack('H', compute_checksum(packet))

        return packet


# cria uma funcao que aguarda no socket 's' por 't' milissegundos
def recvtimeout(s, t):
    # convertendo de milissegundo para segundo
    s.settimeout(t/1000)
    
    try:
        # Recebendo o pacote do servidor com sucesso
        p, addr = s.recvfrom(MAXSDTP)
    
        # TODO: ver se precisa tratar o erro -1 (erro de recepcao)
    except socket.timeout:
        # Pacote perdido por timeout
        return -2
    
    # retornando o pacote recebido
    return p       


# imprime o pacote
def print_packet(p):
    print("Imprimindo o pacote:")
    print("\tseqnum: %d" % struct.unpack("H", p[0:2]))
    print("\tacknum: %d" % struct.unpack("H", p[2:4]))
    print("\tdatalen: %d" % p[4])
    print("\tflags: 0x%x" % p[5])
    print("\twindow: %d" % struct.unpack("H", p[6:8]))
    print("\tchecksum: 0x%x" % struct.unpack("!H", p[8:10]))


###########################
## CLASSES
###########################

class SDTPPacket:
    # para armazenar os dados a enviar
    data = ""

    def __init__(self,
                 seqnum = 0,   # Numero de sequencia
                 acknum = 0,   # Numero de confirmacao
                 datalen = 0,  # Tamanho dos dados no segmento
                 flags = 0,    # Campo de flags
                 window = 0,   # Tamanho da janela
                 checksum = 0  # Soma de verificacao (sera calculada)
                 ):
        self.seqnum   = seqnum 
        self.acknum   = acknum 
        self.datalen  = datalen
        self.flags    = flags
        self.window   = window
        self.checksum = checksum

    # cria um pacote (struct) a partir dos atributos da classe
    def to_struct(self):
        # criando o pacote
        packet = struct.pack(
            'HHBBHH', # representacao do tamanho de cada campo
            self.seqnum,   # H - 16 bits
            self.acknum,   # H - 16 bits
            self.datalen,  # B - 8 bits
            self.flags,    # B - 8 bits
            self.window,   # H - 16 bits
            self.checksum  # H - 16 bits
            # NOTE: o checksum sera inicialmente zero, depois calculado
        )

        # adicionando (concatenando) os dados (caso possua) e calculando o checksum
        if (self.datalen > 0):
            # adicionando os dados ao final do pacote
            packet = packet \
                        + struct.pack(str(self.datalen)+"s", bytes(self.data,'UTF-8'))
            # computando o checksum e o adicionando ao pacote
            packet = packet[:8] \
                        + struct.pack('H', compute_checksum(packet)) \
                        + struct.pack(str(self.datalen)+"s", bytes(self.data,'UTF-8'))
        else:
            # apenas adicionando o checksum ao pacote
            packet = packet[:8] + struct.pack('H', compute_checksum(packet))

        return packet

    # preenche os atributos da classe a partir de uma struct do pacote
    def from_struct(self, p):
        # NOTE: The result of unpack is a tuple even if it contains exactly one
        # item
        self.seqnum   = struct.unpack("H", p[0:2])[0] # 1st elemento da tupla
        self.acknum   = struct.unpack("H", p[2:4])[0]
        self.datalen  = p[4]
        self.flags    = p[5]
        self.window   = struct.unpack("H", p[6:8])[0]
        self.checksum = struct.unpack("!H", p[8:10])[0]
    
    # imprime os campos do pacote, a partir dos atributos da classe
    def print(self):
        print("Imprimindo o pacote (atributos):")
        print("\tseqnum: %d" % self.seqnum)
        print("\tacknum: %d" % self.acknum)
        print("\tdatalen: %d" % self.datalen)
        print("\tflags: 0x%x" % self.flags)
        print("\twindow: %d" % self.window)
        print("\tchecksum: 0x%x" % self.checksum)
        if self.datalen > 0:
            print("\tdata: %s" % self.data)

    # imprime os campos do pacote, após converte-lo em uma struct (similar à
    # funcao print_packet)
    def print_struct(self):
        p = self.to_struct()
        print("Imprimindo o pacote (struct):")
        print("\tseqnum: %d" % struct.unpack("H", p[0:2]))
        print("\tacknum: %d" % struct.unpack("H", p[2:4]))
        print("\tdatalen: %d" % p[4])
        print("\tflags: 0x%x" % p[5])
        print("\twindow: %d" % struct.unpack("H", p[6:8]))
        print("\tchecksum: 0x%x" % struct.unpack("!H", p[8:10]))
        if self.datalen > 0:
            print("\tdata: %s" % struct.unpack(str(self.datalen)+"s", p[10:(10+self.datalen)]))

