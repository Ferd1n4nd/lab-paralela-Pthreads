#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <time.h>

#define REPS 5

typedef struct {
    long long suma;
    int min, max;
    double promedio;
} Resultado;

typedef struct {
    const int *a;
    long inicio, fin;
    long long suma;
    int min, max;
} ArgsThread;

double ahora(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

// Secuencial
Resultado secuencial(const int *a, long n) {
    Resultado r = { 0, INT_MAX, INT_MIN, 0.0 };
    for (long i = 0; i < n; i++) {
        r.suma += a[i];
        if (a[i] < r.min)
            r.min = a[i];
        if (a[i] > r.max)
            r.max = a[i];
    }
    r.promedio = (double)r.suma / n;
    return r;
}

// Funciones de cada thread
void *trabajo(void *p) {
    ArgsThread *h = (ArgsThread *)p;

    // Variables locales
    long long suma = 0;
    int min = INT_MAX, max = INT_MIN;
    for (long i = h->inicio; i < h->fin; i++) {
        suma += h->a[i];
        if (h->a[i] < min) min = h->a[i];
        if (h->a[i] > max) max = h->a[i];
    }
    h->suma = suma;
    h->min = min;
    h->max = max;
    return NULL;
}

// Paralela
Resultado paralela(const int *a, long n, int T) {
    pthread_t *tid = malloc(T * sizeof(pthread_t));
    ArgsThread  *args = calloc(T, sizeof(ArgsThread));

    long base = n / T, resto = n % T, inicio = 0;
    for (int t = 0; t < T; t++) {
        long tam = base + (t < resto ? 1 : 0);
        args[t].a = a;
        args[t].inicio = inicio;
        args[t].fin = inicio + tam;
        inicio += tam;
        pthread_create(&tid[t], NULL, trabajo, &args[t]);
    }

    Resultado r = { 0, INT_MAX, INT_MIN, 0.0 };
    for (int t = 0; t < T; t++) {
        pthread_join(tid[t], NULL);
        r.suma += args[t].suma;
        if (args[t].min < r.min) r.min = args[t].min;
        if (args[t].max > r.max) r.max = args[t].max;
    }
    r.promedio = (double)r.suma / n;
    free(tid);
    free(args);
    return r;
}

int main() {
    long N = 100000L;
    int hilos[] = {1, 2, 4, 8};

    int *a = malloc(N * sizeof(int));
    srand(0);
    for (long i = 0; i < N; i++) 
        a[i] = rand() % 100001 - 50000;

    printf("N = %ld, repeticiones = %d\n", N, REPS);

    // Tiempo secuencial
    Resultado rs = {0};
    double ts = 0;
    for (int k = 0; k < REPS; k++) {
        double t0 = ahora();
        rs = secuencial(a, N);
        ts += ahora() - t0;
    }
    ts /= REPS;
    printf("Secuencial: suma=%lld min=%d max=%d prom=%.4f  tiempo=%.6f s\n\n", rs.suma, rs.min, rs.max, rs.promedio, ts);
    printf("%-8s %-12s %-10s %-12s %-10s\n", "Threads", "Tiempo(s)", "Speedup", "Eficiencia", "Resultado igual a secuencial");

    // Tiempo paralela
    for (int i = 0; i < 4; i++) {
        int T = hilos[i];
        Resultado rp = {0};
        double tp = 0;
        for (int k = 0; k < REPS; k++) {
            double t0 = ahora();
            rp = paralela(a, N, T);
            tp += ahora() - t0;
        }
        tp /= REPS;

        // Verificacion de resultados
        int ok = (rp.suma == rs.suma && rp.min == rs.min && rp.max == rs.max);
        double speedup = ts / tp;
        double efic = speedup / T;
        printf("%-8d %-12.6f %-10.3f %-12.3f %-10s\n", T, tp, speedup, efic, ok ? "SI" : "NO");
    }
    free(a);
    return 0;
}