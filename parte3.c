#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define PRODUCTORES 2
#define CONSUMIDORES 2
#define TAM_BUFFER 8
#define ITEMS_POR_PRODUCTOR 10000

typedef struct {
    int *datos;
    int capacidad;
    int in;
    int out;
    int cont;
    int productores_activos;
    int *vistos;
    pthread_mutex_t mutex;
    pthread_cond_t no_lleno;
    pthread_cond_t no_vacio;
} Buffer;

static Buffer buf;

typedef struct {
    int id;
    int items;
    long long suma;
    long cantidad;
} ArgsHilo;

/* ---------- Productor ---------- */
static void *productor(void *p) {
    ArgsHilo *h = p;

    for (int i = 0; i < h->items; i++) {
        int item = h->id * h->items + i;

        pthread_mutex_lock(&buf.mutex);

        while (buf.cont == buf.capacidad)
            pthread_cond_wait(&buf.no_lleno, &buf.mutex);

        buf.datos[buf.in] = item;
        buf.in = (buf.in + 1) % buf.capacidad;
        buf.cont++;

        pthread_cond_signal(&buf.no_vacio);
        pthread_mutex_unlock(&buf.mutex);

        h->suma += item;
        h->cantidad++;
    }

    /* Avisar que este productor ha terminado */
    pthread_mutex_lock(&buf.mutex);

    buf.productores_activos--;

    if (buf.productores_activos == 0)
        pthread_cond_broadcast(&buf.no_vacio);

    pthread_mutex_unlock(&buf.mutex);

    return NULL;
}

/* ---------- Consumidor ---------- */
static void *consumidor(void *p) {
    ArgsHilo *h = p;

    for (;;) {
        pthread_mutex_lock(&buf.mutex);

        while (buf.cont == 0 && buf.productores_activos > 0)
            pthread_cond_wait(&buf.no_vacio, &buf.mutex);

        /* No hay elementos y ningun productor producira mas */
        if (buf.cont == 0) {
            pthread_mutex_unlock(&buf.mutex);
            break;
        }

        int item = buf.datos[buf.out];

        buf.out = (buf.out + 1) % buf.capacidad;
        buf.cont--;

        /* Registrar cuantas veces se consumio este elemento */
        buf.vistos[item]++;

        pthread_cond_signal(&buf.no_lleno);
        pthread_mutex_unlock(&buf.mutex);

        h->suma += item;
        h->cantidad++;
    }

    return NULL;
}

int main(void) {
    const int P = PRODUCTORES;
    const int C = CONSUMIDORES;
    const int B = TAM_BUFFER;
    const int K = ITEMS_POR_PRODUCTOR;

    long total = (long)P * K;

    /* Reservar memoria */
    buf.datos = malloc((size_t)B * sizeof(*buf.datos));
    buf.vistos = calloc((size_t)total, sizeof(*buf.vistos));

    if (buf.datos == NULL || buf.vistos == NULL) {
        fprintf(stderr, "Error al reservar memoria.\n");
        free(buf.datos);
        free(buf.vistos);
        return 1;
    }

    /* Inicializar el buffer */
    buf.capacidad = B;
    buf.in = 0;
    buf.out = 0;
    buf.cont = 0;
    buf.productores_activos = P;

    pthread_mutex_init(&buf.mutex, NULL);
    pthread_cond_init(&buf.no_lleno, NULL);
    pthread_cond_init(&buf.no_vacio, NULL);

    /* Crear arreglos para los hilos */
    pthread_t *tp = malloc((size_t)P * sizeof(*tp));
    pthread_t *tc = malloc((size_t)C * sizeof(*tc));

    ArgsHilo *ap = calloc((size_t)P, sizeof(*ap));
    ArgsHilo *ac = calloc((size_t)C, sizeof(*ac));

    if (tp == NULL || tc == NULL || ap == NULL || ac == NULL) {
        fprintf(stderr, "Error al reservar memoria para los hilos.\n");

        free(tp);
        free(tc);
        free(ap);
        free(ac);
        free(buf.datos);
        free(buf.vistos);

        pthread_mutex_destroy(&buf.mutex);
        pthread_cond_destroy(&buf.no_lleno);
        pthread_cond_destroy(&buf.no_vacio);

        return 1;
    }

    /* Crear primero los consumidores */
    for (int i = 0; i < C; i++) {
        ac[i].id = i;

        if (pthread_create(&tc[i], NULL, consumidor, &ac[i]) != 0) {
            fprintf(stderr, "Error al crear consumidor.\n");
            exit(EXIT_FAILURE);
        }
    }

    /* Crear los productores */
    for (int i = 0; i < P; i++) {
        ap[i].id = i;
        ap[i].items = K;

        if (pthread_create(&tp[i], NULL, productor, &ap[i]) != 0) {
            fprintf(stderr, "Error al crear productor.\n");
            exit(EXIT_FAILURE);
        }
    }

    /* Esperar a que terminen los productores */
    for (int i = 0; i < P; i++)
        pthread_join(tp[i], NULL);

    /* Esperar a que terminen los consumidores */
    for (int i = 0; i < C; i++)
        pthread_join(tc[i], NULL);

    /* ---------- Verificacion ---------- */
    long long sp = 0;
    long long sc = 0;
    long np = 0;
    long nc = 0;

    for (int i = 0; i < P; i++) {
        sp += ap[i].suma;
        np += ap[i].cantidad;
    }

    for (int i = 0; i < C; i++) {
        sc += ac[i].suma;
        nc += ac[i].cantidad;

        printf("Consumidor %d consumio %ld items\n", i, ac[i].cantidad);
    }

    long perdidos = 0;
    long duplicados = 0;

    for (long x = 0; x < total; x++) {
        if (buf.vistos[x] == 0)
            perdidos++;
        else if (buf.vistos[x] > 1)
            duplicados++;
    }

    printf("\nP=%d C=%d buffer=%d items/productor=%d\n", P, C, B, K);

    printf("Producidos = %ld, Consumidos = %ld, Esperados = %ld\n", np, nc, total);

    printf("Suma producida = %lld, Suma consumida = %lld\n", sp, sc);

    printf("Perdidos = %ld, Duplicados = %ld, "
           "Quedaron en buffer = %d\n", perdidos, duplicados, buf.cont);

    printf("RESULTADO: %s\n", (np == total && nc == total && sp == sc && !perdidos && !duplicados && buf.cont == 0) ? "CORRECTO" : "ERROR");

    free(tp);
    free(tc);
    free(ap);
    free(ac);
    free(buf.datos);
    free(buf.vistos);

    pthread_mutex_destroy(&buf.mutex);
    pthread_cond_destroy(&buf.no_lleno);
    pthread_cond_destroy(&buf.no_vacio);

    return 0;
}