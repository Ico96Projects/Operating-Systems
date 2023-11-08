
#include "master.h"

/*	conf 1 	*/
/*
#define FILE_CONFIG "conf#1.conf"

/*	conf 2 	*/

#define FILE_CONFIG "conf#2.conf"

/*	conf 3 	*/
/*
#define FILE_CONFIG "conf#3.conf"

/* variabili globali che corrispondono ai parametri di configurazione */
int SO_USERS_NUM;
int SO_NODES_NUM;
int SO_BUDGET_INIT;
int SO_REWARD;
int SO_MIN_TRANS_GEN_NSEC;
int SO_MAX_TRANS_GEN_NSEC;
int SO_RETRY;
int SO_TP_SIZE;
int SO_MIN_TRANS_PROC_NSEC;
int SO_MAX_TRANS_PROC_NSEC;
int SO_SIM_SEC;
int SO_FRIENDS_NUM;
int SO_HOPS;

/* varibili globali usate in tutta la simulazione */
sem_t mb_sem;
sem_t f_sem;
int size_users;
int size_nodes;
int size_friends;
int size_t_hops;
int queue_id;

/*  shared memory  */
single_block *master_book;
id_proc *active_users;
id_proc *active_nodes;
count_attempt *trans_hops;
int *remaning_trans_in_tp;
int *status_simulazione;
int *book_row_write;
int *friend_to_add;

/* variabili globali usate per la stampa da effettuare ogni secondo */
int sec;
int switch_print;
int book_row_print;
int book_block_print;
balance_proc *users_balance;
int num_users_proc;
balance_proc *nodes_balance;
int num_nodes_proc;

/*
 *Legge un file di configurazione 'FILE_CONFIG' (in master.h).
 *Il file contiene una serie di valori che sono assegnati alle variabili globali.
 *NB: L'ORDINE DI LETTURA DEL FILE È RILEVANTE!
 *	  SO_REGISTRY_SIZE e SO_BLOCK_SIZE devono essere modificati in 'master.h' perchè letti a tempo di compilazione.
 *    (stesso meccanismo per file config)
 *Dopo la lettura l'ambiente è stato preparato e inizia la simulazione.
*/
void read_file_config() {
	FILE *fp;

	fp = fopen(FILE_CONFIG, "r");
	if (fp == 0) {
		printf("\n ERROR: bad behaviour opening FILE_CONFIG");
		exit(EXIT_FAILURE);
	}
	fscanf (fp, "%d", &SO_USERS_NUM);
	fscanf (fp, "%d", &SO_NODES_NUM);
	fscanf (fp, "%d", &SO_BUDGET_INIT);
	fscanf (fp, "%d", &SO_REWARD);
	fscanf (fp, "%d", &SO_MIN_TRANS_GEN_NSEC);
	fscanf (fp, "%d", &SO_MAX_TRANS_GEN_NSEC);
	fscanf (fp, "%d", &SO_RETRY);
	fscanf (fp, "%d", &SO_TP_SIZE);
	fscanf (fp, "%d", &SO_MIN_TRANS_PROC_NSEC);
	fscanf (fp, "%d", &SO_MAX_TRANS_PROC_NSEC);
	fscanf (fp, "%d", &SO_SIM_SEC);
	fscanf (fp, "%d", &SO_FRIENDS_NUM);
	fscanf (fp, "%d", &SO_HOPS);

	fclose(fp);

	printf("\nSET PARAMETRI ");
	printf("\n  Le MACRO sono state definite nel file 'master.h':");
	printf("\n    SO_BLOCK_SIZE = %d", SO_BLOCK_SIZE);
	printf("\n    SO_REGISTRY_SIZE = %d\n", SO_REGISTRY_SIZE);

	printf("\n  Le variabili globali i parametri di input del file :  "FILE_CONFIG);
	printf("\n    SO_USERS_NUM = %d", SO_USERS_NUM);
	printf("\n    SO_NODES_NUM = %d", SO_NODES_NUM);
	printf("\n    SO_BUDGET_INIT = %d", SO_BUDGET_INIT);
	printf("\n    SO_REWARD = %d", SO_REWARD);
	printf("\n    SO_MIN_TRANS_GEN_NSEC = %d", SO_MIN_TRANS_GEN_NSEC);
	printf("\n    SO_MAX_TRANS_GEN_NSEC = %d", SO_MAX_TRANS_GEN_NSEC);
	printf("\n    SO_RETRY = %d", SO_RETRY);
	printf("\n    SO_TP_SIZE = %d", SO_TP_SIZE);
	printf("\n    SO_MIN_TRANS_PROC_NSEC = %d", SO_MIN_TRANS_PROC_NSEC);
	printf("\n    SO_MAX_TRANS_PROC_NSEC = %d", SO_MAX_TRANS_PROC_NSEC);
	printf("\n    SO_SIM_SEC = %d", SO_SIM_SEC);
	printf("\n    SO_FRIENDS_NUM = %d", SO_FRIENDS_NUM);
	printf("\n    SO_HOPS = %d", SO_HOPS);
}

