# Laboratorio de Pthreads

**Curso:** Computación Paralela y Distribuida
**Universidad:** Universidad Nacional de San Agustín de Arequipa
**Alumno:** Chavez Medina, Fernando Jesus
**Docente:** Mamani Aliaga, Alvaro Henry

Tres programas en C con Pthreads. En cada uno se implementó primero la versión secuencial y después la paralela, y se comprobó que den los mismos resultados.

- `parte1.c`: suma, mínimo, máximo y promedio de un arreglo, dividiendo el arreglo entre threads y calculando resultados parciales en variables locales.
- `parte2.c`: histograma de 10 categorías con tres estrategias paralelas (sin sincronización, mutex compartido e histograma privado por thread).
- `parte3.c`: productores y consumidores con un buffer circular de tamaño fijo, usando un mutex y variables de condición.

## Cómo compilar y ejecutar

```bash
gcc -O2 -pthread parte1.c -o parte1 && ./parte1
gcc -O2 -pthread parte2.c -o parte2 && ./parte2
gcc -O2 -pthread parte3.c -o parte3 && ./parte3
```

Cada programa prueba automáticamente con 1, 2, 4 y 8 threads (partes 1 y 2) y muestra los tiempos, el speedup, la eficiencia y si el resultado es correcto.

## Requisitos

- Linux (o WSL en Windows)
- `gcc` con soporte para Pthreads