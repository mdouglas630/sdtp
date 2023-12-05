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
            //
            // falta fazer
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
            pout->checksum = checksum(
                    (void *)pout, sizeof(struct sdtphdr) + size); // calculando o checksum

            // verificando se chegou ao final do arquivo
            if (size < window)
            {
                // mudando o estado para 3
                state = 3;
                // setando a flag FIN
                pout->flags |= TH_FIN;
            }

            // enviando o pacote
            sendto(meusocket, buffer_out, sizeof(struct sdtphdr) + size, 0,
                    (struct sockaddr *)&destinatario, sockettamanho);

            // recebendo a confirmacao
            numbytes = recvtimeout(meusocket, buffer_in, MAXSDTP, 1000,
                    (struct sockaddr *)&destinatario, &sockettamanho);

            // verificando se houve timeout ou erro
            if (numbytes <= 0)
            {
                // reenviando o pacote
                continue;
            }

            // verificando se o pacote recebido tem a flag ACK e o numero de sequencia esperado
            if ((pin->flags & TH_ACK) && (pin->acknum == ackbytes + size))
            {
                // atualizando o numero de bytes confirmados
                ackbytes += size;
                // atualizando a janela
                window = pin->window;
            }
            else
            {
                // reenviando o pacote
                continue;
            }
        }
        else if (state == 3)
        {
            // enviar o pacote FIN
            printf("enviar o pacote FIN\n");

            // preenchendo o novo pacote
            pout->seqnum   = ackbytes; // seqnum assume o ack antigo
            pout->acknum   = 0;        // zerando o ack
            pout->datalen  = 0;        // zerando o tamanho dos dados
            pout->flags    = TH_FIN;   // setando a flag FIN
            pout->window   = 0;        // zerando a janela
            pout->checksum = 0;        // zerando o checksum
            pout->checksum = checksum(
                    (void *)pout, sizeof(struct sdtphdr)); // calculando o checksum

            // enviando o pacote
            sendto(meusocket, buffer_out, sizeof(struct sdtphdr), 0,
                    (struct sockaddr *)&destinatario, sockettamanho);

            // recebendo a confirmacao
            numbytes = recvtimeout(meusocket, buffer_in, MAXSDTP, 1000,
                    (struct sockaddr *)&destinatario, &sockettamanho);

            // verificando se houve timeout ou erro
            if (numbytes <= 0)
            {
                // reenviando o pacote
                continue;
            }

            // verificando se o pacote recebido tem a flag ACK e o numero de sequencia esperado
            if ((pin->flags & TH_ACK) && (pin->acknum == ackbytes + 1))
            {
                // enviando o ultimo pacote ACK
                printf("enviar o ultimo pacote ACK\n");

                // preenchendo o novo pacote
                pout->seqnum   = ackbytes + 1; // seqnum assume o ack anterior + 1
                pout->acknum   = 0;            // zerando o ack
                pout->datalen  = 0;            // zerando o tamanho dos dados
                pout->flags    = TH_ACK;       // setando a flag ACK
                pout->window   = 0;            // zerando a janela
                pout->checksum = 0;            // zerando o checksum
                pout->checksum = checksum(
                        (void *)pout, sizeof(struct sdtphdr)); // calculando o checksum

                // enviando o pacote
                sendto(meusocket, buffer_out, sizeof(struct sdtphdr), 0,
                        (struct sockaddr *)&destinatario, sockettamanho);

                // fechando o socket e o arquivo
                printf("fechando o socket e o arquivo\n");
                close(meusocket);
                fclose(loremfile);
                break; // saindo do loop
            }
            else
            {
                // reenviando o pacote
                continue;
            }
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

            // recebendo o pacote
            numbytes = recvfrom(meusocket, buffer_in, MAXSDTP, 0,
                    (struct sockaddr *)&destinatario, &sockettamanho);

            // verificando se houve erro na recepcao
            if (numbytes == -1)
            {
                // tratando o erro
                perror("recvfrom");
                exit(1);
            }

            // calculando o checksum do pacote recebido
            uint16_t checksum_recebido = checksum(
                    (void *)pin, sizeof(struct sdtphdr) + pin->datalen);

            // comparando com o valor do campo checksum do pacote
            if (checksum_recebido != pin->checksum)
            {
                // pacote corrompido, descartando
                printf("pacote corrompido, descartando\n");
                continue;
            }

            // pacote integro, verificando o tipo do pacote
            if (pin->flags & TH_SYN)
            {
                // pacote de sincronizacao, verificando o subtipo
                if (pin->flags & TH_ACK)
                {
                    // pacote SYN-ACK, verificando o estado atual
                    if (state == 0)
                    {
                        // estado 0, recebendo o SYN-ACK do servidor
                        printf("recebendo o SYN-ACK do servidor\n");
                        // mudando o estado para 1
                        state = 1;
                        // obtendo a janela do servidor
                        window = pin->window;
                    }
                    else
                    {
                        // estado invalido, descartando o pacote
                        printf("estado invalido, descartando o pacote\n");
                        continue;
                    }
                }
                else
                {
                    // pacote SYN, descartando o pacote
                    printf("pacote SYN, descartando o pacote\n");
                    continue;
                }
            }
            else if (pin->flags & TH_ACK)
            {
                // pacote ACK, verificando o estado atual
                if (state == 1)
                {
                    // estado 1, recebendo o ACK do servidor
                    printf("recebendo o ACK do servidor\n");
                    // mudando o estado para 2
                    state = 2;
                }
                else if (state == 3)
                {
                    // estado 3, recebendo o ACK do servidor
                    printf("recebendo o ACK do servidor\n");
                    // mudando o estado para 4
                    state = 4;
                }
                else
                {
                    // estado invalido, descartando o pacote
                    printf("estado invalido, descartando o pacote\n");
                    continue;
                }
            }
            else
            {
                // pacote de dados, verificando o estado atual
                if (state == 2 || state == 4)
                {
                    // estado 2 ou 4, recebendo os dados do servidor
                    printf("recebendo os dados do servidor\n");
                    // lendo os dados do pacote
                    printf("data: %s\n", (char *)pin + sizeof(struct sdtphdr));
                    // enviando o ACK para o servidor
                    printf("enviando o ACK para o servidor\n");
                    // preenchendo o novo pacote
                    pout->seqnum   = 0; // zerando o seqnum
                    pout->acknum   = pin->seqnum + pin->datalen; // acknum assume o seqnum do pacote recebido mais o tamanho dos dados
                    pout->datalen  = 0; // zerando o tamanho dos dados
                    pout->flags    = TH_ACK; // setando a flag ACK
                    pout->window   = 0; // zerando a janela
                    pout->checksum = 0; // zerando o checksum
                    pout->checksum = checksum(
                            (void *)pout, sizeof(struct sdtphdr)); // calculando o checksum
                    // enviando o pacote
                    sendto(meusocket, buffer_out, sizeof(struct sdtphdr), 0,
                            (struct sockaddr *)&destinatario, sockettamanho);
                }
                else
                {
                    // estado invalido, descartando o pacote
                    printf("estado invalido, descartando o pacote\n");
                    continue;
                }
            }

        }

    }


	close(meusocket);

    return 0;
}