/*
 * setta i valori delle variabili globali non associate alla memoria condivisa
 */
void set_global_variables(){
	struct msqid_ds data_queue;
	size_users = SO_USERS_NUM;
	size_nodes = SO_NODES_NUM * 5;
	size_friends = SO_FRIENDS_NUM * 5;
	size_t_hops = SO_USERS_NUM*15;
	sec = 0;
	switch_print = 50;
	printf("\n\n   Se il numero dei processi di cui dobbiamo stampare lo stato è maggiore di %d \n   allora stamperemo solo lo stato dei processi più significativi",switch_print);

	if (sem_init(&mb_sem, 1, 1) == -1) {
		TEST_ERROR;
		exit(EXIT_FAILURE);
	}
	if (sem_init(&f_sem, 1, 1) == -1) {
		TEST_ERROR;
		exit(EXIT_FAILURE);
	}

	queue_id = msgget(ftok(FILE_FOR_QUEUE, 1), IPC_CREAT | 0666);

	msgctl(queue_id, IPC_STAT, &data_queue);
	data_queue.msg_qbytes = MSG_LENGTH * 5 * (data_queue.msg_qbytes/MSG_LENGTH);
	msgctl(queue_id, IPC_SET, &data_queue);

	book_row_print = 0;
	book_block_print = 0;
	num_users_proc = 0;
	num_nodes_proc = 0;

	users_balance = malloc( sizeof(balance_proc) * size_users );
	nodes_balance = malloc( sizeof(balance_proc) * size_nodes );
}

/*
 *Funzione che fornisce l'ID del segmento di memoria condivisa richiesto; se non esiste ne crea uno nuovo.
 *(più di un segmento di memoria condivisa: masterbook, active_users, active_nodes....)
 *Tale segmento di memoria sarà riempito in un secondo momento.
*/
int get_shm_id(size_t size, int index_key) {
	key_t key;

	key = ftok(FILE_FOR_SHM, index_key);
	if (key == -1) {
		printf("\n key for shared memory does not fit");
		return key;
	}
	return shmget(key, size, IPC_CREAT | 0666);
}

/*
 * crea porzioni di memoria condivisa assegnandola a variabili globali
*/
void set_shared_memory(){
	int shm_id;

	shm_id = get_shm_id(SO_REGISTRY_SIZE * sizeof(single_block), 'x');
	master_book = (single_block*) shmat(shm_id, NULL, 0);

	shm_id = get_shm_id(size_users * sizeof(id_proc), 'y');
	active_users = (id_proc*) shmat(shm_id, NULL, 0);

	shm_id = get_shm_id(size_nodes * sizeof(id_proc), 'z');
	active_nodes = (id_proc*) shmat(shm_id, NULL, 0);

	shm_id = get_shm_id(size_t_hops * sizeof(count_attempt), 'h');
	trans_hops = (count_attempt*) shmat(shm_id, NULL, 0);

	shm_id = get_shm_id(sizeof(int)*size_nodes, 't');
	remaning_trans_in_tp = (int*) shmat(shm_id, NULL, 0);

	shm_id = get_shm_id(sizeof(int)*1, 's');
	status_simulazione = (int*) shmat(shm_id, NULL, 0);

	shm_id = get_shm_id(sizeof(int)*1, 'b');
	book_row_write = (int*) shmat(shm_id, NULL, 0);

	shm_id = get_shm_id(sizeof(int)*2, 'f');
	friend_to_add = (int*) shmat(shm_id, NULL, 0);
}

