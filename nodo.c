
#include "master.h"

int *friends;
int node_index;
int actual; /*indice ultima transazione inserita dentro trans_pool*/
int start_read; /*indice inizio lettura trans_pool (NON CODA). Incrementa a 'salti' di dimensione SO_BLOCK_SIZE - 1*/
transaction *trans_pool;

/*
 *Metodo che ispeziona la memoria condivisa in cui viene salvato l'elenco dei processi nodo attivi.
 *Ritorna un intero, cioè proprio il numero di processi nodo attivi.
*/
int num_active_nodes() {
	int num = 0, i = 0;

	while (i < size_nodes) {
		if (active_nodes[i].active == 1)
			num++;
		i++;
	}
	return num;
}

/*
 *metodo che aggiunge all'array di friends l'indice contenuto in friend_to_add[1] 
 *e decrementa friend_to_add[0] (il numero dei processi nodo rimanenti che devono aggiungere il nodo amico)
*/
void add_new_friend(){
	int stop=0, i=0;

	while( i < size_friends && stop == 0){
		if(friends[i] == -1){
			stop = 1;
			friends[i] = friend_to_add[1];
			friend_to_add[0]--;
		}
		i++;
	}
}

/*
 *manda la transazione a un nodo amico estratto a caso.
 *per ogni volta che la medesima transazione viene inviata ad un amico
 *trans_hops[].count della transazione viene incrementato 
*/
void send_transaction_to_friend(pid_t pid, transaction trans){
	int i, stop=0, rand_index = 0;
	pid_t node_sel = 0;
	msgbuf msg_arg;

	i=0;
	/* cerchiamo la transazione nell'array trans_hops*/
	while( stop==0 && i < size_t_hops){
		if( trans_hops[i].trans.sender == trans.sender &&
			trans_hops[i].trans.receiver == trans.receiver &&
			trans_hops[i].trans.time_stamp.tv_sec == trans.time_stamp.tv_sec &&
			trans_hops[i].trans.time_stamp.tv_nsec == trans.time_stamp.tv_nsec )
		{
			trans_hops[i].count++;
			stop = 1;
			if(trans_hops[i].count >= SO_HOPS)
				kill(getppid(), SIGUSR1);
		}
		i++;
	}
	i=0;
	/* se non ha trovato la transazione, la aggiunge all'array trans_hops e incrementa il count*/
	while( stop==0 && i < size_t_hops){
		if( trans_hops[i].count == 0 ){
			stop=1;
			trans_hops[i].trans = trans;
			trans_hops[i].count++;
		}
		i++;
	}

	while (node_sel == 0 || node_sel == pid || active_nodes[ friends[rand_index] ].active==0) {
		do{
			rand_index = rand()% size_friends;
			node_sel = active_nodes[ friends[rand_index] ].pid;
		}while(friends[rand_index] == -1);
	}

	msg_arg.mtype = rand_index;
	sprintf(msg_arg.msg_trans,"%d,%d,%ld,%ld,%d,%d,",trans.sender, trans.receiver, trans.time_stamp.tv_sec, trans.time_stamp.tv_nsec, trans.quantity ,trans.reward );

	msgsnd(queue_id, (void*)&msg_arg, MSG_LENGTH, 0);
}

