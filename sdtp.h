/**
 * @file sdtp.h
 * @brief Arquivo que contem as definicoes essenciais para a implementacao
 * @author Joao Borges
 */
#include <stdint.h>

/// \defgroup flags Flags segundo a RFC do TCP
/// @{
#define TH_FIN  0x01 ///< Finalize
#define TH_SYN  0x02 ///< Synchronize
#define TH_RST  0x04 ///< Reset
#define TH_PUSH 0x08 ///< Push (NAO USADA)
#define TH_ACK  0x10 ///< Acknowledgment
#define TH_URG  0x20 ///< Urgent (NAO USADA)
/// @}

/**
 * Cabecalho SDTP
 */
struct sdtphdr 
{
    uint16_t seqnum;    ///< Numero de sequencia
    uint16_t acknum;    ///< Numero de confirmacao
    uint8_t  datalen;   ///< Tamanho dos dados no segmento
    uint8_t  flags;     ///< Campo de flags
    uint16_t window;    ///< Tamanho da janela
    uint16_t checksum;  ///< Soma de verificacao
};

/**
 * \defgroup constants Constantes usadas pelo protocolo
 */
/// @{
#define PORTA        21020    ///< Porta de conexao com o servidor
#define MSS          255      ///< Maximo tamanho do payload (\f$2^8-1\f$)
#define MAXSDTP      10 + MSS ///< Tamanho do cabecalho + MSS
#define LOREMSIZE    6328     ///< Total de bytes do arquivo a ser enviado
#define ALPHA        0.125    ///< Valor inicial do \f$\alpha\f$
#define BETA         0.25     ///< Valor inicial do \f$\beta\f$
#define ESTIMATEDRTT 250      ///< RTT estimado inicial (ms)
#define DEVRTT       0        ///< Desvio do RTT estimado inicial (ms)
/// @}

/**
 * Timeout para a recepcao de uma mensagem UDP, utilizando a funcao
 * recvfrom, adaptado de Beej's Guide to Network Programming
 *
 * @param s Socket utilizado para a recepcao dos dados
 * @param buf Buffer para armazenar os dados recebidos
 * @param len Quantidade de bytes a receber no buffer
 * @param timeout Tempo a esperar (em milisegundos) ate declarar que a
 * recepcao falhou
 * @param dest Ponteiro para o endereco do cliente recebido
 * @param destlen Tamanho do endereco recebido
 *
 * @return -2 em caso de timeout
 * @return -1 em caso de erro na recepcao
 * @return n, o tamanho de bytes recebidos (pode ser 0)
 *
 * Exemplo de utilizacao no arquivo cliente_sdtp.c
 *
 */
int recvtimeout(int s, char *buf, int len, int timeout, 
        struct sockaddr *dest, int *destlen)
{
    fd_set fds;
    int n;
    struct timeval tv;

    // set up the file descriptor set
    FD_ZERO(&fds);
    FD_SET(s, &fds);

    // set up the struct timeval for the timeout
    tv.tv_sec = (int)timeout/1000;
    tv.tv_usec = (int)(timeout%1000)*1000;

    // wait until timeout or data received
    n = select(s+1, &fds, NULL, NULL, &tv);
    if (n == 0) return -2; // timeout!
    if (n == -1) return -1; // error

    // data must be here, so do a normal recv()
    return recvfrom(s, buf, len , 0, dest, destlen);
}

/**
 * Calcula o checksum de um determinado pacote, seguindo a RFC 1071
 *
 * @param hdr Ponteiro para o inicio dos dados a somar
 * @param count A quantidade de bytes a contabilizar nesta soma
 *
 * @return O valor do checksum contabilizado
 */
uint16_t checksum(void *hdr, int count)
{
    long sum = 0;
    uint16_t *addr = (uint16_t *)hdr;

    while(count > 1)
    {
        sum += *(addr++);
        count -= 2;
    }

    while (sum>>16)
    {
        sum = (sum & 0xffff) + (sum>>16);
    }

    if (count > 0)
        sum += *(uint8_t *)addr;
        
    return (uint16_t)~sum;
}

/**
 * Funcao de ajuda que imprime o conteudo de um pacote STDP na tela
 *
 * @param p Ponteiro para o pacote sdtp
 */
void printpacket(struct sdtphdr *p)
{
    printf("\nImprimindo Pacote (%x)\n",p);
    printf("\tseqnum:   %d\n",p->seqnum);
    printf("\tacknum:   %d\n",p->acknum);
    printf("\tdatalen:  %d\n",p->datalen);
    printf("\tflags:    0x%x\n",p->flags);
    printf("\twindow:   %d\n",p->window);
    printf("\tchecksum: 0x%x\n",p->checksum);
    if (p->datalen)
        printf("\tdata:     %s\n\n",(char *)p+sizeof(struct sdtphdr));
}