/*
 * questo metodo si occupa di deallocare la memoria occupato durante l'intera simulazione
*/
void dealloc_mem(){
	int shm_id;

	free(users_balance);
	free(nodes_balance);
	msgctl(queue_id, IPC_RMID, NULL);	

	shmdt((void *) master_book);
	shm_id = get_shm_id(SO_REGISTRY_SIZE * sizeof(single_block), 'x');
	shmctl(shm_id, IPC_RMID, NULL);
	sem_destroy(&mb_sem); 

	shmdt((void *) active_users);
	shm_id = get_shm_id(size_users * sizeof(id_proc), 'y');
	shmctl(shm_id, IPC_RMID, NULL);
	
	shmdt((void *) active_nodes);
	shm_id = get_shm_id(size_nodes * sizeof(id_proc), 'z');
	shmctl(shm_id, IPC_RMID, NULL);

	shmdt((void *) trans_hops);
	shm_id = get_shm_id(size_t_hops * sizeof(count_attempt), 'h');
	shmctl(shm_id, IPC_RMID, NULL);

	shmdt((void *) remaning_trans_in_tp);
	shm_id = get_shm_id(sizeof(int)*size_nodes, 't');
	shmctl(shm_id, IPC_RMID, NULL);

	shmdt((void *) status_simulazione);
	shm_id = get_shm_id(sizeof(int)*1, 's');
	shmctl(shm_id, IPC_RMID, NULL);

	shmdt((void *) friend_to_add);
	shm_id = get_shm_id(sizeof(int)*1, 'b');
	shmctl(shm_id, IPC_RMID, NULL);
	sem_destroy(&f_sem); 

	shmdt((void *) book_row_write);
	shm_id = get_shm_id(sizeof(int)*2, 'f');
	shmctl(shm_id, IPC_RMID, NULL);
}


/*----------METODI PER LA STAMPA----------*/
/*
 *Questa funzione utilizza l'array passatogli come argomento
 *per creare due nuovi array ordinati in modo decrescente sulla base del valore del bilancio;
 * - un array conterrà i balance più alti
 * - un array conterrà i balance più bassi
 *stampa i due array
*/
void print_limits(int size, balance_proc *balances){
	balance_proc top_b[NUM_LIMITS_PRINTS], bot_b[NUM_LIMITS_PRINTS];
	int j=0, i=0, index_shift;

	while( i < NUM_LIMITS_PRINTS ){
		top_b[i].balance = INT_MIN;
		bot_b[i].balance = INT_MAX;
		i++;
	}

	while( j<size && balances[j].pid!=0 ){
		/*  vedo se il valore balances[j].balance è tra i più alti visti fin'ora
		    in tal caso i valori pid e balance dell'array vengono aggiornati   */
		i=0;
		index_shift=NUM_LIMITS_PRINTS-1;
		if( balances[j].balance > top_b[index_shift].balance){
			
			while( i < NUM_LIMITS_PRINTS-1){
				if ( balances[j].balance > top_b[i].balance )
					index_shift--;
				i++;
			}
			/* i = NUM_LIMITS_PRINTS-1  */
			while( i > index_shift ){
				top_b[i].balance = top_b[i-1].balance;
				top_b[i].pid = top_b[i-1].pid;
				i--;
			}
			top_b[i].balance = balances[j].balance;
			top_b[i].pid = balances[j].pid;
		}

		/*  vedo se il valore balances[j].balance è tra i più bassi visti fin'ora
		    in tal caso i valori pid e balance dell'array vengono aggiornati    */
		i=0;
		index_shift=NUM_LIMITS_PRINTS-1;
		if( balances[j].balance < bot_b[index_shift].balance){
			
			while( i < NUM_LIMITS_PRINTS-1){
				if ( balances[j].balance < bot_b[i].balance )
					index_shift--;
				i++;
			}
			/* i = NUM_LIMITS_PRINTS-1  */
			while( i > index_shift ){
				bot_b[i].balance = bot_b[i-1].balance;
				bot_b[i].pid = bot_b[i-1].pid;
				i--;
			}
			bot_b[i].balance = balances[j].balance;
			bot_b[i].pid = balances[j].pid;
		}
		j++;
	}

	i=0;
	while(i<NUM_LIMITS_PRINTS){
		printf("\n   PID =  %d  \t balance = %d",top_b[i].pid, top_b[i].balance);
		i++;
	}
	printf("\n   -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  - ");
	i = NUM_LIMITS_PRINTS-1;
	while( i>=0 && bot_b[i].pid>0 && bot_b[i].balance>=0){
		printf("\n   PID =  %d  \t balance = %d",bot_b[i].pid, bot_b[i].balance);
		i--;
	}
}