/*
 *Funzione che legge le transazioni presenti nella coda di messaggi.
 *Una volta letta la scrive ('memorizza privatamente') dentro transaction pool proprietaria del nodo se non è piena.
 *se la transaction pool è piena la transazione viene inviata a un nodo amico
*/
void receive_trans(pid_t pid) {
	int full_tp = 0, i, stop=0, count=0;
	msgbuf msg_arg;
	struct msqid_ds buf;
	transaction t_received;

	msgctl(queue_id, IPC_STAT, &buf);
	if(buf.msg_qnum <= 0)
		stop=1;

	while( stop==0 && status_simulazione[0]==0){ /* finchè ho transazioni da leggere dalla queue */
		if( msgrcv(queue_id, &msg_arg, MSG_LENGTH, node_index, 0) < MSG_LENGTH ){
			stop=1;
		} else {
			if ( actual != -1 && ( actual+1 == start_read || (start_read == 0 && actual == SO_TP_SIZE-1)) ){
				full_tp = 1;
			} else {
				actual++;
				if (actual == SO_TP_SIZE)
					actual = 0; /*Simulo il comportamento di array circolare mod SO_TP_SIZE*/
			}
			
			t_received.sender = atoi(strtok(msg_arg.msg_trans,","));
			t_received.receiver = atoi(strtok(NULL,","));
			t_received.time_stamp.tv_sec = atol(strtok(NULL,","));
			t_received.time_stamp.tv_nsec = atol(strtok(NULL,","));
			t_received.quantity = atoi(strtok(NULL,","));
			t_received.reward = atoi(strtok(NULL,","));


			if (full_tp == 0) { /*ho spazio nella tp, posso salvare le transazioni.*/
				trans_pool[actual].sender     = t_received.sender;
				trans_pool[actual].receiver   = t_received.receiver;
				trans_pool[actual].time_stamp.tv_sec = t_received.time_stamp.tv_sec;
				trans_pool[actual].time_stamp.tv_nsec = t_received.time_stamp.tv_nsec;
				trans_pool[actual].quantity   = t_received.quantity;
				trans_pool[actual].reward     = t_received.reward;
				
					
				i=0;
				while( i < size_t_hops){
					if( trans_hops[i].trans.sender == trans_pool[actual].sender &&
						trans_hops[i].trans.receiver == trans_pool[actual].receiver &&
						trans_hops[i].trans.time_stamp.tv_sec == trans_pool[actual].time_stamp.tv_sec &&
						trans_hops[i].trans.time_stamp.tv_nsec == trans_pool[actual].time_stamp.tv_nsec  )
							trans_hops[i].count = 0;
					i++;
				}
			}else { /* tp è piena */
				/*Manda transazioni in eccesso a un nodo amico*/ 
				if( t_received.sender!=0 &&
					t_received.receiver!=0 &&
					t_received.time_stamp.tv_sec!=0 &&
					t_received.time_stamp.tv_nsec!=0 &&
					t_received.quantity!=0 &&
					t_received.reward!=0 )
				{	
					count++;
					/*count evita la situazione in cui i nodi non fanno altro che 
					 *inoltrarsi a vicenda le transazioni
					*/
					if(count >= SO_TP_SIZE*2 )
						stop=1;
					send_transaction_to_friend(pid, t_received); 
				}
			}
			msgctl(queue_id, IPC_STAT, &buf);
			if(buf.msg_qnum <= 0)
				stop=1;
		}
	}
}
/*
 *Questo metodo controlla che la tp sia in regola, in tal caso ritorna 0
 *se non è in regola modifica le variabili globali actual e start read ed aventualmente rimpiazza 
 *la transaction pool con la versiona corretta
*/
int check_tp(){
	int j=0, i=0, k=0, count=0, stop=0, ok=1, check_actual;
	transaction *old_tp;

	if(actual == -1)
		return 1;
	/* conto quante transazioni valide ci sono */
	for(j=0; j < SO_TP_SIZE ;j++){
		if( trans_pool[j].sender!=0 &&
			trans_pool[j].receiver!=0 &&
			trans_pool[j].time_stamp.tv_sec!=0 &&
			trans_pool[j].time_stamp.tv_nsec!=0 &&
			trans_pool[j].quantity!=0 &&
			trans_pool[j].reward!=0 )
			count++;
	}
	if(count==0){ /* non ci sono transazioni valide*/
		start_read = 0;
		actual = -1;
		return 1;
	}

	i = start_read;
	for(j=0; j < SO_TP_SIZE && stop==0;j++){
		if( i == actual )
			stop=1;
		if( trans_pool[i].sender==0 ||
			trans_pool[i].receiver==0 ||
			trans_pool[i].time_stamp.tv_sec==0 ||
			trans_pool[i].time_stamp.tv_nsec==0 ||
			trans_pool[i].quantity==0 ||
			trans_pool[i].reward==0 )
			ok = 0;
		
		i++;
		if(i == SO_TP_SIZE)
			i=0;
	}

	/* tutte le transazioni valide sono contenute tra start_read e actual */
	check_actual = start_read + ( j-1 );
	if( check_actual >= SO_TP_SIZE )
		check_actual = check_actual - SO_TP_SIZE;
	if(count == j && ok==1 && actual==check_actual) 
		return 0; /* tutto in regola*/
	

	/* 
	 *se arrivo qui significa che ci sono transazioni valide nella tp ma non sono in ordine,
	 *risolviamo la cosa rimpiazzando la tp con un'altro array copiando 
	 *in modo ordinato le transazioni valide e aggiornando i campi start_read e actual
	*/
	old_tp = trans_pool;
	i = start_read;
	k = -1;
	trans_pool = (transaction *) malloc(sizeof(transaction) * SO_TP_SIZE);
	for(j=0; j < SO_TP_SIZE ;j++){
		if( old_tp[i].sender!=0 &&
			old_tp[i].receiver!=0 &&
			old_tp[i].time_stamp.tv_sec!=0 &&
			old_tp[i].time_stamp.tv_nsec!=0 &&
			old_tp[i].quantity!=0 &&
			old_tp[i].reward!=0 )
		{
			k++;
			trans_pool[k].sender = old_tp[i].sender;
			trans_pool[k].receiver = old_tp[i].receiver;
			trans_pool[k].time_stamp.tv_sec = old_tp[i].time_stamp.tv_sec;
			trans_pool[k].time_stamp.tv_nsec = old_tp[i].time_stamp.tv_nsec;
			trans_pool[k].quantity = old_tp[i].quantity;
			trans_pool[k].reward = old_tp[i].reward;
		}
		i++;
		if(i==SO_TP_SIZE)
			i=0;
	}
	free(old_tp);
	start_read = 0;
	actual = k;
	remaning_trans_in_tp[node_index] = k+1;

	return 0;
}

