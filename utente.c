
#include "master.h"

int user_index;
int budget;
int size_not_official;
transaction *not_official;

/*
 *Metodo che ispeziona la memoria condivisa in cui viene salvato l'elenco dei processi utente attivi.
 *Ritorna un intero, cioè proprio il numero di processi utente attivi.
*/
int num_active_users() {
	int num = 0, i = 0;

	while (i < size_users) {
		if (active_users[i].active == 1)
			num++;
		i++;
	}
	return num;
}


/*
 *Funzione che aggiorna il budget ogni volta leggendo il libro mastro (come da specifiche consegna progetto)
*/
void set_budget(pid_t pid) {
	int i=0, j, k;
	budget = SO_BUDGET_INIT;

	while (i < book_row_write[0] && i < SO_REGISTRY_SIZE) {
		j = 0;
		while (j < SO_BLOCK_SIZE-1 ) {

			if(pid == master_book[i].block[j].receiver)
				budget = budget + master_book[i].block[j].quantity;

			if(pid == master_book[i].block[j].sender){
				budget = budget - (master_book[i].block[j].quantity + master_book[i].block[j].reward);

				for(k=0;k<size_not_official;k++){
					if( not_official[k].sender == master_book[i].block[j].sender &&
						not_official[k].receiver == master_book[i].block[j].receiver &&
						not_official[k].time_stamp.tv_sec == master_book[i].block[j].time_stamp.tv_sec &&
						not_official[k].time_stamp.tv_nsec == master_book[i].block[j].time_stamp.tv_nsec  )
					{
						not_official[k].sender=0;
					}
				}
			}
			j++;
		}
		i++;
	}
	for(k=0;k<size_not_official;k++){
		if( not_official[k].sender == pid){
			budget = budget - (not_official[k].quantity + not_official[k].reward);
		}
	}
}

/*
 *Funzione che crea una nuova transazione estraendo a caso il receiver, quantità, reward per il nodo.
*/
transaction create_new_trans(pid_t pid) {
	int rand_user_index;
	transaction new_trans;

	new_trans.sender = pid;
	new_trans.receiver = 0;

	do { /*estrae a caso destinatario */
		rand_user_index = rand() % size_users;
		new_trans.receiver = active_users[rand_user_index].pid;
	}while(new_trans.receiver == pid || new_trans.receiver == 0 || active_users[rand_user_index].active == 0 );

	if (clock_gettime(CLOCK_REALTIME, &new_trans.time_stamp) == -1) {
		TEST_ERROR;
		exit(EXIT_FAILURE);
	}

	if( budget > 2 ) {
		do{
			new_trans.quantity = rand() % budget;
		}while( new_trans.quantity<1 ); /*estrae a caso quantità*/

		new_trans.reward = (new_trans.quantity * SO_REWARD)/100;
		if (new_trans.reward == 0)
			new_trans.reward = 1;
		new_trans.quantity = new_trans.quantity - new_trans.reward;
		if (new_trans.quantity == 0)
			new_trans.quantity = 1;
	} 
	if(budget==2){
		new_trans.reward = 1;
		new_trans.quantity = 1;
	}

	return new_trans;
}

/*
 *Questa funzione si occupa di inviare la transazione appena creata ad un nodo destinatario che la dovrà processare.
 *Come parametro viene passata la nuova transazione appena creata e estrae casualmente un processo nodo destinatario.
 *Estratto il nodo scrive la transazione nella coda di messaggi con mtype = indice del nodo destinatario.
*/
int send_trans(transaction trans) {
	pid_t node_sel = 0;
	int rand_index = 0;
	msgbuf msg_arg;

	while (node_sel == 0 || active_nodes[ rand_index ].active==0) {
		rand_index = rand()% size_nodes;
		node_sel = active_nodes[ rand_index ].pid;
	}
	msg_arg.mtype = rand_index;
	sprintf(msg_arg.msg_trans,"%d,%d,%ld,%ld,%d,%d,",trans.sender, trans.receiver, trans.time_stamp.tv_sec, trans.time_stamp.tv_nsec, trans.quantity ,trans.reward );

	if( (msgsnd(queue_id, (void*)&msg_arg, MSG_LENGTH, 0) ) < 0 ){
		return -1;
	}
	return 1;
}

/*
 *La seguente funzione crea una nanosleep di valore estratto a caso tra SO_MIN_TRANS_GEN_NSEC e SO_MIN_TRANS_GEN_NSEC.
 *serve ad ogni utente dopo aver inviato una transazione.
*/
void nanosleep_gen() {
	struct timespec tim_req, tim_rem;

	tim_req.tv_sec = 0;
	if (SO_MAX_TRANS_GEN_NSEC == SO_MIN_TRANS_GEN_NSEC) {
		tim_req.tv_nsec = SO_MAX_TRANS_GEN_NSEC;
	}
	else {
		tim_req.tv_nsec = SO_MIN_TRANS_GEN_NSEC + (rand()% (SO_MAX_TRANS_GEN_NSEC - SO_MIN_TRANS_GEN_NSEC));
	}
	nanosleep(&tim_req, &tim_rem);
}

/*
 *metodo che viene chiamato dal SIGUSR1,
 *crea e invia una nuova transazione
*/
void sig_create_and_send_transaction(){
	transaction new_trans;
	int i=0, stop=0;

	set_budget(getpid());

	if (budget >= 2) {
		new_trans = create_new_trans(getpid());
		
		if (new_trans.quantity > 0 && send_trans(new_trans) > 0) {

			for(i=0; i<size_not_official && stop==0;i++){
				if(not_official[i].sender==0){
					stop=1;
					not_official[i] = new_trans;
				}
			}	
		}
	}
}

/*
 *Ogni utente deve: -calcolare il bilancio corrente (somma alagebrica entrate, uscite e sottrarre pending)
 *					-se bilancio >= 2 allora estrae a caso altro utente, nodo, quantità (parte della quantità è reward)
 *					-invia a nodo transazione da processare
 *					-aspetta tempo tra SO_MIN_TRANS_GEN_NSEC e SO_MIN_TRANS_GEN_NSEC
 *				-ripete.
 *se non manda transazione entro SO_RETRY volte muore e setta active_user[user_index].active = 0
*/
void user(int f_user_index) {
	int i = 0, retry_count = 0, error=0;
	pid_t user_pid;
	transaction new_trans;

	srand(getpid());
	size_not_official = 500;
	not_official = (transaction*) malloc(sizeof(transaction)*size_not_official);

	user_index = f_user_index;	
	user_pid = getpid();
	active_users[user_index].pid = user_pid;
	active_users[user_index].active = 1;

	while (retry_count < SO_RETRY && status_simulazione[0]==0) {
		error=0;
		set_budget(user_pid);
		/* settato budget */
		
		if ( budget < 2 )
			error=1;

		if( error==0 ){
			new_trans = create_new_trans(user_pid);
			if (new_trans.quantity <= 0){
				error=1;
			}
		}
		
		if ( error==0 && send_trans(new_trans) > 0) {
			retry_count = 0;

			not_official[i] = new_trans;
			i++;
			if(i==size_not_official)
				i=0;

			nanosleep_gen();
		}else{
			error=1;
		}

		if(error==1)
			retry_count++;
	}

	/*	Sto per morire!		*/ 
	active_users[user_index].active = 0;
	free(not_official);

	if ( num_active_users() <= 1 && status_simulazione[0]==0 ) 
		status_simulazione[0]=3;

	exit(EXIT_SUCCESS);
}