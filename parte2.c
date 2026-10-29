#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define BINS 10
#define REPS 3
#define N 20000000L
#define DIST 1  // 0 = uniforme; 1 = sesgada

static const int *datos;
static long hist_global[BINS];
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    long inicio, fin;
    long hist_local[BINS];
} ArgsHilo;

static double ahora(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

// Secuencial
static void secuencial(long *h) {
    memset(h, 0, BINS * sizeof(*h));

    for (long i = 0; i < N; i++)
        h[datos[i] / 10]++;
}

// Sin sincronizacion
// Tiene condicion de carrera intencionalmente.
static void *hilo_sin_sync(void *p) {
    ArgsHilo *h = p;

    for (long i = h->inicio; i < h->fin; i++)
        hist_global[datos[i] / 10]++;

    return NULL;
}

// Mutex compartido
static void *hilo_mutex(void *p) {
    ArgsHilo *h = p;

    for (long i = h->inicio; i < h->fin; i++) {
        int b = datos[i] / 10;

        pthread_mutex_lock(&mutex);
        hist_global[b]++;
        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

// Histograma privado por hilo 
static void *hilo_privado(void *p) {
    ArgsHilo *h = p;

    memset(h->hist_local, 0, sizeof(h->hist_local));

    for (long i = h->inicio; i < h->fin; i++)
        h->hist_local[datos[i] / 10]++;

    return NULL;
}

// Lanza T hilos y obtiene el histograma final.
static int paralelo(void *(*f)(void *), int T, char modo, long *resultado) {
    pthread_t *tid = malloc((size_t)T * sizeof(*tid));
    ArgsHilo *args = calloc((size_t)T, sizeof(*args));

    if (tid == NULL || args == NULL) {
        fprintf(stderr, "Error al reservar memoria para los hilos.\n");
        free(tid);
        free(args);
        return 0;
    }

    memset(hist_global, 0, sizeof(hist_global));

    long base = N / T;
    long resto = N % T;
    long ini = 0;

    for (int t = 0; t < T; t++) {
        long tam = base + (t < resto ? 1 : 0);

        args[t].inicio = ini;
        args[t].fin = ini + tam;
        ini += tam;

        int rc = pthread_create(&tid[t], NULL, f, &args[t]);

        if (rc != 0) {
            fprintf(stderr, "Error al crear el hilo %d.\n", t);

            for (int j = 0; j < t; j++)
                pthread_join(tid[j], NULL);

            free(tid);
            free(args);
            return 0;
        }
    }

    for (int t = 0; t < T; t++) {
        int rc = pthread_join(tid[t], NULL);

        if (rc != 0) {
            fprintf(stderr, "Error al esperar al hilo %d.\n", t);
            free(tid);
            free(args);
            return 0;
        }
    }

    if (modo == 'C') {
        memset(resultado, 0, BINS * sizeof(*resultado));

        for (int t = 0; t < T; t++)
            for (int b = 0; b < BINS; b++)
                resultado[b] += args[t].hist_local[b];
    } else {
        memcpy(resultado, hist_global, sizeof(hist_global));
    }

    free(tid);
    free(args);
    return 1;
}

static long suma_hist(const long *h) {
    long s = 0;

    for (int b = 0; b < BINS; b++)
        s += h[b];

    return s;
}

static int iguales(const long *a, const long *b) {
    return memcmp(a, b, BINS * sizeof(*a)) == 0;
}

int main(void) {
    const int hilos[] = {1, 2, 4, 8};

    int *a = malloc((size_t)N * sizeof(*a));

    if (a == NULL) {
        fprintf(stderr, "Error: no hay memoria suficiente.\n");
        return 1;
    }

    srand(0);

    for (long i = 0; i < N; i++) {
    if (DIST == 1) {
        if (rand() % 100 < 90) {
            a[i] = rand() % 10;
        } else {
            a[i] = rand() % 100;
        }
    } else {
        a[i] = rand() % 100;
    }
}

    datos = a;

    printf("N = %ld, distribucion = %s\n", (long)N, DIST ? "sesgada (90%% en 0-9)" : "uniforme");

    long ref[BINS], res[BINS];
    double ts = 0.0;

    // Calculo secuencial
    for (int k = 0; k < REPS; k++) {
        double t0 = ahora();
        secuencial(ref);
        ts += ahora() - t0;
    }

    ts /= REPS;

    printf("\nHistograma secuencial (tiempo %.6f s, " "suma = %ld, N = %ld):\n", ts, suma_hist(ref), (long)N);

    for (int b = 0; b < BINS; b++)
        printf("  %2d-%2d: %ld\n", b * 10, b * 10 + 9, ref[b]);

    printf("\n%-22s %-8s %-11s %-9s %-11s %-9s %-8s\n", "Estrategia", "Threads", "Tiempo(s)", "Speedup", "Eficiencia", "Suma==N", "Correcto");

    struct {
        const char *nombre;
        void *(*f)(void *);
        char modo;
    } est[] = {
        {"A) sin sincronizar", hilo_sin_sync, 'A'},
        {"B) mutex compartido", hilo_mutex, 'B'},
        {"C) histograma privado", hilo_privado, 'C'}
    };

    for (int e = 0; e < 3; e++) {
        for (int i = 0; i < 4; i++) {
            int T = hilos[i];
            double tp = 0.0;
            int fallo = 0;

            for (int k = 0; k < REPS; k++) {
                double t0 = ahora();

                if (!paralelo(est[e].f, T, est[e].modo, res)) {
                    fallo = 1;
                    break;
                }

                tp += ahora() - t0;
            }

            if (fallo) {
                free(a);
                pthread_mutex_destroy(&mutex);
                return 1;
            }

            tp /= REPS;

            double sp = ts / tp;

            printf("%-22s %-8d %-11.6f %-9.3f " "%-11.3f %-9s %-8s\n", est[e].nombre, T, tp, sp, sp / T,
                   suma_hist(res) == N ? "SI" : "NO", iguales(res, ref) ? "SI" : "NO");
        }
    }

    free(a);
    pthread_mutex_destroy(&mutex);

    return 0;
}