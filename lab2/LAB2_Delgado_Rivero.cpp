#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <semaphore.h>

using namespace std;
// Enumeramos los equipos
enum Team
{
    TEAM_AGUA = 0,
    TEAM_ALIMENTOS = 1,
    TEAM_CONSTRUCCION = 2,
    TEAM_SENALES = 3
};

static const int MIN_AGUA = 8;
static const int MIN_ALIM = 12;
static const int MIN_CONS = 4;
static const int MIN_SEN = 2;

static const int PEN_AGUA = 10;
static const int PEN_ALIM = 15;
static const int PEN_CONS = 8;
static const int PEN_SEN = 7;


typedef struct reporte
{
    pid_t id;
    int recoleccion;
    int estado;
} reporte_t;

typedef struct shared_block
{
    int moral;
    int dia;
    bool start;

    int turn;
    int substep;
    int printing;

    int termino;

    int distribucion;

    reporte_t reportes[4];
} shared_block_t;

static shared_block_t *SHM = NULL;

// Inicializamos los semáforos y variables globales
sem_t *sem_turn[4];
sem_t *sem_step_done;
sem_t *sem_done;
sem_t *sem_mutex;
sem_t *sem_day_start;


static const char *TAG[4] = {
    "EQUIPO AGUA", "EQUIPO ALIMENTOS", "EQUIPO CONSTRUCCION", "EQUIPO SENALES"};
static const char *MSG1[4] = {
    "Explorando fuentes de agua...",
    "Explorando territorio para cazar...",
    "Buscando materiales de construccion...",
    "Recolectando combustible seco..."};
static const char *MSG2[4] = {
    "Recolectando agua del arroyo encontrado...",
    "Intentando pescar en la laguna...",
    "Cortando ramas utiles...",
    "Manteniendo fogata de senales..."};
static const char *MSG3[4] = {
    "Purificando agua recolectada...",
    "Preparando alimentos recolectados...",
    "Construyendo/reforzando refugio...",
    "Creando senales de humo..."};

// 3 rondas: cada hijo imprime 1 linea por ronda, en orden Agua->Alimentos->Construccion->Senales
void imprimir_tres_rondas(int my_team)
{
    // RONDA 1
    sem_wait(sem_turn[my_team]);
    printf("[%s - PID: %d] %s\n", TAG[my_team], getpid(), MSG1[my_team]);
    fflush(stdout);
    if (my_team < 3)
        sem_post(sem_turn[my_team + 1]);
    else
        sem_post(sem_step_done);

    // RONDA 2
    sem_wait(sem_turn[my_team]);
    printf("[%s - PID: %d] %s\n", TAG[my_team], getpid(), MSG2[my_team]);
    fflush(stdout);
    if (my_team < 3)
        sem_post(sem_turn[my_team + 1]);
    else
        sem_post(sem_step_done);

    // RONDA 3
    sem_wait(sem_turn[my_team]);
    printf("[%s - PID: %d] %s\n", TAG[my_team], getpid(), MSG3[my_team]);
    fflush(stdout);
    if (my_team < 3)
        sem_post(sem_turn[my_team + 1]);
    else
        sem_post(sem_step_done);
}

static void recolectar(int team, int object)
{
    double recoleccion = 0.0;
    int estado = 0;
    int prob = rand() % 100;

    if (prob < 30)
    {
        recoleccion = object;
        estado = 2;
    }
    else if (prob >= 30 && prob < 80)
    {
        recoleccion = object * ((rand() % 31 + 50) / 100.0);
        estado = 1;
    }
    else
    {
        recoleccion = object * ((rand() % 30) / 100.0);
        estado = 0;
    }

    sem_wait(sem_mutex);
    SHM->reportes[team].id = getpid();
    SHM->reportes[team].recoleccion = (int)lround(recoleccion);
    SHM->reportes[team].estado = estado;
    sem_post(sem_mutex);

    sem_post(sem_done);
}

void agua()
{
    recolectar(TEAM_AGUA, 8);
}

void alimentos()
{
    recolectar(TEAM_ALIMENTOS, 12);
}

void construccion()
{
    recolectar(TEAM_CONSTRUCCION, 4);
}

void senales()
{
    recolectar(TEAM_SENALES, 2);
}


#define SHM_NAME "/lab2_so_shm_gabo"
#define SHM_SIZE ((off_t)sizeof(shared_block_t))


