#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#define SEM_NAME "/example33_sem"

volatile sig_atomic_t sigint_received = 0;
volatile sig_atomic_t sigterm_received = 0;
volatile sig_atomic_t sigusr1_received = 0;
volatile sig_atomic_t sigusr2_received = 0;
volatile sig_atomic_t voting_start = 0;

sem_t *sem_candidato;

void sigint_handler(int signum) {
    if (signum == SIGINT) {
        sigint_received = 1;
        exit(EXIT_SUCCESS);
    }
}

void sigterm_handler(int signum) {
    if (signum == SIGTERM) {
        sigterm_received = 1;
    }
}

void sigusr2_handler(int signum) {
    if (signum == SIGUSR2) {
        sigusr2_received = 1;
    }
}

void sigusr1_handler(int signum) {
    if (signum == SIGUSR1) {
        sigusr1_received = 1;
    }
}

char decideYN() {
    srand(time(NULL) + getpid()); // Inicializar la semilla

    int num = rand() % 2; // Modificar para que esté dentro del rango [0, 1]

    if (num == 0) {
        return 'Y';
    } else {
        return 'N';
    }
}

void candidato(int n_procs, char *pidsname, char *votos, sigset_t oset) {
    struct timespec tiempo;
    int i=0, numero;
    int yes=0, no=0;
    pid_t *pids2;
    char c;
    FILE *v, *f;
    pids2 = (pid_t*)malloc(sizeof(pid_t)*n_procs+1);
    tiempo.tv_sec = 0;
    tiempo.tv_nsec = 10000000;

    sigsuspend(&oset);

    f = fopen(pidsname, "r");
    rewind(f);
    while (fscanf(f, "%d\n", &numero) != EOF) {
        pids2[i] = numero;
        i++;
    }
    for(i=0; i<n_procs; i++) {
        if(pids2[i] != getpid()){
            kill(pids2[i], SIGUSR2);
        }
    }
    
    i=0;
    free(pids2);
    while (i != n_procs-1)
    {
        i=0;
        v = fopen(votos, "r");
        rewind(v);
        while(fscanf(v, "%c ",&c) == 1){
            i++;
        }
        fclose(v);
        nanosleep(&tiempo, NULL);
    }
    fclose(f);
    v = fopen(votos, "r");
    printf("Candidate %d => [ ", getpid());
    for (i = 0; i < n_procs-1; i++)
    {
        fscanf(v, "%c ", &c);
        printf("%c ", c);
        if(c == 'N')
            no++;
        else 
            yes++;
    }
    fclose(v);
    printf("] => ");
    if(no<=yes)
        printf("Accepted\n");
    else
        printf("Rejected\n");
}

void votante(int n_procs, char *votos, sigset_t oset) {
    char voto;
    FILE *f;
    printf("1");
    sigsuspend(&oset);
    printf("2");
    
    f=fopen(votos, "a");
    voto = decideYN();
    fprintf(f, "%c ", voto);
    fclose(f);


}

void principal(int n_procs, int n_secs, sigset_t oset, sigset_t set) {
    int i, j=0, value;
    pid_t* pids;
    struct timespec tiempo;
    FILE *f, *v;
    char votos[126] = {"votos.txt"}, pidsname[126]={"pids.txt"};
    tiempo.tv_sec = 0;
    tiempo.tv_nsec = 100;
    struct sigaction actterm, actint, actsr1, actsr2;

    actint.sa_handler = sigint_handler;
    sigemptyset(&(actint.sa_mask));
    actint.sa_flags = 0;
    if (sigaction(SIGINT, &actint, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    actterm.sa_handler = sigterm_handler;
    sigemptyset(&(actterm.sa_mask));
    actterm.sa_flags = 0;
    if (sigaction(SIGTERM, &actterm, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    actsr1.sa_handler = sigusr1_handler;
    sigemptyset(&(actsr1.sa_mask));
    actsr1.sa_flags = 0;
    if (sigaction(SIGUSR1, &actsr1, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    actsr2.sa_handler = sigusr2_handler;
    sigemptyset(&(actsr2.sa_mask));
    actsr2.sa_flags = 0;
    if (sigaction(SIGUSR2, &actsr2, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    for(j=0; j<n_procs; j++) {

        pids = (pid_t*)malloc(sizeof(pid_t)*n_procs+1);

        if (!pids) {
            perror("Out of memory");
            exit(EXIT_FAILURE);
        }

        if ((sem_candidato = sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
        }
        sem_unlink(SEM_NAME);
        f = fopen(pidsname, "w+");
        v = fopen(votos, "w+");

        if (f == NULL) {
            perror("Error al abrir el archivo");
            exit(EXIT_FAILURE); 
        }
        for (i = 0; i < n_procs; i++) {
            pid_t pid = fork();
            if (pid < 0) {
                perror("Error en la creación del proceso hijo");
                return;
            }
            //printf("Proceso %d\n", i);
            if (pid == 0) {
                free(pids);
                fclose(v);
                fclose(f);
                for(j=0; j < n_procs; j++) {
                    sem_getvalue(sem_candidato, &value);
                    printf("%d value %d\n",j, value);
                    if (sem_trywait(sem_candidato) == 0) {
                        printf("0\n");
                        candidato(n_procs, pidsname, votos, oset);
                    } else {
                        printf("9");
                        votante(n_procs, votos, oset);
                    }
                    sem_post(sem_candidato);
                }
                sem_close(sem_candidato);
                exit(EXIT_SUCCESS);
            } else {
                pids[i] = pid;
            }
            nanosleep(&tiempo, NULL);
        }
        for ( i = 0; i < n_procs; i++)
        {
            fprintf(f,"%d\n", pids[i]);
        }

        fclose(f);
        fclose(v);

        for(i = 0; i < n_procs; i++) {
            kill(pids[i], SIGUSR1);
        }

        for(i = 0; i < n_procs; i++) {
            kill(pids[i], SIGTERM);
        }

        for(i = 0; i < n_procs; i++) {
            waitpid(pids[i], NULL, 0);
        }

        sem_close(sem_candidato);
        free(pids);
    }

}

int main(int argc, char *argv[]) {
    sigset_t set, oset;
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <N_PROCS> <N_SECS>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int n_procs = atoi(argv[1]);
    int n_secs = atoi(argv[2]);

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);

    if (sigprocmask(SIG_BLOCK, &set, &oset) < 0) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    principal(n_procs, n_secs, set, oset);
    

    return 0;
}
