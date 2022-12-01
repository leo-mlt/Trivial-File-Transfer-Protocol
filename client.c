#include <arpa/inet.h> //Gestione della modalita' txt e bin??
#include <sys/types.h>//GEstione pacchetto con nulla dentro 
#include <sys/socket.h>//funzione per leggere una riga di comando
#include <netinet/in.h>  
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define REQUEST 1
#define MAX_LEN_BUFF 1024
#define MAX_LEN_DATA 512
#define MAX_LEN_COMANDO 6
#define MAX_LEN_ACK 5
#define MAX_LEN_NOMEFILE 512 //inserire condizione di errore per questo valore


//FUNZIONE PER LA CREAZIONE DEL PACCHETTO DI RICHIESTA

void pacchetto_rrq(char * file_name, bool mode, char * pacchetto){
	
	int pos = 0;
	uint16_t message_type = htons(REQUEST);
//	printf("%d\n", message_type);

	memcpy(pacchetto + pos, &message_type, sizeof(message_type)); //opcode
	pos += sizeof(message_type);	


	strcpy((pacchetto + pos), file_name);
	pos += strlen(file_name)+1;


	if(mode == true){

		strcpy(pacchetto + pos, "netascii\0");
		pos +=strlen("netascii\0");
	}else{
		strcpy(pacchetto + pos, "octet\0");
		pos +=strlen("octet\0");	
	}
	
	
}

//FUNZIONE PER CREAZIONE DELL'ACK

void creazione_ack(char * ack, int block){
	int pos = 0;
	uint16_t type_message = htons(4);
	uint16_t nblocco = htons(block);

	memcpy(ack + pos, &type_message, sizeof(type_message));
	pos += sizeof(type_message);
	
	memcpy(ack + pos, &nblocco, sizeof(nblocco));
	pos += sizeof(nblocco);
}


//FUNZIONE PER VISUALIZZAZIONE DEL MENU'

void menu(){
	printf("\n");
	printf("Sono disponibili i seguenti comandi: \n");
	printf("!help --> Mostra l'elenco dei comandi disponibili\n");
	printf("!mode --> Imposta il modo del trasferimento dei files (testo o binario)\n");
	printf("!get --> Richiede al server il file e lo salva localmente\n");
	printf("!quit --> termina il client\n");
	
}

/*************MAIN**************/

