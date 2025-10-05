#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

void agua()
void alimentos()
void construccion()
void senales()

typedef struct reporte {
    pid_t id;
    int recoleccion;
    int estado;
} reporte_t;

typedef struct shared_block {
    int moral;
    int dia;
    bool start;
    reporte_t reportes[4];
} shared_block_t;

#define SHM_NAME "/lab2_so_shm_gabo"
#define SHM_SIZE ((off_t)sizeof(shared_block_t))

static shared_block_t *SHM = NULL;

int shm_setup(){
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd == -1) { perror("shm_open"); return -1; }

    if (ftruncate(fd, SHM_SIZE) == -1) { perror("ftruncate"); close(fd); return -1; }

    void *addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) { perror("mmap"); close(fd); return -1; }
    close(fd);

    SHM = (shared_block_t*)addr;
    memset(SHM, 0, sizeof(*SHM));
    SHM->moral = 100;
    SHM->dia = 0;
    return 0;
}

void shm_teardown(){
    if (SHM) {
        munmap(SHM, sizeof(*SHM));
        SHM = NULL;
    }
    // Solo una vez, al final del coordinador:
    shm_unlink(SHM_NAME);
}


int main(){
    pid_t pid;
    int i;
    pid_t recolctores[4];

    // primero se crean los hijos
    for (i = 0; i < 4; i++){
        pid = fork();
        if (pid == 0){
            recolctores[i] = getpid();
            break;
        } 
    }
    

    
    //despues ciclo de dias de supervivencia
    for (int j = 0; j < 10; j++){
        if (getpid() == recolctores[0]){
            agua()
        }

        else if (getpid() == recolctores[1]){
            alimentos()
        }
        else if (getpid() == recolctores[2]){
            construccion()
        }
        else if (getpid() == recolctores[3]){
            senales()
        }
        else {
            // coordinador
        }
    }

    return 0;
}