/*
 *Funzione che legge dall'array transaction pool esattamente SO_BLOCK_SIZE-1 transazioni.
 *scrive ogni transazione nel libro mastro e somma tutte le reward per poi creare una transazione finale
 *di "pagamento" che si auto-assegna.
 *Per scrivere nel libro mastro occorre usare un semaforo. 
*/
void write_master_book(pid_t pid) {
	int sum = 0, count;
	transaction empty_trans;

	if( sem_wait(&mb_sem) < 0 ){
		perror("\nsemaforo non fa sem_wait");
		exit(EXIT_FAILURE);
	}

	if( status_simulazione[0]==0 && book_row_write[0] < SO_REGISTRY_SIZE ) {
		for (count = 0; count < SO_BLOCK_SIZE-1 ; count++) {
			master_book[book_row_write[0]].block[count].sender = trans_pool[start_read].sender;
			master_book[book_row_write[0]].block[count].receiver = trans_pool[start_read].receiver;
			master_book[book_row_write[0]].block[count].time_stamp.tv_sec = trans_pool[start_read].time_stamp.tv_sec;
			master_book[book_row_write[0]].block[count].time_stamp.tv_nsec = trans_pool[start_read].time_stamp.tv_nsec;
			master_book[book_row_write[0]].block[count].quantity = trans_pool[start_read].quantity;
			master_book[book_row_write[0]].block[count].reward = trans_pool[start_read].reward;

			sum += trans_pool[start_read].reward;

			/* cancella dalla tp transazione appena processata*/
			trans_pool[start_read].sender = 0;
			trans_pool[start_read].receiver = 0;
			trans_pool[start_read].time_stamp.tv_nsec = 0;
			trans_pool[start_read].time_stamp.tv_sec = 0;
			trans_pool[start_read].quantity = 0;
			trans_pool[start_read].reward = 0;

			start_read++;
			if (start_read == SO_TP_SIZE)
				start_read = 0;
		}

		/*scrive reward trans, qui count==SO_BLOCK_SIZE-1*/
		master_book[book_row_write[0]].block[count].sender = -1;
		master_book[book_row_write[0]].block[count].receiver = pid;
		master_book[book_row_write[0]].block[count].quantity = sum;
		master_book[book_row_write[0]].block[count].reward = 0;
		clock_gettime(CLOCK_REALTIME, &master_book[book_row_write[0]].block[count].time_stamp);

		book_row_write[0]++;
		/*  se masterbook termina la propria capienza allora settiamo status_simulazione = 2  */
		if( book_row_write[0] >= SO_REGISTRY_SIZE )
			status_simulazione[0]=2;
	}

	if( sem_post(&mb_sem) < 0 ){
		perror("\nsemaforo non fa sem_post");
		exit(EXIT_FAILURE);
	}
}

