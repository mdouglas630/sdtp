/**
 * @file cliente_sdtp.c
 * @brief Arquivo que contem uma versao inicial (incompleta) do cliente SDTP
 * @author Joao Borges
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sdtp.h"

//// OBS.: muita coisa ainda falta ser feita, mas seguem algumas
//operacoes que podem ser uteis

int main(int argc, char *argv[])
{
    // buffers para armazenar os pacotes de recepcao (in) e envio (out)
    char buffer_in[MAXSDTP];
    char buffer_out[MAXSDTP];

    // ponteiro para os pacotes sdtp (recepcao e envio)
    struct sdtphdr *pin, *pout;

    // ponteiro para o inicio dos dados do pacote sdtp
    char *data;

    // descritor do socket do servidor
    int meusocket;

    // recebe o tamanho da estrutura sockaddr_in
    int sockettamanho;

    // numero de bytes recebidos pelo socket
    int numbytes;

    // informacoes do servidor
    struct sockaddr_in destinatario;

    if (argc != 3) 
    {
		printf("Erro: uso correto: ./cliente_sdtp ipservidor porta\n");
		return 1;
	}

    // tamanho da estrutura de endereco do socket usado
    sockettamanho = sizeof(struct sockaddr_in);

    // criando o socket
    meusocket = socket(AF_INET, SOCK_DGRAM, 0);

    destinatario.sin_family = AF_INET;

    // ip do servidor - 127.0.0.1 se estiver rodando na sua mesma maquina
    destinatario.sin_addr.s_addr = inet_addr(argv[1]);

    // porta do servidor
    destinatario.sin_port = htons(atoi(argv[2]));
    
    // zerando o resto da estrutura
    memset(&(destinatario.sin_zero), '\0', sizeof(destinatario.sin_zero));
    

    // apontando para o inicio do buffer
    pin  = (struct sdtphdr *)&buffer_in;    // para recepcao
    pout = (struct sdtphdr *)&buffer_out;   // para envio

    // apontando para o inicio dos dados do pacote de envio
    data = (char *)pout + sizeof(struct sdtphdr);

    // preenchendo o cabecalho do pacote
    pout->seqnum   = 0;
    pout->acknum   = 0;
    pout->datalen  = 0;
    pout->flags    = 0;
    pout->window   = 0;
    pout->checksum = 0;

    // tamanho informado pelo cliente dos dados a serem enviados
    int size;

    // 0 - inicio / envia o syn e aguarda pelo syn-ack
    // 1 - recebeu syn-ack / envia o ack
    // 2 - apto a enviar pacotes / 
    // 3 - envia fin quando terminar dados , aguardando ack
    int state = 0;

    // janela informada pelo servidor
    int window; 

    // abrindo o arquivo
    FILE *loremfile = fopen("./lorem_ipsum.txt", "r");


    if (loremfile == NULL)
    {
        printf("erro em abrir o arquivo\n");
        return 0;
    }
    
    // valor de bytes enviados e que se espera confirmacao
    int ackbytes = 0;

    while(1)
    {
        // aqui devem vir os testes de estado
        
        if (state == 0)
        {
            // montar pacote SYN
            pout->seqnum   = 0;
            pout->acknum   = 0;
            pout->datalen  = 0;
            pout->flags    = TH_SYN;
            pout->window   = 0;
            pout->checksum = 0;
            pout->checksum = checksum(
                    (void *)pout, sizeof(struct sdtphdr));
            size = 0;
        }
        else if (state == 1)
        {
            // montar pacote ACK
            pout->seqnum   = 0;
            pout->acknum   = 0;
            pout->datalen  = 0;
            pout->flags    = TH_ACK;
            pout->window   = 0;
            pout->checksum = 0;
            pout->checksum = checksum(
                    (void *)pout, sizeof(struct sdtphdr));
            size = 0;
        }
        else if (state == 2)
        {   
            // enviar os dados
            printf("enviar os dados\n");
    
            // zerando os dados
            memset(data, 0x0, MSS);

            // lendo os dados
            size = fread(data, 1, window, loremfile);
    
            // preenchendo o novo pacote
            pout->seqnum   = ackbytes; // seqnum assume o ack antigo
            pout->acknum   = 0;        // zerando o ack
            pout->datalen  = size;     // define novo tamanho dos dados
            pout->flags    = 0x0;      // zerando as flags
            pout->window   = 0;        // zerando a janela
            pout->checksum = 0;        // zerando o checksum
            // calculando o checksum
            pout->checksum = checksum(
                    (void *)pout, size+sizeof(struct sdtphdr));
    
            printf("meu checksum: %d (%d bytes)\n",pout->checksum, 
                    size+sizeof(struct sdtphdr));

            ackbytes += size;

            // envio do arquivo finalizou, agora deve-se enviar o FIN
            if (size == 0)
            {
                printf("finalizou o envio do arquivo! enviar FIN\n");
                state = 3;
                continue;
            }
        }
        else if (state == 3)
        {
            printf("PACOTE FIN\n");
            
            // montar pacote FIN
            pout->seqnum   = 0;
            pout->acknum   = 0;
            pout->datalen  = 0;
            pout->flags    = TH_FIN;
            pout->window   = 0;
            pout->checksum = 0;
            pout->checksum = checksum(
                    (void *)pout, sizeof(struct sdtphdr));
            size = 0;
        }
        
        // imprimindo pacote
        printf("DEBUG - PACOTE A ENVIAR");
        printpacket(pout);
        
        // testando o envio do pacote para o servidor
        numbytes = sendto(meusocket, pout, size+sizeof(struct sdtphdr), 0,
                 (struct sockaddr *)&destinatario, sockettamanho);

        printf("Cliente: enviou %d bytes para %s:%s\n", 
                numbytes, argv[1], argv[2]);

        // exemplo de utilizacao da funcao de timeout no recvfrom
        // definindo um timeout de 10 segundos
        numbytes = recvtimeout(meusocket, buffer_in, MAXSDTP, 10000,
                (struct sockaddr *)&destinatario, &sockettamanho);

        // verificacao do retorno da funcao
        if (numbytes == -1)
        {
            // error occurred
            perror("recvtimeout");

            // finaliza por causa do erro
            break;
        }
        else if (numbytes == -2)
        {
            // timeout occurred
        }
        else 
        {
            // got some data in buf
            printf("Cliente: recebeu %d bytes da mensagem \"%s\"\n", 
                numbytes, buffer_in);

            printf("DEBUG - PACOTE RECEBIDO");
            printpacket(pin);

            // deve se verificar o checksum do pacote recebido
            if (pin->checksum!= checksum(
                    (void *)pin, numbytes-sizeof(struct sdtphdr)))
            {
                printf("Cliente: checksum invalido\n");
                continue;
            }

            // verificando se o pacote recebido é FIN
            if (pin->flags & TH_FIN)
            {
                printf("Cliente: recebeu FIN\n");
                break;
            }

            // verificando se o pacote recebido é ACK
            if (pin->flags & TH_ACK)
            {
                printf("Cliente: recebeu ACK\n");
                state = 1;
                continue;
            }

            // verificando se o pacote recebido é SYN
            if (pin->flags & TH_SYN)
            {
                printf("Cliente: recebeu SYN\n");
                state = 0;
                continue;
            }

            // deve se verificar o tipo do pacote recebido, se eh de
            // sincronizacao, ou ja sao dados
        }

    }


	close(meusocket);

    return 0;
}
