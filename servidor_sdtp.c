/**
 * \file servidor_sdtp.c
 * \brief Implementacao do Servidor SDTP
 * \author Joao Borges
 *
 * \todo Implementar armazenamento do DEBUG do servidor em arquivos de LOG,
 * permitindo que ele possa rodar em \a background sem problemas
 *  
 * \mainpage
 * 
 * <h2>Diagrama de estados do Servidor SDTP (Finite State Machine)</h2>
 * 
 * \dot
    digraph fsm {
        graph [ rankdir="LR"];
        node [fontname=Helvetica, fontsize=10];
        a [ label="WAIT_SYN" URL="\ref A"];
        b [ label="WAIT_ACK" URL="\ref B"];
        c [ label="ESTABLISHED" URL="\ref C" group="d"];
        d [ label="CLOSED"   URL="\ref D" group="c" ];
        edge [fontsize=9];
        a -> b [ label="SYN / SYN-ACK" ];
        b -> c [ label="ACK / ^" ];
        c -> c [ headport="n" tailport="n" label="data & sum_ok / ACK" ];
        c -> d [ tailport="ne" headport="n" label="FIN & correct / ACK" ];
        c -> d [ tailport="se" headport="s" label="FIN & incorrect / RST" ];
    }
    \enddot
 *
 * <b>OBS.: Qualquer pacote corrompido sera descartado pelo servidor, nao 
 * gerando resposta ao cliente.</b>
 *
 * Este diagrama segue a notacao:
 * - Evento / Acao
 *
 * Por exemplo, estando no estado WAIT_SYN, ao receber um pacote SYN, ira
 * enviar ao cliente um pacote SYN-ACK:
 * - SYN / SYN-ACK
 *
 * Quando a ação for denotada por ^, significa que nenhuma ação é executada.
 *
 * Para as transições do estado ESTABLISHED para CLOSED:
 * - Caso receba um pacote FIN e se verifique que o envio foi bem sucedido,
 *   estando os dados recebidos corretos, ele enviara um pacote ACK
 * - Caso receba um pacote FIN, mas os dados enviados estejam errados,
 *   enviara um pacote RST para o cliente
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>

#include "sdtp.h"

/// \defgroup states Estados do socket SDTP
/// \{
#define SDTP_WAIT_SYN       0x00 ///< Aguardando syn do 3-way handshake 
#define SDTP_WAIT_ACK       0x01 ///< Aguardando ack do 3-way handshake
#define SDTP_ESTABLISHED    0x02 ///< Conexao estabelecida
#define SDTP_CLOSED         0x03 ///< Conexao finalizada
/// \}

/// \defgroup errors Possiveis erros simulados
/// \{
#define SDTP_ERROR_NONE     0x00 ///< Sem erro
#define SDTP_ERROR_LOST_IN  0x01 ///< Perda de pacote na recepcao
#define SDTP_ERROR_LOST_OUT 0x02 ///< Perda de pacote no envio
#define SDTP_ERROR_SUM_IN   0x03 ///< Checksum errado no pacote recebido
#define SDTP_ERROR_SUM_OUT  0x04 ///< Checksum errado no pacote enviado
/// \}

/**
 * Define o calculo para geracao de um valor (nao nulo) para a janela
 */
#define WINDOW() (rand() % MSS)+1

/** 
 * Estrutura referente a um socket SDTP estabelecido
 */
struct socket_sdtp
{
    uint32_t ip;              ///< Ip do cliente
    uint16_t porta;           ///< Porta do cliente
    char data[2*LOREMSIZE];   ///< Dados entregues pelo cliente
    uint8_t  state;           ///< Estado da conexao @see states
    uint16_t expseqnum;       ///< Numero de sequencia esperado
    uint8_t  window;          ///< Armazena o valor da janela informado
    struct socket_sdtp *next; ///< Proximo item da lista de conexoes ativas
};

/**
 * Ponteiro para o inicio da lista de conexoes (estruturas) ativas
 */
