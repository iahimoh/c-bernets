#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>

#define MAX_WORKERS 10
#define MAX_INSTRUCTIONS 10

#define LOGFILE "example.log"

#define SEM_MUTEX_NAME "/sem-mutex"
#define SEM_SPOOL_SIGNAL_NAME "/sem-spool-signal"
#define SHARED_MEM_NAME "/posix-shared-mem-example"

struct instrucao{
  int custo[3];
  char nome[256];
};

struct shared_memory{
  struct instrucao instrucoes[MAX_WORKERS][MAX_INSTRUCTIONS];
  int nproc, limites[MAX_WORKERS][3];
};

void error(char *msg);

int main(int argc, char **argv)
{
  struct shared_memory *shared_mem_ptr;
  sem_t *mutex_sem, *spool_signal_sem[MAX_WORKERS];
  int fd_shm, fd_log, procindex, instr_rd_indx;
  char mybuf [256];
  time_t t;
  srand((unsigned)time(&t));

  // Open log file
  if ((fd_log = open(LOGFILE, O_WRONLY | O_APPEND | O_SYNC, 0666)) == -1)
    error ("fopen");

  //  mutual exclusion semaphore, mutex_sem
  if ((mutex_sem = sem_open(SEM_MUTEX_NAME, 0, 0, 0)) == SEM_FAILED)
    error ("sem_open");

  // Get shared memory
  if ((fd_shm = shm_open(SHARED_MEM_NAME, O_RDWR, 0)) == -1)
    error ("shm_open");

  if ((shared_mem_ptr = mmap(NULL, sizeof(struct shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0)) == MAP_FAILED)
    error("mmap");

  // counting semaphore, indicating the number of strings to be printed.
  for(int i=0;i<MAX_WORKERS;i++){
    char name[100] = SEM_SPOOL_SIGNAL_NAME;
    strncat(name, (char *)&i,1);
    if ((spool_signal_sem[i] = sem_open(name, 0, 0, 0)) == SEM_FAILED)
      error("sem_open");
  }

  if (sem_wait(mutex_sem) == -1)
    error("sem_wait: mutex_sem");
  //critical section para escrever na memoria compartilhada.
  procindex = (shared_mem_ptr -> nproc);
  if (procindex == MAX_WORKERS){
    if (sem_post(mutex_sem) == -1)
      error("sem_post: mutex_sem");
    exit(0);
  }
  (shared_mem_ptr -> nproc)++;
  //end of critical section
  if (sem_post(mutex_sem) == -1)
    error("sem_post: mutex_sem");

  shared_mem_ptr -> limites[procindex][0] = rand() % 100;
  shared_mem_ptr -> limites[procindex][1] = rand() % 100;
  shared_mem_ptr -> limites[procindex][2] = rand() % 100;
  printf("meus limites: cpu-%d memoria-%d rede-%d\n",shared_mem_ptr -> limites[procindex][0], shared_mem_ptr -> limites[procindex][1],shared_mem_ptr -> limites[procindex][2]);

  while (1) {  // forever

    if (sem_wait(spool_signal_sem[procindex]) == -1)
      error("sem_wait: spool_signal_sem");

    sprintf(mybuf,"%d-%d-%s\n", getpid(), procindex, shared_mem_ptr -> instrucoes[procindex][instr_rd_indx].nome);
    printf("meus limites: cpu-%d memoria-%d rede-%d\n",shared_mem_ptr -> limites[procindex][0], shared_mem_ptr -> limites[procindex][1],shared_mem_ptr -> limites[procindex][2]);
    sleep(rand()%5);
    if (write(fd_log, mybuf, strlen(mybuf)) != strlen(mybuf))
      error("write: logfile");
    shared_mem_ptr -> limites[procindex][0] += shared_mem_ptr ->instrucoes[procindex][instr_rd_indx].custo[0];
    shared_mem_ptr -> limites[procindex][1] += shared_mem_ptr ->instrucoes[procindex][instr_rd_indx].custo[1];
    shared_mem_ptr -> limites[procindex][2] += shared_mem_ptr ->instrucoes[procindex][instr_rd_indx].custo[2];
    printf("meus limites: cpu-%d memoria-%d rede-%d\n",shared_mem_ptr -> limites[procindex][0], shared_mem_ptr -> limites[procindex][1],shared_mem_ptr -> limites[procindex][2]);

    instr_rd_indx++;
    if (instr_rd_indx == MAX_INSTRUCTIONS){
      instr_rd_indx=0;
    }
  }

  if (munmap(shared_mem_ptr, sizeof(struct shared_memory)) == -1)
    error ("munmap");
  exit (0);
}

// Print system error and exit
void error (char *msg)
{
  perror (msg);
  exit (1);
}