/*
 *Questa funzione ordina l'array passatogli come argomento 
 *in ordine decrescente sulla base del valore del bilancio e lo stampa
*/
void print_all(unsigned int size, balance_proc *balances){
	int j=0, i=0, max , tmp_balance;
	pid_t tmp_pid;

	while( i<size && balances[i].pid!=0 ){/* ordina l'array */
		max = i;
		j = i + 1;
		while( j<size && balances[j].pid!=0 ){
			if( balances[j].balance > balances[max].balance)
				max = j ;
			j++;
		}
		tmp_balance = balances[i].balance;
		tmp_pid = balances[i].pid;

		balances[i].balance = balances[max].balance;
		balances[i].pid = balances[max].pid;

		balances[max].balance = tmp_balance;
		balances[max].pid = tmp_pid;
		i++;
	}
	
	i=0;
	while( i < size && balances[i].pid>0 && balances[i].balance>=0){
		printf("\n   PID =  %d  \t balance = %d",balances[i].pid, balances[i].balance);
		i++;
	}
}

/*
 *Questa funzione elabora una singola transazione,
 *in particolare una transazione tra due user, 
 *
 *questa funzione aggiunge al bilancio del user ricevente le somme ricevuti
 *e sottrae al bilancio del user inviante le somme inviati (sottraendo anche le somme trattenuti per il reward)
 *
 *se un user (ricevente o inviante) riferito dalla transazione, 
 *non è ancora presente nell'array dei bilanci dei user, allora esso viene aggiunto
*/
void calculate_user_transaction(transaction trans){
	int j=0, stop=0;
	
	/*receiver*/
	while( j<size_users && users_balance[j].pid!=0 && stop==0){
		if(users_balance[j].pid==trans.receiver){
			users_balance[j].balance+= trans.quantity;
			stop++;
		}
		j++;
	}
	if(users_balance[j].pid==0 && users_balance[j-1].pid!=trans.receiver){ 
		/*questo user non esiste in user_balance, lo devo aggiungere*/
		users_balance[j].pid = trans.receiver;
		users_balance[j].balance = SO_BUDGET_INIT + trans.quantity;
		num_users_proc++;
	}
	
	j=0;
	stop=0;
	/*sender*/
	while(j<size_users && users_balance[j].pid!=0 && stop==0){
		if(users_balance[j].pid==trans.sender){
			users_balance[j].balance = users_balance[j].balance - (trans.quantity + trans.reward);
			stop++;
		}
		j++;
	}
	if(users_balance[j].pid==0 && users_balance[j-1].pid!=trans.sender){ 
		/*questo user non esiste in user_balance, lo devo aggiungere*/
		users_balance[j].pid = trans.sender;
		users_balance[j].balance = SO_BUDGET_INIT - (trans.quantity + trans.reward);
		num_users_proc++;
	}
}