struct socket_sdtp *head = NULL;

/**
 * Quantidade de sockets estabelecidos
 */
int numsockets = 0;

/**
 * Armazena o checksum dos dados do arquivo lorem_ipsum.txt
 *
 * Este valor sera comparado ao final da transmissao do cliente, para
 * verificar a validade dos dados enviados.
 */
uint16_t datasum = 0;

/**
 * Armazena o valor do erro simulado para cada recepcao de pacote
 */
char global_error;

/**
 * Retorna o ponteiro para um socket sdtp, de acordo com a tupla 
 * (ip, porta) recebida.
 *
 * Se nao encontrar um socket sdtp para a tupla, cria um novo socket sdtp e
 * o retorna.
 *
 * \param addr Um ponteiro para os dados recebidos do cliente.
 * \return Um ponteiro para um socket sdtp.
 */
struct socket_sdtp * get_socket_sdtp(struct sockaddr_in * addr)
{
    struct socket_sdtp *tmp = NULL, *last = NULL;
    
    tmp = head;

    while(tmp != NULL)
    {
        // verificando se ja ha socket
        if (tmp->ip == addr->sin_addr.s_addr
                &&
            tmp->porta == htons(addr->sin_port))
        {
            return tmp;
        }
        
        last = tmp;
        tmp = tmp->next;
    }

    numsockets++;
    
    // nao encontrou o elemento na lista, entao cria um novo
    tmp = (struct socket_sdtp *) malloc(sizeof(struct socket_sdtp));

    // preenchendo os campos da estrutura
    tmp->ip        = addr->sin_addr.s_addr;
    tmp->porta     = htons(addr->sin_port);
    memset(tmp->data, 0x0, sizeof(tmp->data));
    tmp->state     = SDTP_WAIT_SYN;
    tmp->expseqnum = 0;
    tmp->window    = 0;
    tmp->next      = NULL;
    
    // ja existe elementos na lista
    if (head != NULL)
    {
        last->next = tmp;
    }
    // nao existe elementos
    else
    {
        head = tmp;
    }

    return tmp;
}

/**
 * Remove o socket SDTP ativo, liberando o seu espaco
 *
 * Apos remover o socket SDTP, reorganiza a fila de conexoes ativas.
 *
 * \param s Ponteiro para o socket que se deseja remover
 */
void remove_socket_sdtp(struct socket_sdtp *s)
{
    struct socket_sdtp *tmp = NULL, *last = NULL;

    // primeiro elemento da lista
    if (s == head)
    {
        head = s->next;
        free(s);
        numsockets--;
    }
    else
    {
        tmp = head->next;
        last = head;

        while(tmp != NULL)
        {
            if (tmp == s)
            {
                last->next = tmp->next;
                free(s);
                numsockets--;
                break;
            }
            
            last = tmp;
            tmp = tmp->next;
        }
    }
}

/**
 * Imprime a lista de conexoes (sockets) ativas
 */
void print_socket_list()
{
    struct socket_sdtp *tmp = head;
    
    printf("DEBUG - SOCKET LIST\n");

    while(tmp != NULL)
    {
        printf("%x %d\n",tmp->ip,tmp->porta);
        tmp = tmp->next;
    }
}

/**
 * Gerador de um erro aleatorio, para cada pacote recebido
 *
 * O erro sera gerado conforme as probabilidades definidas para os Possiveis
 * Erros Simulados.
 *
 * Gera um erro simulado com as seguintes probabilidades (%)
 * - SDTP_ERROR_NONE (0x00):     70  
 * - SDTP_ERROR_LOST_IN (0x01):  10  
 * - SDTP_ERROR_LOST_OUT (0x02): 05  
 * - SDTP_ERROR_SUM_IN (0x03):   05  
 * - SDTP_ERROR_SUM_OUT (0x04):  10  
 *
 * Exemplo da estrategia de geracao do erro, retornando o indice do vetor de
 * probabilidades, conforme seja encontrado o valor gerado:
 *
 \verbatim
    0                        70        80      85      90       99
    [------------------------[---------[-------[-------[---------]
                NONE           LOSTin   LOSTout  SUMin   SUMout
 \endverbatim
 *
 * \see errors
 *
 * \todo: Definir os valores das probabilidades de acordo com a realidade
 *
 * \return Um erro a ser simulado.
 */
