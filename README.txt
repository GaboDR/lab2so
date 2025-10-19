LABORATORIO 
=============================================
--------------------------------------------------------------------
IMPORTANTE SOBRE PAQUETES USADOS
--------------------------------------------------------------------
- Requerido:  
 * make (necesario para ejecutar el makefile).
 * unzip (necesario para descomprimir la carpeta).
 * Librerías POSIX usadas: sem_open, shm_open, mmap.
--------------------------------------------------------------------
AMBIENTE DE EJECUCION USADO
--------------------------------------------------------------------
- Sistema operativo: VM Linux Ubuntu 22.04.3 WSL
- Ejecución del código: Visual Studio Code

--------------------------------------------------------------------
EJECUCIÓN
--------------------------------------------------------------------
- La ejecución se hizo por consola usando los siguientes comandos: 
  $ make
  $ make run

- Se usó el compilador g++

--------------------------------------------------------------------
SUPUESTOS
--------------------------------------------------------------------
- Se asume que el usuario ingresa un número válido de días (entre 10
   y 30).

--------------------------------------------------------------------
ESPECIFICACIONES DE ALGORITMOS UTILIZADOS
--------------------------------------------------------------------
1) Creación y coordinación de procesos:

- Se utiliza el algoritmo de fork() para generar 4 procesos hijos,
  cada uno representando a un equipo distinto (agua, alimentos, cons
  trucción y señales).

-El proceso padre conserva los PID de los hijos y actúa como coordin
  ador general, controlando el flujo de días y verificando condicio
  nes de término.

2) Sincronización por semáforos POSIX:

- Se usan semáforos binarios (sem_turn[i]) para imponer un orden de
  ejecución entre los hijos, simulando turnos de trabajo.

- sem_step_done se emplea como barrera al final de cada ronda, garan
  tizando que todas las tareas se completen antes de avanzar.

- sem_done sincroniza la recolección final de resultados de los 4 equi
  pos antes de que el padre procese el día.

- sem_mutex asegura exclusión mutua en la escritura de la memoria com
  partida, evitando condiciones de carrera.

- sem_day_start actúa como barrier global para iniciar simultáneamente
  la lectura de la nueva configuración diaria.

3) Comunicación mediante memoria compartida (IPC):

- Se emplea shm_open y mmap para crear un bloque de memoria común 
  (shared_block_t), donde se almacenan las variables globales de simula
  ción: moral, distribución de tareas, estado de cada equipo y bandera 
  de término.

- Los hijos escriben sus resultados en la estructura compartida y el padre
  los lee tras finalizar cada ronda.
  
4) Control de moral y condiciones de fin:

- Se implementa un algoritmo de actualización de moral grupal basado en pena
  lizaciones acumulativas si los mínimos diarios no se cumplen.

- Se incluye una verificación doble de término:

-- Moral ≤ 0 → Fracaso del grupo

-- 10 días consecutivos de señales suficientes → Rescate exitoso

--------------------------------------------------------------------
NOMBRE Y ROL
--------------------------------------------------------------------
- Miguel Rivero par200
- 202373518-3
- Gabriel Delgado par200
- 202373610-4