/*
 *Questa funzione elabora una singola transazione,
 *in particolare una transazione di reward, 
 *transazione adibita al pagamento del nodo che ha elaborato il blocco di transazioni
 *
 *questa funzione aggiunge il reward al bilancio del nodo (a cui la transazione fa riferimento)
 *
 *se il nodo riferito dalla transazione non è ancora presente nell'array dei bilanci dei nodi, 
 *allora esso viene aggiunto
*/
void calculate_node_transaction(transaction trans){
	int j=0, stop=0;

	while(j<size_nodes && nodes_balance[j].pid!=0 && stop==0){
		if(nodes_balance[j].pid==trans.receiver){
			nodes_balance[j].balance+= trans.quantity;
			stop++;
		}
		j++;
	}
	if(nodes_balance[j].pid==0 && nodes_balance[j-1].pid!=trans.receiver){ 
		/*questo user non esiste in nodes_balance, lo devo aggiungere*/
		nodes_balance[j].pid = trans.receiver;
		nodes_balance[j].balance = trans.quantity;
		num_nodes_proc++;
	}
}

/*
 *Questo metodo itera il libro mastro dall'ultima lettura fino allo stato di riempimento attuale
*/
void calculate_balance_sec(){
	while( book_row_print < book_row_write[0] && book_row_print<SO_REGISTRY_SIZE){
		while( book_block_print < SO_BLOCK_SIZE-1 ){
			/* analizzo su una singola transazione user */
			calculate_user_transaction(master_book[book_row_print].block[book_block_print]);
			book_block_print++;
		}
		/* analizzo su una singola transazione nodo */
		calculate_node_transaction(master_book[book_row_print].block[book_block_print]);
		book_block_print = 0;
		book_row_print++;
	}
}

/*
 * metodo che viene chiamato dal SIGALRM
 * Stampa ogni secondo:
 *  - il numero dei processi utente e nodo attivi
 *  - balance corrente di ogni processo utente e nodo (letti da libro mastro)
*/
void sig_print_sec(){
	struct msqid_ds data_queue;

	if(status_simulazione[0]==0 && sec < SO_SIM_SEC ){
		sec++;
		if( sec==SO_SIM_SEC )
			status_simulazione[0]=1;
		else
			alarm(1);

		printf("\n\n\n___________________SITUAZIONE AL SECONDO  %d \n",sec);

		/* controlla lo stato di occupazione della msg queue: */
/*		msgctl(queue_id, IPC_STAT, &data_queue);
		printf("\n msg queue bytes:  %ld / %ld",data_queue.msg_cbytes, data_queue.msg_qbytes);
*/
		printf("\n  Ci sono  %d  processi user attivi",num_active_users());
		printf("\n  Ci sono  %d  processi node attivi",num_active_nodes());
		fflush(stdout);
		calculate_balance_sec();

		printf("\n\n  Budget corrente dei processi USER (così come registrato nel libro mastro)");
		if( num_users_proc > switch_print )
			print_limits(num_users_proc, users_balance);
		else
			print_all(num_users_proc, users_balance);
		fflush(stdout);
		
		printf("\n\n  Budget corrente dei processi NODE (così come registrato nel libro mastro)");
		if( num_nodes_proc > switch_print )
			print_limits(num_nodes_proc, nodes_balance);
		else
			print_all(num_nodes_proc, nodes_balance);

		fflush(stdout);

	}
}