char simerror()
{
    // 0                        70        80      85      90       99
    // [------------------------[---------[-------[-------[---------]
    //             NONE           LOSTin   LOSTout  SUMin   SUMout
    
    // definindo as probabilidades, 
    // onde prob[i] = probabilidade do evento i definido
    int prob[5] = {70, 10, 5, 5, 10};
    
    // gerando um resultado aleatoriamente
    int r = rand() % 100;

    printf("RAND GERADO: %d\n",r);

    char i = 0;
    int sum = prob[i];

    // encontrando qual evento ocorreu, com base em suas probabilidades
    while (r >= sum)
    {
        sum += prob[++i];
    }

    // retorna o indice equivalente ao evento
    return i;
}

/**
 * Corrompe alguns bytes de buf, entre os bytes 0 e len passado
 *
 * \param buf Ponteiro para a posicao inicial do buffer a ser corrompido
 * \param len Tamanho em bytes do buffer a ser corrompido
 */
void corrupt(char *buf, int len)
{
    int i = 5;

    while(i--)
    {
        buf[rand()%len] = rand() % 256;
    }
}

/**
 * Funcao responsavel por fazer o tratamento no pacote recebido.
 *
 * \return 1 quando for necessario reponder um pacote ao cliente, que sera
 * realizado no `main()`
 * \return 0 quando nao for necessario resposta, seja por causa de um erro
 * real ou simulado
 * 
 */
