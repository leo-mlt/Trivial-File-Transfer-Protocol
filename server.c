#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_LEN_BUFF 1024
#define MAX_LEN_DATA 512
#define MAX_LEN_ACK 5
#define MAX_LEN_NOMEFILE 512


//Serializzazione del pacchetto di errore da mandare al client
void creazione_errorpack(char * errore, int error_number){
	int pos = 0;
	uint16_t type_message =htons(5);
	uint16_t  error = htons(error_number);

	memcpy(errore + pos, &type_message, sizeof(type_message));
	pos += sizeof(type_message);

	memcpy(errore + pos, &error, sizeof(error));
	pos += sizeof(error);

	if(error_number == 4){
		strcpy(errore + pos, "Illegal TFTP operation\0");
		pos += strlen("Illegal TFTP operation\0");
	}else{
		strcpy(errore + pos, "File not found\0");
		pos += strlen("File not found\0");
	}
}

// Serializzazione del pacchetto dati da mandare al client
void creo_pacchetto_dati(char * pacchetto, char * data, int block, int nbytesread){
	int pos = 0;
	uint16_t type_message = htons(3);
	uint16_t blocco = htons(block);

	memcpy(pacchetto + pos, &type_message, sizeof(type_message));
	pos += sizeof(type_message);

	memcpy(pacchetto + pos, &blocco, sizeof(blocco));
	pos += sizeof(blocco);

	if(data[0] == '\0')
		return;

	//printf("%s\n", data);
	memcpy(pacchetto + pos, data, nbytesread);
	pos += strlen(data);
	

}

/************* MAIN SERVER*********************************/


