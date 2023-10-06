#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
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
void signal_hdr(int);

int main()
{
  struct instrucao instruct;
  struct shared_memory *shared_mem_ptr;
  sem_t *mutex_sem, *spool_signal_sem[MAX_WORKERS];
  int fd_shm, fd_log, instr_wrt_indx[MAX_WORKERS];

  shm_unlink(SHARED_MEM_NAME);
  sem_unlink(SEM_MUTEX_NAME);
  for(int i=0;i<MAX_WORKERS;i++){
    char name[100] = SEM_SPOOL_SIGNAL_NAME;
    strncat(name, (char *)&i,1);
    sem_unlink(name);
  }

  // Creates log file
  if ((fd_log = open(LOGFILE, O_CREAT, 0666)) == -1)
    error("fopen");

  //  mutual exclusion semaphore, mutex_sem with an initial value 0.
  if ((mutex_sem = sem_open(SEM_MUTEX_NAME, O_CREAT, 0660, 0)) == SEM_FAILED)
    error("sem_open");

  // Get shared memory
  if ((fd_shm = shm_open(SHARED_MEM_NAME, O_RDWR | O_CREAT | O_EXCL, 0660)) == -1)
    error("shm_open");

  if (ftruncate (fd_shm, sizeof(struct shared_memory)) == -1)
    error("ftruncate");

  if ((shared_mem_ptr = mmap(NULL, sizeof(struct shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0)) == MAP_FAILED)
    error("mmap");

  // counting semaphore, indicating the number of strings to be printed. Initial value = 0
  for(int i=0;i<MAX_WORKERS;i++){
    char name[100] = SEM_SPOOL_SIGNAL_NAME;
    strncat(name, (char *)&i,1);
    if ((spool_signal_sem[i] = sem_open(name, O_CREAT | O_EXCL, 0660, 0)) == SEM_FAILED)
      error("sem_open");
  }
  for(int i=0;i<MAX_WORKERS;i++){
    instr_wrt_indx[i]=0;
  }//conserta um erro onde o index comeca com 1


  if (sem_post(mutex_sem) == -1) //avisa ao trabalhador que ele pode trabalhar
    error("sem_post: mutex_sem");

  signal(SIGINT, signal_hdr);

  while (1) {
    printf("insira um comando:\n");
    scanf("%255[^\r\n]", instruct.nome);

    printf("insira o custo de cpu do comando:\n");
    scanf("%d",&instruct.custo[0]);
    printf("insira o custo de memoria do comando:\n");
    scanf("%d",&instruct.custo[1]);
    printf("insira o custo de rede do comando:\n");
    scanf("%d",&instruct.custo[2]);
    getchar(); //conserta um erro onde o primeiro scanf nao scaneia
    for(int i=0;i<shared_mem_ptr -> nproc;i++)
    {
      if(shared_mem_ptr -> limites[i][0] >= instruct.custo[0] && shared_mem_ptr -> limites[i][1] >= instruct.custo[1] && shared_mem_ptr -> limites[i][2] >= instruct.custo[2])
      {
        shared_mem_ptr -> instrucoes[i][instr_wrt_indx[i]] = instruct;
        shared_mem_ptr -> limites[i][0] -= instruct.custo[0];
        shared_mem_ptr -> limites[i][1] -= instruct.custo[1];
        shared_mem_ptr -> limites[i][2] -= instruct.custo[2];
        instr_wrt_indx[i]++;
        if (instr_wrt_indx[i] == MAX_INSTRUCTIONS){
          instr_wrt_indx[i]=0;
        }
        if (sem_post(spool_signal_sem[i]) == -1)
          error("sem_post: (spool_signal_sem");
        break;
      }
    }
  }
}

void error (char *msg)
{
  perror (msg);
  exit (1);
}

void signal_hdr(int i)
{
  shm_unlink(SHARED_MEM_NAME);
  sem_unlink(SEM_MUTEX_NAME); 
  for(int i=0;i<MAX_WORKERS;i++){
    char name[100] = SEM_SPOOL_SIGNAL_NAME;
    strncat(name, (char *)&i,1);
    sem_unlink(name);
  }
  exit(1);
}