int handle_socket_sdtp(struct socket_sdtp *s, struct sdtphdr *p)
{ 
    // se for um pacote de sincronizacao esperado do 3-way handshake
    if ( p->flags == TH_SYN )
    {
        if ( s->state == SDTP_WAIT_SYN )
        {
            // muda o estado para wait ack
            s->state = SDTP_WAIT_ACK;
        }

        // responde com syn/ack
        p->seqnum   = 0;
        p->acknum   = 0;
        p->flags    = TH_SYN|TH_ACK;
        s->window   = WINDOW(); // define o valor da janela
        p->window   = s->window;
        p->checksum = 0;
        p->checksum = checksum((void *)p, sizeof(struct sdtphdr));

        // habilita devolucao do pacote
        return 1;
    }
    // se for o ack do 3-way handshake esperado
    else if ( p->flags == TH_ACK )
    {
        if ( s->state == SDTP_WAIT_ACK )
        {
            // pronto para receber dados, ou finalizar a transferencia
            s->state = SDTP_ESTABLISHED;
        }

        // nao retorna nada
        return 0;
    }
    // conexao ja estabelecida e pacote de finalizacao
    else if ( p->flags == TH_FIN 
                &&
              (
               s->state == SDTP_ESTABLISHED 
                ||
               s->state == SDTP_CLOSED
              )
            )
    {
        // finaliza conexao
        s->state = SDTP_CLOSED;

        printf("size final: %d\n",strlen(s->data));

        printf("datasum %d %x\n",datasum,datasum);
        printf("checksum final %d\n",checksum((void *)s->data, LOREMSIZE));

        // verifica e a validade dos dados recebidos
        if (
            strlen(s->data) == LOREMSIZE 
                &&
            checksum((void *)s->data, LOREMSIZE) == datasum
            )
        {
            printf("checksum final bateu!\n\n\n");
        
            // se dados corretos, devolve ACK e finaliza
            p->flags = TH_ACK;
            
        }
        else
        {
            printf("erro no checksum final!\n\n\n");
        
            // se dados errados, devolve RST e finaliza
            p->flags = TH_RST;
        }
            
        p->seqnum   = 0;
        p->acknum   = 0;
        p->window   = 0;
        p->checksum = 0;
        p->checksum = checksum((void *)p, sizeof(struct sdtphdr));

        // se nao houver nenhum erro programado, pode finalizar
        // o socket da conexao
        if ( global_error == SDTP_ERROR_NONE )
        {
            // libera o espaco do socket sdtp desta conexao
            // reorganizar a fila de conexoes, ver se precisa de um 
            remove_socket_sdtp(s);
        
            print_socket_list();

            printf("REALMENTE FINALIZOU!\n\n");
        }

        // habilita envio deste pacote
        return 1;

        
    }
    // pacote de dados e conexao estabelecida
    else if ( p->flags == 0x00 
                && 
              (
                s->state == SDTP_ESTABLISHED 
                    || 
                s->state == SDTP_WAIT_ACK 
              )
            )
    {
        // se o ACK falhar, assumo os dados como meu ack e mudo estado
        if (s->state == SDTP_WAIT_ACK )
        {
            s->state = SDTP_ESTABLISHED;
        }

        // verificando:
        // - se o pacote esta na ordem correta
        // - se o tamanho esta de acordo com a janela
        if ( 
            s->expseqnum == p->seqnum 
                &&
            s->window >= p->datalen
            )
        {
            // verifica se ainda cabe no buffer
            if ( strlen(s->data) + p->datalen < 2*LOREMSIZE )
            {
                // salva os dados no buffer da conexao
                memcpy(
                    s->data + p->seqnum, // deslocamento no buffer
                    (uint8_t *)p + sizeof(struct sdtphdr), // dados
                    p->datalen  // tamanho informado
                   );

                // anda o valor do proximo ack esperado
                s->expseqnum += p->datalen; // a ser retornado no ack
            }
        }
        
        // devolve um ack para o cliente
        // se algum teste acima falhar, este ack sera equivalente 
        // ao ultimo pacote valido recebido
        p->seqnum   = 0;
        p->acknum   = s->expseqnum;
        p->datalen  = 0;;
        p->flags    = TH_ACK;
        s->window   = WINDOW(); // define o valor da janela
        p->window   = s->window;
        p->checksum = 0;
        p->checksum = checksum((void *)p, sizeof(struct sdtphdr));

        // habilita devolucao do ack no main
        return 1;
    }
    else
    {
        printf("DEBUG - PACOTE INVALIDO RECEBIDO:\n");
        printpacket(p);
    }

    return 0;
}

/**
 * Funcao principal do servidor
 *
 * Nesta ocorre o \a looping infinito do servidor:
 * - Recebendo pacotes de clientes
 * - Passando para o tratador
 * - Recebendo a resposta do tratador
 * - Devolvendo ou nao uma resposta ao cliente
 */