int main(int argc, char * argv[]){
	int sd, ret, totale, rimanenza, blocco, pos, type, error_code, ultimo = 0, n;
	struct sockaddr_in my_addr, cl_addr;
	char buffer[MAX_LEN_BUFF];
	char pacchetto_errore[MAX_LEN_BUFF]; //pacchetto di errore
	char modalita[MAX_LEN_BUFF];       //memorizza la modalita' di trasferimento
	char pacchetto_dati[MAX_LEN_BUFF];
	uint16_t message_type, tipo, dimensione;
	char percorso_file[MAX_LEN_BUFF];
	fd_set rset;
	
	int addrlen = sizeof(cl_addr);

	char nome_file[MAX_LEN_NOMEFILE];
	char ACK[MAX_LEN_ACK];
	FILE * ftp;

	
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	
	//creazione della bind
	memset(&my_addr, 0, sizeof(my_addr)); //pulizia
	my_addr.sin_family = AF_INET;
	
	if(strcmp(argv[1], "69")==0)
		my_addr.sin_port = htons(69);
	else{
		printf("Porta non adatta per il protocollo TFTP");
		exit(-1);	
	}

	my_addr.sin_addr.s_addr = INADDR_ANY;
	

	ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));	
	if(ret < 0){
		perror("Errore nella bind\n");
		exit(-1);	
	}

	FD_ZERO(&rset);
	
	FD_SET(sd, &rset);
		
	

	while(1){
		//GESTIONE DEL SERVER CONCORRENTE

		select(sd +1, &rset, NULL, NULL, NULL);  // OK mod alternativa e' eseguire con fork

		if(FD_ISSET(sd, &rset)){
		
		ret = recvfrom(sd, buffer, MAX_LEN_BUFF, 0, (struct sockaddr*)&cl_addr,	&addrlen);
		if(ret < 0){
			perror("Errore nella ricezione della richiesta\n");
			exit(-1);	
		}// FINE GESTIONE DEL SERVER CONCORRENTE
		
		//deserializzo il pacchetto
		pos = 0;
	
		//printf("%s\n", buffer);
	
		memcpy(&message_type, buffer+pos, sizeof(message_type));
		pos += sizeof(message_type);
		
		tipo = ntohs(message_type);
	
		strcpy(nome_file, buffer+pos);
		pos += strlen(nome_file) +1;
	
		//printf("%s\n", nome_file);
	
		//Pacchetto con richiesta di un file
		if(tipo == 1){
			strcpy(percorso_file, argv[2]);
			// gestico invio del file
			strcat(percorso_file, "/");
			strcat(percorso_file, nome_file);
	
			//printf("%s\n", percorso_file);
	
			strcpy(modalita, buffer+pos);
			pos += strlen(modalita)+1;
	
			if(strcmp(modalita, "netascii")==0)
				ftp = fopen(percorso_file, "r"); //apro in modalita' txt
			else
				ftp = fopen(percorso_file, "rb"); //apro in modalita' bin
	
			//gestione di file non trovato (invio pacchetto di errore)
			if(ftp == NULL){
				error_code = 1;
				creazione_errorpack(pacchetto_errore, error_code);	
				ret = sendto(sd, pacchetto_errore, MAX_LEN_BUFF, 0, (struct sockaddr*)&cl_addr, sizeof(cl_addr));	
				if(ret < 0){
					perror("Errore nell'invio del pacchetto di errore del file non trovato\n");				
				}
				continue;
			}	
		
			//Gestione invio del file
			blocco = 1;
			fseek(ftp, 0, SEEK_END);
			totale = ftell(ftp); //misuro grandezza del file
	
			printf("%d\n", totale);
	
			fseek(ftp, 0, SEEK_SET);
			rimanenza = totale;
	
			if(rimanenza == 0 && totale == 0) //nei casi particolari assumo che il file piu' di 1 cosi' posso mandare ultimo pacchetto di dimensione 0
				rimanenza++;
	
			while(rimanenza > 0 || ultimo == 1){
				char temp[MAX_LEN_DATA];
				
				memset(temp, 0, MAX_LEN_DATA); //pulizia

				if(rimanenza > MAX_LEN_DATA){
					n = fread(temp, MAX_LEN_DATA, sizeof(char), ftp); //fread(temp, MAX_LEN_DATA-1, sizeof(char), ftp);
					//temp[MAX_LEN_DATA] = '\0';
					n= n*MAX_LEN_DATA;
					rimanenza -= MAX_LEN_DATA;			
				}else{
					if(ultimo == 1 || totale == 0){ //caso particolare di ultimo pacchetto
						ultimo = 0;
						rimanenza = 0;
						n = 0;
						temp[0] = '\0';				
					}
					else{ //caso di ultimo pacchetto non particolare
						n = fread(temp, rimanenza, sizeof(char), ftp);
					//	temp[rimanenza] = '\0';	
						n = n*rimanenza;
						rimanenza = 0;			
					}
				}		
				//printf("%s     %d\n", temp, n);
				creo_pacchetto_dati(pacchetto_dati, temp, blocco, n);
				dimensione = htons(totale);
	
				ret = sendto(sd, pacchetto_dati, sizeof(buffer), 0, (struct sockaddr*)&cl_addr, sizeof(cl_addr));
			
				if(ret < 0){
					printf("Errore invio pacchetto numero %d\n", blocco);
					break;			
				}
	
				if(blocco == 1){
					ret = sendto(sd, (void*)&dimensione, sizeof(uint16_t), 0, (struct sockaddr*)&cl_addr, sizeof(cl_addr));
			
					if(ret < 0){
						printf("Errore invio dimensione\n");
						break;			
					}
				}

				//ricezione ack
				ret = recvfrom(sd, ACK, MAX_LEN_ACK, 0, (struct sockaddr*)&cl_addr, &addrlen);
				if(ret < 0){
					perror("Errore nella ricezione dell'ack");
					break;			
				}
	
				printf("ACK per il pacchetto %d\n", blocco); //printf di verifica ricezione ack	
	
				++blocco;
	
				//verifico pacchetto da mandare sia l'ultimo multiplo di 512
				if(n == 512 && rimanenza == 0){
						ultimo = 1;
						continue;
				}
			}// chiusura while gestione del pacchetto
		
		
	
		} //fine if 

		else{
			//mando pacchetto con errore
			error_code = 4;

			creazione_errorpack(pacchetto_errore, error_code); //creo pacchetto di errore con error_code 4

			ret = sendto(sd, pacchetto_errore, MAX_LEN_BUFF, 0, (struct sockaddr*)&cl_addr, sizeof(cl_addr));
			if(ret < 0){
				perror("Errore nell'invio del pacchetto di errore per operazione non valida\n");			
			}
			continue;
		}
	


		}

	}//fine while infinito


close(sd);

}// fine main