/*
 *Genera randomicamente, nel range di tempo dato, un intervallo di attesa che deve effettuare ogni nodo per simulare
 *l'elaborazione di un blocco di transazioni.
*/
void nanosleep_proc(){
	struct timespec tim, tim_rem;
	tim.tv_sec = 0;
	if( SO_MAX_TRANS_PROC_NSEC == SO_MIN_TRANS_PROC_NSEC ){
		tim.tv_nsec = SO_MAX_TRANS_PROC_NSEC;
	}else{
		tim.tv_nsec = SO_MIN_TRANS_PROC_NSEC + ( rand()%(SO_MAX_TRANS_PROC_NSEC-SO_MIN_TRANS_PROC_NSEC) );
	}
	nanosleep(&tim, &tim_rem);
}


/*
 *Ogni nodo deve: -ricevere transazioni dai processi user
 *                -leggere dalla transaction pool SO_BLOCK_SIZE - 1 transazioni 
 *				  -oltre queste transazioni aggiungere transazione di reward (vedere specifiche nel file del progetto)
 *				  -scrivere nel libro mastro le transazioni e eliminarle dalla transaction pool
 *				  -simulare elaborazione blocco attendendo tra SO_MIN_TRANS_PROC_NSEC e SO_MIN_TRANS_PROC_NSEC
 *			  -ripete.
*/
void node(int *f_node_friends, transaction first_trans, int f_node_index) {
	int aviable_transaction=0, rand_index, error=0;
	pid_t node_pid;
	transaction new_trans;

	srand(getpid());
	actual = -1;
	start_read = 0;
	node_index = f_node_index;
	friends = f_node_friends;

	node_pid = getpid();
	active_nodes[node_index].pid = node_pid;
	active_nodes[node_index].active = 1;

	trans_pool = (transaction *)malloc(sizeof(transaction) * SO_TP_SIZE);

	if( node_index >= SO_NODES_NUM ){
		actual = 0;
		trans_pool[actual].sender     = first_trans.sender;
		trans_pool[actual].receiver   = first_trans.receiver;
		trans_pool[actual].time_stamp.tv_sec  = first_trans.time_stamp.tv_sec;
		trans_pool[actual].time_stamp.tv_nsec = first_trans.time_stamp.tv_nsec;
		trans_pool[actual].quantity   = first_trans.quantity;
		trans_pool[actual].reward     = first_trans.reward;
	}
/*test del signal*//*
	if(node_index==SO_NODES_NUM-1)
		kill(getppid(), SIGUSR1);
*/
	/*questo finchè non riceve la prima transazione*/
	while( actual == -1 )
		receive_trans(node_pid);

	while( status_simulazione[0] == 0 ){
		error = 0;
		receive_trans(node_pid);

		error = check_tp();

		sem_wait(&f_sem);
		if( friend_to_add[0] > 0 )/* inizializzato a SO_NUM_FRIENDS quando viene creato il nodo con indice friend_to_add[1]*/
			add_new_friend();
		sem_post(&f_sem);


		if( error==0 && rand()%10 == 0 ){ /* invia transazione a caso a un nodo amico*/
			do{
				rand_index = rand()%SO_TP_SIZE;
			}while( ( rand_index>actual  && actual>=start_read ) || 
					( rand_index<start_read && start_read<=actual ) || 
					( actual<rand_index  && rand_index<start_read ) );

			send_transaction_to_friend(node_pid, trans_pool[rand_index]);
			trans_pool[rand_index] = trans_pool[actual];
			trans_pool[actual].sender = 0;
			trans_pool[actual].receiver = 0;
			trans_pool[actual].time_stamp.tv_nsec = 0;
			trans_pool[actual].time_stamp.tv_sec = 0;
			trans_pool[actual].quantity = 0;
			trans_pool[actual].reward = 0;

			actual--;
			if( actual < 0 )
				actual = SO_TP_SIZE-1;
		}

		error = check_tp();

		/*calcolo le transazioni presenti nella transaction pool*/
		if ( actual >= start_read )
			aviable_transaction = actual - start_read + 1;
		else 
			aviable_transaction = (actual + 1) + (SO_TP_SIZE - start_read);

		if( error==0 && aviable_transaction >= SO_BLOCK_SIZE-1){
			write_master_book(node_pid);
			aviable_transaction = aviable_transaction - (SO_BLOCK_SIZE-1);
			nanosleep_proc();
		}
		if(error==0)
			remaning_trans_in_tp[node_index] = aviable_transaction;
	
	}

	/*	Sto per morire!		*/ 
	free(trans_pool);
	free(friends);
	active_nodes[node_index].active = 0;
	exit(EXIT_SUCCESS);
}
