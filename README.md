# Paralelización con Intel TBB

Infraestructuras Paralelas y Distribuidas
Escuela de Ingeniería de Sistemas y Computación, Universidad del Valle
Carlos Andrés Delgado Saavedra

El producto de Hadamard multiplica dos vectores posición a posición: cada
elemento del resultado depende solo de los dos elementos correspondientes de
las entradas. No hay dependencias entre iteraciones, así que es el primer caso
donde repartir el trabajo entre varios núcleos tiene sentido.

## Qué hay que hacer

Sobre `main.cpp`, usando TBB:

1. Llenar `u` y `v` con un valor constante pequeño, menor que diez.
2. Calcular en paralelo el producto elemento a elemento y guardarlo en `w`.
3. Sumar el vector resultante con una reducción paralela.
4. Imprimir el resultado y el tiempo de ejecución en milisegundos.

Los tres bloques de TBB que hacen falta son `parallel_for` para el llenado y el
producto, `blocked_range` para describir el rango que se divide, y
`parallel_reduce` para la suma.

## Cómo compilar y ejecutar

```bash
make
```

La regla compila con `g++ -o exe main.cpp -ltbb`, ejecuta el programa y borra
el ejecutable. Si falta la biblioteca, `script.sh` la instala en Debian o
Ubuntu:

```bash
bash script.sh
```

## Qué revisa el flujo de Actions

Solo que el programa compile y corra sin errores. La implementación se revisa
en clase: interesa que el reparto sea real y que los tiempos que se impriman
permitan comparar contra una versión secuencial.

## Para pensar antes de medir

El vector tiene diez millones de posiciones y cada elemento se toca una sola
vez. Con tan poco cálculo por dato, buena parte del tiempo se va trayendo
memoria. Vale la pena mirar cuánto mejora al pasar de dos a cuatro hilos, y
dónde deja de mejorar.