/*
 * stampo le statistiche finali contenete le informazioni di fine simulazione
*/
void print_end(){
	int i=0, count=0;
	
	switch( status_simulazione[0] ) {
		case 1: 
			printf("\n  La simulazione è finita perchè E' FINITO IL TEMPO");
			break;
		case 2: 
			printf("\n  La simulazione è finita perchè LO SPAZIO NEL LIBRO MASTRO E' FINITO");
			break;
		case 3: 
			printf("\n  La simulazione è finita perchè TUTTI I PROCESSI USER SONO MORTI");
			break;
		default:
			break;
	}

	if(status_simulazione[0]==3)
		printf("\n\n  Sono terminati prematuramente  %d  processi utente   ( tutti )",SO_USERS_NUM);
	else
		printf("\n\n  Sono terminati prematuramente  %d  processi utente",SO_USERS_NUM-num_active_users());

	printf("\n\n  Sono presenti  %d  blocchi nel libro mastro",book_row_write[0]);
	if( book_row_write[0] == SO_REGISTRY_SIZE )
		printf("   ( è pieno )");

	calculate_balance_sec();
	printf("\n\n  Budget finale dei processi USER (così come registrato nel libro mastro)");
	print_all(num_users_proc, users_balance);
	printf("\n\n  Budget finale dei processi NODO (così come registrato nel libro mastro)");
	print_all(num_nodes_proc, nodes_balance);

	i=0;
	printf("\n\n  Situazione nelle transaction pool;");
	while( i < size_nodes && active_nodes[i].pid!=0){
		printf("\n    pid = %d \t transazioni nella transaction pool = %d", active_nodes[i].pid, remaning_trans_in_tp[i]);
		i++;
	}
	printf("\n\n  Durante la simulazione sono stati creati  %d  nuovi processi nodo", i - SO_NODES_NUM );
}

/*--------------------------------*/
/*
 *metodo wrapper per la funzione node,
 *si occupa di creare l'ecosistema dei friends 
*/
void wrap_node(int node_index, transaction trans){
	int *f_node_friends, i=0, j, rand_index, redo=1;

	f_node_friends = (int*) malloc(sizeof(int)*size_friends);
	while( i < size_friends ){
		f_node_friends[i] = -1;
		i++;
	}

	i=0;
	while( i < SO_FRIENDS_NUM){
		/*nella lista amici ci vanno indici di nodi ancora non creati ma comunque3 possiamo già dire che per un determinato nodo
		 *il processo in posizione i-esima di active_nodes sarà amico dell'attuale quando nodo i-esimo verrà creato
		*/
		while( redo==1 ){
			redo=0;

			if(node_index < SO_NODES_NUM){
				rand_index = rand() % SO_NODES_NUM;
				if( rand_index == node_index )
					redo=1;
			}
			else{
				rand_index = rand() % size_nodes;
				if(active_nodes[rand_index].pid==0 || active_nodes[rand_index].active==0 || rand_index == node_index)
					redo=1;
			}
			
			j=0;
			/* indice non ancora esistente nell'array f_node_friends */
			while(redo==0 && j<SO_FRIENDS_NUM){
				if( f_node_friends[j]==rand_index )
					redo=1;
				j++;
			}
		}
		f_node_friends[i] = rand_index;
		i++;
	}

	switch( fork() ) {/* creazione del processo nodo */
		case -1:
			TEST_ERROR;
			exit(EXIT_FAILURE);
		case 0:
			node(f_node_friends, trans, node_index);
			exit(EXIT_SUCCESS);
			break;
		default:
			break;
		}
	
}
/*-------------SIGNALS-----------------*/

/*
 *metodo che viene chiamato dal SIGUSR2:
 * crea un nuovo processo nodo
*/
void sig_create_new_node_proc(){
	transaction trans;
	int stop=0, i=0, node_index;
	
	/* cerchiamo la transazione nell'array trans_hops*/
	while( stop==0 && i < size_t_hops){
		if( trans_hops[i].count >= SO_HOPS ){
			trans_hops[i].count=0;
			stop=1;
		}
		else{
			i++;
		}
	}
	trans.sender     = trans_hops[i].trans.sender;
	trans.receiver   = trans_hops[i].trans.receiver;
	trans.time_stamp.tv_sec  = trans_hops[i].trans.time_stamp.tv_sec;
	trans.time_stamp.tv_nsec = trans_hops[i].trans.time_stamp.tv_nsec;
	trans.quantity   = trans_hops[i].trans.quantity;
	trans.reward     = trans_hops[i].trans.reward;
	/* cerca l'indice del nuovo nodo */
	i = 0;
	stop=0;
	while( i < size_nodes && stop == 0 ){
		if( active_nodes[i].pid == 0 ){
			node_index = i;
			stop = 1;
		}
		i++;
	}
	stop=0;
	/* 
	 *ordina a vari proc nodo di aggiungere questo nuovo proc alla lista di friend 
	 *aspetto che lo scorso nodo agiunto sia friend di SO_FRIENDS_NUM processi 
	*/
	while(stop==0 && status_simulazione[0]==0){
		sem_wait(&f_sem);
		if(friend_to_add[0]<=0){
			friend_to_add[0] = SO_FRIENDS_NUM;
			friend_to_add[1] = node_index;
			stop=1;
		}
		sem_post(&f_sem);
	}
	wrap_node(node_index, trans);
}