int main(int argc, char *argv[])
{
    char buffer[MAXSDTP];

    struct sdtphdr *p;

    struct socket_sdtp *sdtp_sockid;
    
    // abrindo o arquivo lorem_ipsum.txt e calculando seu checksum
    FILE *loremfile = fopen("./lorem_ipsum.txt", "r");
    char loremdata[LOREMSIZE];
    fread(loremdata, 1, LOREMSIZE, loremfile);
    datasum = checksum((void *)loremdata, LOREMSIZE);

    printf("Checksum do arquivo: %d\n",datasum);

    // reiniciando a semente
    srand(time(NULL));

    // descritor do socket do servidor
    int meusocket;

    // recebe o tamanho da estrutura sockaddr_in
    int sockettamanho;

    // numero de bytes recebidos pelo socket
    int numbytes;
    
    // informacoes do cliente
    struct sockaddr_in endereco_cliente;

    // informacoes do servidor
    struct sockaddr_in endereco_servidor;

    // tamanho da estrutura de endereco do socket usado
    sockettamanho = sizeof(endereco_cliente);
    
    // criando o socket
    meusocket = socket(AF_INET, SOCK_DGRAM, 0);

    endereco_servidor.sin_family = AF_INET;
    
    // define qualquer ip da interface de rede
    endereco_servidor.sin_addr.s_addr = INADDR_ANY;
    
    // define a porta de escuta do servidor
    endereco_servidor.sin_port = htons(PORTA);
    
    // zera o resto da estrutura
    memset(&(endereco_servidor.sin_zero), '\0', 
            sizeof(endereco_servidor.sin_zero));

    // liga o socket ao enderecamento do servidor
    bind(meusocket, (struct sockaddr *)&endereco_servidor, 
            sizeof(struct sockaddr));
    
    printf("Servidor escutando conexoes UDP na porta: %d\n", PORTA);
        
    // apontando para o inicio da estrutura
    p = (struct sdtphdr *)&buffer;

    int state = 0;

    // armazena o resultado do checksum para cada pacote recebido
    uint16_t sum = 0;

    while(1)
    {
        printf("Servidor: esperando no recvfrom...\n");

        // limpa o buffer para o novo pacote
        memset(buffer, 0x0, MAXSDTP);

	    numbytes = recvfrom(meusocket, buffer, MAXSDTP, 0,
		    (struct sockaddr *)&endereco_cliente, &sockettamanho);

    	printf("Servidor: pacote recebeu %d bytes\n", numbytes);
    	printf("Servidor: possui %d conexoes ativas\n", numsockets);

        //printf("teste de campo: %x\n",p->flags);
        //printf("\tip: %x\n",endereco_cliente.sin_addr.s_addr);
        //printf("\tporta: %d\n",htons(endereco_cliente.sin_port));
    
        // imprime pacote recebido
        printpacket(p);

        // simula um erro para esta etapa da simulacao
        global_error = simerror();
      
        printf("ERRO GERADO: %x\n",global_error);
    
        // calculando o valor do checksum
        //   sum = 0, em caso de sucesso, ou 
        //   sum > 0, caso contrario
        sum = checksum((void *)p, p->datalen+sizeof(struct sdtphdr));
    
        // verdadeiro em caso de checksum invalido
        //if ( sum != 0xffff )
        if ( sum )
        {
            printf("DEBUG de CHECKSUM\n");
            printf("\tRecebido:  %d\n",p->checksum);
            printf("\tCalculado: %d\n",sum);
            printf("\tNum bytes considerados %d\n\n",
                    p->datalen+sizeof(struct sdtphdr));
        }

        // possibilidades de erro neste ponto:
        // - por perda de pacote na recepcao (simulado)
        // - por checksum invalido (simulado)
        // - por checksum invalido (calculado/real)
        // nao faz nada com o pacote
        if (
            global_error == SDTP_ERROR_LOST_IN 
                ||
            global_error == SDTP_ERROR_SUM_IN 
                ||
            sum
            )
        {
            // nao envia nada como resposta ao cliente
            continue;
        }

        // obtem o socket sdtp para esta conexao
        sdtp_sockid = get_socket_sdtp(&endereco_cliente);

        print_socket_list();

        // passa o pacote para ser analisado pelo tratador
        //
        // em caso de retorno = 1, renvia pacote formatado dentro da funcao
        if ( handle_socket_sdtp(sdtp_sockid, p) )
        {
            // em caso de envio perdido (simulado), nao faz o envio
            if ( global_error == SDTP_ERROR_LOST_OUT )
            {
                continue;
            }

            // em caso de pacote enviado ser corrompido
            if ( global_error == SDTP_ERROR_SUM_OUT )
            {
                corrupt(buffer, sizeof(struct sdtphdr));
            }

            printf("IMPRIMINDO PACOTE REPLY\n");
            printpacket(p);

            numbytes = sendto(meusocket, buffer, sizeof(struct sdtphdr), 0,
			 (struct sockaddr *)&endereco_cliente, sockettamanho);

            printf("Servidor: enviou %d bytes\n\n", numbytes);
        }
        else
        {
            printf("Servidor: nao enviou resposta\n\n");
        }
    }
	
    close(meusocket);

    return 0;
}
