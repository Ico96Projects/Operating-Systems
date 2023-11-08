
#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/signal.h>
#include <time.h>
#include <fcntl.h>
#include <float.h>
#include <stdbool.h>
#include <semaphore.h>

#define TEST_ERROR    if (errno) {fprintf(stderr, \
					  "%s:%d: PID=%5d: Error %d (%s)\n", \
					  __FILE__,			\
					  __LINE__,			\
					  getpid(),			\
					  errno,			\
					  strerror(errno));}

#define MSG_LENGTH 100
#define NUM_LIMITS_PRINTS 10
#define FILE_FOR_SHM "master.c"
#define FILE_FOR_QUEUE "utente.c"

/*	conf 1 	*/
/*
#define SO_BLOCK_SIZE 100
#define SO_REGISTRY_SIZE 1000

/*	conf 2 	*/

#define SO_BLOCK_SIZE 10
#define SO_REGISTRY_SIZE 10000

/*	conf 3 	*/
/*
#define SO_BLOCK_SIZE 10
#define SO_REGISTRY_SIZE 1000


/* Variabili globali riempite in runtime dal metodo read_file_config() */
extern int SO_USERS_NUM;
extern int SO_NODES_NUM;
extern int SO_BUDGET_INIT;
extern int SO_REWARD;
extern int SO_MIN_TRANS_GEN_NSEC;
extern int SO_MAX_TRANS_GEN_NSEC;
extern int SO_RETRY;
extern int SO_TP_SIZE;
extern int SO_MIN_TRANS_PROC_NSEC;
extern int SO_MAX_TRANS_PROC_NSEC;
extern int SO_SIM_SEC;
extern int SO_FRIENDS_NUM;
extern int SO_HOPS;

extern sem_t mb_sem;
extern sem_t f_sem;
extern int size_users;
extern int size_nodes;
extern int size_friends;
extern int size_t_hops;
extern int queue_id;

/*
 *Struct che rappresenta una transazione e contiene tutte le informazioni necessarie a definirla e identificarla.
*/
typedef struct {
	pid_t sender;
	pid_t receiver;
	struct timespec time_stamp;
	int quantity;
	int reward;
} transaction;

/*
 *Struct che rappresenta un blocco di transazioni.
 *Un array di queste struct costituisce il libro mastro.
*/
typedef struct {
	transaction block[SO_BLOCK_SIZE];
} single_block;

/*
 *Struct che associa una transazione al proprio counter per i SO_HOPS
 * una transazione viene scritta nell'array di queste struct quando viene inoltrata a un nodo amico
 * una transazione viene eliminata da questo array se:
 *		entra in una transaction pool
 *		count == SO_HOPS (in tal caso viene creato un nuovo processo node)
*/
typedef struct {
	transaction trans;
	int count;
} count_attempt;

/*
 *Struct che identifica ogni processo.
 *Oltre a darci informazioni su quale processo sia (pid) ci dice anche la sua situazione:
 *'1' se attivo, '0' se morto.
*/
typedef struct {
	pid_t pid;
	int active; /* active = { 1, 0 } */
} id_proc;

/*
 *Struct per la stampa: rappresenta la situazione economica di un processo.
 *Questi campi verranno riempiti con dati provenienti solamente dal libro mastro.
 *Useremo un array di queste struct per effettuare la stampa ogni secondo.
*/
typedef struct {
	pid_t pid;
	int balance;
} balance_proc;

/*
 *Struct necessaria alle funzioni send e receive della coda di messaggi
*/
typedef struct {
	long mtype;
	char msg_trans[MSG_LENGTH];
} msgbuf;


/*shared memory*/
extern single_block *master_book;
extern id_proc *active_users;
extern id_proc *active_nodes;
extern count_attempt *trans_hops;
extern int *remaning_trans_in_tp;
extern int *status_simulazione;
extern int *book_row_write;
extern int *friend_to_add;

/*
 *mettiamo i metodi che vengono chiamati da file esterni.
 *i metodi "privati" non serve che siano qui dentro.
*/

/*----------METODI DI: utente.c----------*/

int num_active_users();

void sig_create_and_send_transaction();
/*
 *comando per terminale:
 *kill -SIGUSR2 <pid>
*/

void user(int f_user_index);


/*----------METODI DI: nodo.c----------*/

int num_active_nodes();

void sig_add_new_friend();

void node(int *f_node_friends, transaction first_trans, int f_node_index);