/*
 * metodo handler per la gestione dei signal,
 * controlla anche la natura del processo a cui viene inviato il signal
*/
static void sig_handler(int sig){
	int i=0, stop=0;
	switch(sig){

		case SIGALRM:
			sig_print_sec();
			break;

		case SIGUSR1:
			for(i=0; i<size_users && stop==0; i++){
				if(active_users[i].pid==getpid()){
					stop=1;
				}
			}
			for(i=0; i<size_nodes && stop==0; i++){
				if(active_nodes[i].pid==getpid()){
					stop=1;
				}
			}
			if(stop==0)
				sig_create_new_node_proc();
			break;

		case SIGUSR2:
			/* controlliamo che siamo un proc user */
			for(i=0; i<size_users && stop==0; i++){
				if(active_users[i].pid==getpid()){
					stop=1;
					sig_create_and_send_transaction();
				}
			}
			break;

		default:
			break;
	}
}

/*
 *modifica il signal handler di default dei i signal usati nella simulazione
 *in modo tale che eseguano codice personalizzato da noi
*/
void set_signals() {
	void (*oldHandler) (int);

	oldHandler = signal(SIGALRM, sig_handler);
	if (oldHandler == SIG_ERR) {
		printf("\nsignal error! SIGALRM\n");
		exit(EXIT_FAILURE);
	}

	oldHandler = signal(SIGUSR1, sig_handler);
	if (oldHandler == SIG_ERR) {
		printf("\nsignal error! SIGUSR1\n");
		exit(EXIT_FAILURE);
	}

	oldHandler = signal(SIGUSR2, sig_handler);
	if (oldHandler == SIG_ERR) {
		printf("\nsignal error! SIGUSR2\n");
		exit(EXIT_FAILURE);
	}
}

/*--------------------------------*/
/*
 * questo metodo si occupa di terminare i rimanenti processi attivi
*/
void kill_all_proc(){
	int i=0;

	while( i < size_users ){
		if(active_users[i].pid!=0 && active_users[i].active==1)
			kill(active_users[i].pid, SIGTERM);
		i++;
	}

	i=0;
	while( i < size_nodes){
		if(active_nodes[i].pid!=0 && active_nodes[i].active==1)
			kill(active_nodes[i].pid, SIGTERM);
		i++;
	}
}