int main(int argc, char* argv[]){
	int sd, ret, addrlen, len2, len, tipo, blocco, len3, dim;
	uint16_t message_type, errore, nblocco, dimensione;
	bool mod = false;                          //Variabile per scelta del modo d invio
	struct sockaddr_in my_addr, srv_addr;
	char comando[MAX_LEN_COMANDO];        //array con il comando inserito
	char modalita[3];       //modalita' di trasferimento
	char a;          //carattere di uscita
	
	char data[MAX_LEN_DATA]; //array dei dati

	char ACK[MAX_LEN_ACK];
	char buffer[MAX_LEN_BUFF];
	char file_server[MAX_LEN_NOMEFILE]; //array che memorizza il file da richiedere al server
	char file_locale[MAX_LEN_NOMEFILE]; //nome che avra' il file memorizzato in locale
	char  rrq[MAX_LEN_BUFF];	//pacchetto rrq
	char error_message[MAX_LEN_BUFF]; //messaggio di errore
	FILE *ftp;

	sd = socket(AF_INET, SOCK_DGRAM, 0);
	
	/*creazione della bind*/
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(4242);
	my_addr.sin_addr.s_addr = INADDR_ANY;	

	ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));

	/*creazione indirizzo del server*/
	
	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	if(strcmp(argv[2], "69")==0){
		srv_addr.sin_port = htons(69);	
	}
	else{
		perror("Il server non e' in ascolto su questa porta\n");
		exit(-1);	
	}

	inet_pton(AF_INET, argv[1], &srv_addr.sin_addr);
	

	menu(); //VISUALIZZAZIONE DEI COMANDI

	while(1){ 
	
		scanf("%s", comando); //Prendo da tastiera il comando selezionato

	/*********GESTIONE DEL COMANDI IN INGESSO*******/
		if(strcmp(comando, "!help") == 0){
			menu();	
			continue;
		}
		//Gestione della selezione della modalita'
		else if(strcmp(comando, "!mode")==0){
c:			mod = false;
			printf("Hai Selezionato il comando ***!mode***\n");

			printf("Scegli la modalita' di trasferimento dei files: Binario o Testuale?(bin/txt)\n");

			scanf("%s", modalita);

			if(strcmp(modalita, "txt")== 0){
				mod = true;
				printf("Modo di Trasferimento Testuale Configurato\n");
			
			}
			else if(strcmp(modalita, "bin") == 0){
				printf("Modo di Trasferimento Binario Configurato\n");
							
			}
			else{
				printf("Modalita' selezionata non valida! Riprova(bin/txt)\n");
				goto c;	
			}		
		}

		else if(strcmp(comando, "!get") == 0){ //gestisco il trasferimento del file 

			printf("Hai Selezionato il comando **!get**\n");
			
			printf("Inserisci il nome del file da chiedere al server: ");
			scanf("%s", file_server);

local:		printf("Inserisci il path e il nome con cui salvare il file: ");
			scanf("%s", file_locale);
			
			
			//Creo pacchetto per la richiesta al server
			len = strlen(file_server); 
			len3 = strlen(file_locale);

			// Verifico che il nome inserito da terminale non superi il da dimensione dell'array
			if(len > MAX_LEN_NOMEFILE){
				printf("Nome inserito troppo lungo. Ritorno al menu!!!\n");
				menu();
				continue;
							
			}			

			if(len3 > MAX_LEN_NOMEFILE){
				printf("Nome inserito troppo lungo. Ritorno al menu!!!\n");
				menu();
				continue;
							
			}			
			
			// VERIFICO CHE IL PATH LOCALE INSERITO DALL'UTENTE SIA GIUSTO, PROVO CON UNA SEMPLICE APERTURA E CHIUSURA DEL FILE
			ftp = fopen(file_locale, "w");
			if(ftp == NULL){
				printf("Path inserito non esiste. Riprova!\n");
				goto local;								
			}
			fclose(ftp);


			file_server[len] = '\0';
			pacchetto_rrq(file_server, mod, rrq); //creo pacchetto rrq
			
			
			len2 = sizeof(rrq);
			//printf("%d\n", len2);
				
			
			printf("Richiesto file %s al server in corso\n", file_server);

			//Mando il pacchetto di richiesta al server 

			ret = sendto(sd, rrq, len2, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
			if(ret < 0){
				perror("Errore nel mandare RRQ");
				exit(-1);			
			}

			//printf("%d\n", ret);

			addrlen = sizeof(srv_addr);
			
		//	printf("Trasferimento in corso.\n");

			/***********GESTIONE PACCHETTI CHE ARRIVANO DAL SERVER******/
			 while(1){
				int pos = 0;
				memset(buffer, 0, MAX_LEN_BUFF); //pulizia
				memset(modalita, 0, 3); //pulizia
				ret = recvfrom(sd, buffer, MAX_LEN_BUFF, 0, (struct sockaddr*)&srv_addr, &addrlen);
				if(ret < 0){	
					perror("Errore della risposta di RRQ");
					exit(-1);			
				}

				//printf("%d\n", ret); Visualizza lettura buffer

				
				
				//Deserializzazione del pacchetto in ricezione
				memcpy(&message_type, buffer+pos, sizeof(message_type));
				pos += sizeof(message_type);

				tipo = ntohs(message_type);

				//Tipo di pacchetto con messaggio di errore
				if(tipo == 5){
					memcpy(&errore, buffer+pos, sizeof(errore));
					pos += sizeof(errore);

					//eventuale catturare numero di errore

					strcpy(error_message, buffer+pos);
					pos += strlen(error_message)+1;
					printf("%s\n", error_message);
					break;
												
				}
				//Tipo di pacchetto di tipo dati
				else if(tipo == 3){
					int blocchiarrivati;
					int blocchitotali;					

					memcpy(&nblocco, buffer + pos, sizeof(nblocco));
					pos += sizeof(nblocco);
					
					blocco = ntohs(nblocco);

					if(blocco == 1){
						blocchiarrivati = 0;
						printf("Trasferimento in corso.\n");

						ret = recvfrom(sd, (void*)&dimensione, sizeof(uint16_t), 0, (struct sockaddr*)&srv_addr, &addrlen);										

						if(ret < 0){	
						perror("Errore della risposta della dimensione\n");
						exit(-1);			
						}
						
						dim = ntohs(dimensione);
						blocchitotali = (dim/512) + 1;
	
					}

					

					//printf("%d\n", dim);
					//printf("Dimensione pos: %d\n", pos);

					//printf("%s\n", modalita);

					memset(data, 0, MAX_LEN_DATA); //pulizia

					//printf("%d\n", (strlen(data))); //Eliminare

					if(dim >= 512)
						memcpy(data, buffer + pos, MAX_LEN_DATA); //Errore (memcpy) //questa memcpy interagisce in qualche modo con il vettore modalita
					//pos += strlen(data);
					else
						memcpy(data, buffer + pos, dim);
					//printf("%d\n", (strlen(data))); //Eliminare
					//printf("%s\n", data);
					
				
					if(mod == false){
						if(blocco == 1){
							ftp = fopen(file_locale, "wb"); //apro file in scrittura in mod binaria			
							
						}else{
							ftp = fopen(file_locale, "ab"); //apro file in append in mod binaria
						}

						if(dim >= 512)
							fwrite(data, MAX_LEN_DATA, sizeof(char), ftp);
						else
							fwrite(data, dim, sizeof(char), ftp);					
					}			
					else{
						if(blocco == 1){
							ftp = fopen(file_locale, "w");	//apro file in scrittura in mod text				
						}else{
							ftp = fopen(file_locale, "a"); //apro file in append in mod text
						}
						fprintf(ftp, "%s", data);
					}
					
					
					fclose(ftp);

					++blocchiarrivati;

					//printf("%d\n", blocchiarrivati);

					//creo pacchetto di ACK
					creazione_ack(ACK, blocco);

					//Invio ack

					ret = sendto(sd, ACK, MAX_LEN_ACK, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));

					if(ret < 0){
						perror("Errore nell'invio del ack");
						exit(-1);					
					}
					
					if(dim < 512){ //Verifico che il pacchetto arrivato e' l'ultimo
						printf("Trasferimento Completato (%d/%d)\n", blocchiarrivati, blocchitotali);
						printf("Salvataggio %s Completato.\n", file_locale);
						break;
					}

					if(dim >=512)
						dim -= 512;
				} //if per il tipo 3
				
			}// while gestione pacchetti

			continue;		
		}

		//Gestione dell'uscita dal client
		else if(strcmp(comando, "!quit") == 0){ //perfezionare la quit
ch:			printf("Hai Selezionato il comando **!quit**\n");
			printf("Sei sicuro di voler chiudere il client?(s/n)\n");
			while(getchar()!='\n');			
			a = getchar();
			//printf("%c\n", a);
			while(getchar()!='\n');  //E' PRESENTE UN ERRORE SULLA GESTIONE DEL SI/NO
			if(a == 's'){
				printf("Terminazione del client\n");
				break;
			}
			else if(a == 'n'){
				printf("Client non terminato\n");
				continue;			
			}
			else
				printf("Carattere non valido! Riprova\n\n");
				goto ch;
		}
		else{
			printf("Comando selezionato non valido! Riprova!('!help' per visualizzare i comandi)\n");
		}
	}// chiusura while
	
	
	close(sd);

} // chiusura main

