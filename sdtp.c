/**
 * @file sdtp.c
 * @brief Arquivo que contem as implementacoes das funcoes uteis
 * @author Joao Borges
 */
#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>

#include "sdtp.h"

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