/*--------------------------------*/
/* 
 *METODO DI DEBUG, usato per controllare le varie risorse
*/
void print_debug(){
	int i, j, shm_id;
/*
	printf("\n\n\t master_book:\n");
	for(i=0;i<SO_REGISTRY_SIZE && i< book_row_write[0];i++){
		printf("\n---------------ROW = %d    from node = %d", i, master_book[i].block[SO_BLOCK_SIZE-1].receiver);
		for(j=0;j<SO_BLOCK_SIZE;j++){
			printf("\n  block[%d]   s:%d\tr:%d ts: %ld  tn:%ld\tq:%d\tr:%d",j,master_book[i].block[j].sender, master_book[i].block[j].receiver, master_book[i].block[j].time_stamp.tv_sec, master_book[i].block[j].time_stamp.tv_nsec, master_book[i].block[j].quantity ,master_book[i].block[j].reward );
		}
	}

	printf("\n\n\t active_users:\n");
	for(i=0; i<size_users && active_users[i].pid!=0;i++){
		printf("\n     [%d] \t pid:%d \t was active untill end: %d", i, active_users[i].pid, active_users[i].active);
	}

	printf("\n\n\t active_nodes:\n");
	for(i=0; i<size_nodes && active_nodes[i].pid!=0;i++){
		printf("\n     [%d] \t pid:%d \t was active untill end: %d", i, active_nodes[i].pid, active_nodes[i].active);
	}

	printf("\n\n\t trans_hops:\n");
	for(i=0; i<size_t_hops ;i++){
		if(trans_hops[i].count>0)
			printf("\n     [%d] \t count hops:%d\t of transaction s:%d \tr:%d ts: %ld tn:%ld", i, trans_hops[i].count,trans_hops[i].trans.sender, trans_hops[i].trans.receiver, trans_hops[i].trans.time_stamp.tv_sec, trans_hops[i].trans.time_stamp.tv_nsec);
	}
*/
	printf("\n\n  Situazione nella SHM, id creati;");
	shm_id = get_shm_id(SO_REGISTRY_SIZE * sizeof(single_block), 'x');
	printf("\n\t shm_id of master_book  %d",shm_id);
	shm_id = get_shm_id(size_users * sizeof(id_proc), 'y');
	printf("\n\t shm_id of active_users  %d",shm_id);
	shm_id = get_shm_id(size_nodes * sizeof(id_proc), 'z');
	printf("\n\t shm_id of active_nodes  %d",shm_id);
	shm_id = get_shm_id(size_t_hops * sizeof(count_attempt), 'h');
	printf("\n\t shm_id of trans_hops  %d",shm_id);
	shm_id = get_shm_id(sizeof(int)*size_nodes, 't');
	printf("\n\t shm_id of remaning_trans_in_tp  %d",shm_id);
	shm_id = get_shm_id(sizeof(int)*1, 's');
	printf("\n\t shm_id of status_simulazione  %d",shm_id);
	shm_id = get_shm_id(sizeof(int)*1, 'b');
	printf("\n\t shm_id of book_row_write  %d",shm_id);
	shm_id = get_shm_id(sizeof(int)*2, 'f');
	printf("\n\t shm_id of friend_to_add  %d",shm_id);
}
 



int main(int argc, char const *argv[]){
	int i;
	transaction trans;

/*----------INIZIO----FASE----SET-UP--------------------*/

	read_file_config();

	set_global_variables();

	set_shared_memory();

	set_signals();

/*----------FINITA----FASE----SET-UP--------------------*/
/*------------------------------------------------------*/
/*----------INIZIO----FASE----PRINCIPALE----------------*/
	printf ("\n\n\n################################################ INIZIO SIMULAZIONE\n");
	i=0;
	srand(getpid());
	alarm(1); /* da qui inizia ufficialmente il tempo della simulazione */
	while(i < SO_NODES_NUM || i < SO_USERS_NUM) {
		if (i < SO_NODES_NUM) {
			wrap_node(i, trans);
		}
		
		if (i < SO_USERS_NUM) {
			switch( fork() ) {/* creazione del processo user */
				case -1:
					TEST_ERROR;
					exit(EXIT_FAILURE);
				case 0:
					user(i);
					exit(EXIT_SUCCESS);
					break;
				default:
					break;
			}
		}
		i++;
	}
	/* tutti i processi nodo e user iniziali sono stati creati */

	while( status_simulazione[0]==0 )
		pause(); /* attesa attiva mentre aspettiamo che finisca la simulazione*/

	printf ("\n\n\n################################################ FINE SIMULAZIONE\n");
/*----------FINITA----FASE----PRINCIPALE----------------*/
/*------------------------------------------------------*/
/*----------INIZIO----FASE----FINALE--------------------*/

	print_end();

	kill_all_proc();

/*  METODO DI DEBUG, usato per controllare le varie risorse  */
	print_debug();

	dealloc_mem();

/*----------FINITA----FASE----FINALE--------------------*/
	printf("\n\n");
	return 0;
}