int shm_setup()
{
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd == -1)
    {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(fd, SHM_SIZE) == -1)
    {
        perror("ftruncate");
        close(fd);
        return -1;
    }

    void *addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return -1;
    }
    close(fd);

    SHM = (shared_block_t *)addr;

    memset(SHM, 0, sizeof(*SHM));
    SHM->moral = 100;
    SHM->dia = 0;
    SHM->start = false;

    SHM->turn = 0;
    SHM->substep = 0;
    SHM->printing = 0;

    SHM->termino = 0;

    SHM->distribucion = 0;

    for (int i = 0; i < 4; ++i)
    {
        SHM->reportes[i].id = 0;
        SHM->reportes[i].recoleccion = 0;
        SHM->reportes[i].estado = -1; // -1 = aún no reporta
    }
    return 0;
}

void shm_teardown()
{
    if (SHM)
    {
        munmap(SHM, sizeof(*SHM));
        SHM = NULL;
    }

    shm_unlink(SHM_NAME);
}

int main()
{
    int dias;
    printf("Ingrese la cantidad de dias a simular (max 30, min 10): ");
    scanf("%d", &dias);

    srand(time(NULL));

    if (shm_setup() == -1)
        return 1;

    // Limpieza preventiva por si quedaron semáforos de una corrida anterior
    sem_unlink("/lab2_turn0");
    sem_unlink("/lab2_turn1");
    sem_unlink("/lab2_turn2");
    sem_unlink("/lab2_turn3");
    sem_unlink("/lab2_step");
    sem_unlink("/lab2_done");
    sem_unlink("/lab2_mutex");
    sem_unlink("/lab2_day_start");

    sem_turn[0] = sem_open("/lab2_turn0", O_CREAT, 0600, 0);
    sem_turn[1] = sem_open("/lab2_turn1", O_CREAT, 0600, 0);
    sem_turn[2] = sem_open("/lab2_turn2", O_CREAT, 0600, 0);
    sem_turn[3] = sem_open("/lab2_turn3", O_CREAT, 0600, 0);
    sem_step_done = sem_open("/lab2_step", O_CREAT, 0600, 0);
    sem_done = sem_open("/lab2_done", O_CREAT, 0600, 0);
    sem_mutex = sem_open("/lab2_mutex", O_CREAT, 0600, 1);
    sem_day_start = sem_open("/lab2_day_start", O_CREAT, 0600, 0);


    if (!sem_turn[0] || !sem_turn[1] || !sem_turn[2] || !sem_turn[3] || !sem_step_done || !sem_done || !sem_mutex)
    {
        perror("sem_open");
        return 1;
    }

    pid_t hijos[4] = {0, 0, 0, 0};
    int my_team = -1;

    // Crear procesos hijos para cada equipo
    for (int i = 0; i < 4; i++)
    {
        pid_t pid = fork();

        if (pid == 0)
        {
            my_team = i;
            break;
        }
        else
        {
            hijos[i] = pid;
        }
    }

    // Lógica de los procesos hijos
    if (my_team == -1)
    {
        // Proceso padre
        for (int dia = 1; dia <= dias; dia++)
        {
            printf("=== DIA %d DE SUPERVIVENCIA ===\n", dia);
            printf("Iniciando recoleccion de recursos...\n");

            sleep(2);

            // Resetear reportes
            for (int i = 0; i < 4; ++i)
            {
                SHM->reportes[i].id = 0;
                SHM->reportes[i].recoleccion = 0;
                SHM->reportes[i].estado = -1; // -1 = aún no reporta
            }

            SHM->distribucion = (rand() % 3) + 1;

            // liberar a los 4 hijos para que lean la nueva distribucion
            for (int i = 0; i < 4; ++i) sem_post(sem_day_start);

            // Esto es solo para printear todo de forma ordenada (con las 3 rondas de la tarea)
            sem_post(sem_turn[0]);
            sem_wait(sem_step_done);

            sem_post(sem_turn[0]);
            sem_wait(sem_step_done);

            sem_post(sem_turn[0]);
            sem_wait(sem_step_done);

            for (int k = 0; k < 4; k++)
            {
                sem_wait(sem_done);
            }

            // Totales del día
            int total_agua_dia = SHM->reportes[TEAM_AGUA].recoleccion;
            int total_alimentos_dia = SHM->reportes[TEAM_ALIMENTOS].recoleccion;
            int total_construccion_dia = SHM->reportes[TEAM_CONSTRUCCION].recoleccion;
            int total_senales_dia = SHM->reportes[TEAM_SENALES].recoleccion;

            // Cumplimiento de mínimos
            bool ok_agua = (total_agua_dia >= MIN_AGUA);
            bool ok_alim = (total_alimentos_dia >= MIN_ALIM);
            bool ok_cons = (total_construccion_dia >= MIN_CONS);
            bool ok_sen = (total_senales_dia >= MIN_SEN);

            printf("\nREPORTES FINALES (DIA %d):\n", dia);
            printf("- Equipo Agua completó ciclo: %d unidades obtenidas\n",
                   total_agua_dia);
            printf("- Equipo Alimentos completó ciclo: %d unidades obtenidas\n",
                   total_alimentos_dia);
            printf("- Equipo Construccion completó ciclo: %d unidades obtenidas\n",
                   total_construccion_dia);
            printf("- Equipo Senales completó ciclo: %d unidades obtenidas\n",
                   total_senales_dia);

            // Calculo de penalizaciones
            int penalizaciones = 0;
            if (!ok_agua)
                penalizaciones += PEN_AGUA;
            if (!ok_alim)
                penalizaciones += PEN_ALIM;
            if (!ok_cons)
                penalizaciones += PEN_CONS;
            if (!ok_sen)
                penalizaciones += PEN_SEN;

            SHM->moral -= penalizaciones;
            if (SHM->moral < 0)
                SHM->moral = 0;

            // Racha de señales para victoria
            static int racha_senales = 0;
            racha_senales = ok_sen ? (racha_senales + 1) : 0;

            // Mostrar resumen del día
            printf("RESULTADOS DEL DIA %d:\n", dia);
            printf("- Agua         : %d/%d %s\n", total_agua_dia, MIN_AGUA, ok_agua ? "(SUFICIENTE)" : "(INSUFICIENTE)");
            printf("- Alimentos    : %d/%d %s\n", total_alimentos_dia, MIN_ALIM, ok_alim ? "(SUFICIENTE)" : "(INSUFICIENTE)");
            printf("- Construccion : %d/%d %s\n", total_construccion_dia, MIN_CONS, ok_cons ? "(SUFICIENTE)" : "(INSUFICIENTE)");
            printf("- Senales      : %d/%d %s\n", total_senales_dia, MIN_SEN, ok_sen ? "(SUFICIENTE)" : "(INSUFICIENTE)");
            if (penalizaciones > 0)
                printf("Moral del grupo: %d/100 (-%d)\n\n", SHM->moral, penalizaciones);
            else
                printf("Moral del grupo: %d/100 (-0)\n\n", SHM->moral);

            // Verificar condiciones de fin del juego
            if (SHM->moral == 0)
            {
                printf("La moral del grupo ha llegado a 0. El grupo no puede continuar. FIN DEL JUEGO.\n");
                SHM->termino = 1;
                for (int i = 0; i < 4; ++i) sem_post(sem_day_start);  // liberar a todos los hijos
                break;
            }

            if (racha_senales >= 10)
            {
                printf("El grupo ha mantenido señales efectivas por 10 dias consecutivos. RESCATE LOGRADO. FIN DEL JUEGO.\n");
                SHM->termino = 1;
                for (int i = 0; i < 4; ++i) sem_post(sem_day_start);
                break;
            }
        }
        for (int i = 0; i < 4; ++i)
            if (hijos[i] > 0)
                waitpid(hijos[i], NULL, 0);

        for (int i = 0; i < 4; i++)
            sem_close(sem_turn[i]);
            sem_close(sem_step_done);
            sem_close(sem_done);
            sem_close(sem_mutex);
            sem_close(sem_day_start);

            sem_unlink("/lab2_turn0");
            sem_unlink("/lab2_turn1");
            sem_unlink("/lab2_turn2");
            sem_unlink("/lab2_turn3");
            sem_unlink("/lab2_step");
            sem_unlink("/lab2_done");
            sem_unlink("/lab2_mutex");
            sem_unlink("/lab2_day_start");

        shm_teardown();
        return 0;
    }
    else
    {
        // Proceso hijo, setear semilla de random para cada equipo
        srand((unsigned)time(NULL) ^ (unsigned)getpid());

        for (int dia = 1; dia <= dias; dia++)
        {
            sem_wait(sem_day_start);            // ← asegura que ya está la nueva distribucion
            
            if (SHM->termino) {        // ← salir limpio si el padre anunció término
            break;
            }

            int tarea = (my_team + SHM->distribucion) % 4;
            imprimir_tres_rondas(tarea );

            switch (tarea)
            {
            case TEAM_AGUA:
                agua();
                break;
            case TEAM_ALIMENTOS:
                alimentos();
                break;
            case TEAM_CONSTRUCCION:
                construccion();
                break;
            case TEAM_SENALES:
                senales();
                break;
            }
        }
        return 0;
    }
}
