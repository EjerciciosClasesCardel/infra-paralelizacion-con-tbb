# Estrategias de paralelización: hilos y TBB

Infraestructuras Paralelas y Distribuidas
Escuela de Ingeniería de Sistemas y Computación, Universidad del Valle
Carlos Andrés Delgado Saavedra

[![Pruebas](../../actions/workflows/pruebas.yml/badge.svg)](../../actions/workflows/pruebas.yml)

Cuatro programas, uno por cada decisión que va antes de repartir trabajo: qué
se reparte y quién decide el reparto; cómo se asignan tareas que no cuestan
lo mismo; hasta dónde se parte un problema que se divide en dos copias de sí
mismo; y qué hacer cuando cada paso depende del anterior. Todos imprimen el
resultado al lado del tiempo, y en todos el resultado se compara antes que el
reloj.

| Parte | Archivos | Qué se decide |
|---|---|---|
| 1 | `hadamard_hilos.cpp`, `hadamard_tbb.cpp` | Descomposición de datos a mano y delegada en TBB; el grano |
| 2 | `balanceo.cpp` | Reparto fijo o por demanda cuando las tareas cuestan distinto |
| 3 | `maximo.cpp` | Divide y vencerás con corte por profundidad |
| 4 | `prefijos.cpp` | Una dependencia que se reformula en dos pasadas |

Van de la más directa a la más exigente. Cada archivo trae el `main`, el
llenado de los datos y la salida ya escritos; lo que falta está marcado con
`TODO`.

## Parte 1: producto de Hadamard, dos versiones

El producto de Hadamard multiplica dos vectores posición a posición,
`w[i] = u[i] * v[i]`, y después se suman los elementos de `w`. Cada posición
depende solo de las dos entradas correspondientes, así que es el primer caso
donde repartir tiene sentido. `u` vale 4 y `v` vale 9 en todas las posiciones,
de modo que la suma tiene que dar `36 * n`: para `n = 10^8` son
3 600 000 000, que no caben en un `int`.

`hadamard_hilos.cpp` reparte a mano. Faltan tres funciones: `producto` y
`sumar`, que cada hilo ejecuta sobre su trozo `[ini, fin)`, y `hadamard`, que
parte `[0, n)` en `k` trozos, lanza los hilos, los une y combina los
parciales. El programa recibe `n` y recorre `k = 1, 2, 4, 8`.

`hadamard_tbb.cpp` delega el reparto. Faltan `llenar` y `producto` con
`parallel_for` sobre un `blocked_range`, y `sumar` con `parallel_reduce`. El
programa recibe `n`, el grano del rango y un tope de hilos:

```bash
./hadamard_tbb 100000000            # grano 1, todos los núcleos
./hadamard_tbb 100000000 1 2        # grano 1, dos hilos
./hadamard_tbb 100000000 100000     # grano de cien mil índices
```

```bash
make hadamard
```

La regla compila los dos programas y los corre con `n = 10^6` y `n = 10^8`;
en TBB, además, con uno, dos y cuatro hilos, y con granos de 1, 1 000,
100 000 y 10 000 000. Las dieciséis líneas quedan en `hadamard.txt`.

### Qué observar

Con un millón de elementos el trabajo entero toma alrededor de un milisegundo
y el reloj apenas lo distingue. Con cien millones, pasar de uno a ocho hilos
no divide el tiempo entre ocho: la operación hace muy poco cálculo por cada
dato que trae de memoria, y varios núcleos pidiendo a la vez saturan el bus
antes que los núcleos.

El grano fija el tamaño mínimo de un subrango. Con 10 000 000 quedan diez
trozos para repartir, y con cuatro hilos eso deja a dos de ellos con tres
trozos y a los otros dos con dos. Con grano 1 el particionador decide, y vale
la pena ver cuánto se separa de los otros.

## Parte 2: balanceo de carga

Sesenta y cuatro tareas y cuatro hilos. Las tareas cuestan distinto según dos
patrones: en el **creciente** la tarea `i` cuesta proporcional a `i` a la
cuarta, y en el **periódico** una de cada cuatro cuesta veinticinco veces lo
que las otras. Sobre cada patrón corren tres repartos:

- `bloques`, ya escrito: el hilo `h` hace las tareas `[16h, 16h + 16)`.
- `turnos`, por escribir: el hilo `h` hace las tareas `h, h + 4, h + 8, ...`
- `demanda`, por escribir: un contador compartido entrega la siguiente tarea
  libre y cada hilo vuelve por otra al terminar. Un `atomic<int>` con
  `fetch_add` reparte sin cerrojo.

```bash
make balanceo
```

Salen seis líneas con el tiempo y el total de cada combinación. Los seis
totales tienen que coincidir dentro de cada patrón: si uno difiere, algún
hilo hizo una tarea de más o dejó una sin hacer.

### Qué observar

En el patrón creciente el último bloque concentra el 77 % del trabajo, y el
hilo que lo recibe termina cuando los otros tres llevan rato esperando. En el
periódico las tareas caras caen todas en la posición 3 de cada cuatro, y el
reparto por turnos se las da todas al mismo hilo. Ningún reparto fijo queda
bien en los dos patrones sin conocer los costos de antemano; el reparto por
demanda no necesita conocerlos.

Esto es lo que hay detrás de `schedule(static)`, `schedule(static, 1)` y
`schedule(dynamic)` en OpenMP.

## Parte 3: el máximo por divide y vencerás

El máximo de cien millones de enteros. `maximo_sec` ya está: el máximo de un
rango es el mayor de los máximos de sus dos mitades, y los tramos de menos de
`MINIMO` elementos se resuelven con un ciclo. Falta `maximo_par`, que hace lo
mismo pero lanza un hilo para la mitad izquierda mientras el hilo actual
resuelve la derecha. El argumento `prof` es el presupuesto de niveles que
todavía pueden crear hilos; cuando llega a cero, o cuando el tramo baja de
`MINIMO`, el tramo sigue con `maximo_sec`.

```bash
make maximo
```

El programa imprime la versión secuencial y después las profundidades de
corte 0 a 4, con la aceleración de cada una. Los seis máximos tienen que ser
el mismo número.

### Qué observar

Combinar cuesta una comparación, así que aquí no está la mezcla que frenaba
al mergesort. Aun así la aceleración se detiene: cien millones de enteros son
400 MB, y leerlos es lo que cuesta. Ubique la profundidad a partir de la
cual agregar hilos ya no ayuda en su máquina.

## Parte 4: el máximo acumulado

Una serie de veinte millones de registros diarios. Para cada día se quiere el
saldo más alto visto hasta ese día, incluido él. La versión secuencial arrastra
el máximo de una posición a la siguiente, así que la cadena de dependencias
recorre la serie entera. Calcular el saldo de un registro cuesta, y ese costo
es el que hace que repartir se note.

Falta `en_dos_pasadas`, con `BLOQUES` hilos. En la primera pasada cada hilo
calcula el máximo de su bloque. Entre las dos, el punto de partida de cada
bloque es el máximo acumulado de los bloques anteriores. En la segunda cada
hilo rehace su bloque desde ese punto y escribe la salida.

```bash
make prefijos
```

El programa compara las dos salidas posición por posición e imprime
`coinciden` o `DIFIEREN`.

### Qué observar

La versión en dos pasadas calcula cada saldo dos veces y aun así termina
antes. Con dos núcleos pierde, porque hacer el doble de trabajo entre dos
deja el mismo tiempo. El esquema es el de la suma de prefijos con el máximo
en lugar de la suma; lo que cambia es qué reporta cada bloque y cómo se
combinan esos reportes.

## Cómo compilar y ejecutar

```bash
make hadamard     # parte 1, deja hadamard.txt
make balanceo     # parte 2, deja balanceo.txt
make maximo       # parte 3, deja maximo.txt
make prefijos     # parte 4, deja prefijos.txt
make todo         # las cuatro
```

Cada regla compila, ejecuta y borra el ejecutable. TBB es una biblioteca
externa y hay que instalarla; en Debian o Ubuntu lo hace `script.sh`:

```bash
bash script.sh
```

Sin el paquete, el error aparece en la primera línea de `hadamard_tbb.cpp`,
en el `#include`, no al enlazar.

## Qué revisa el flujo de Actions

- Parte 1: que las dieciséis sumas den `36 * n`, y que con `n = 10^8` cuatro
  hilos tarden menos que uno, tanto con `std::thread` como con TBB.
- Parte 2: que los seis totales sean los correctos; que el reparto por
  demanda no tarde más que los bloques en el patrón creciente ni más que los
  turnos en el periódico, y que los turnos no tarden más que los bloques en
  el creciente; y que el programa haya usado más de un procesador en
  promedio, medido como tiempo de CPU sobre tiempo de reloj.
- Parte 3: que las seis filas impriman el mismo máximo y que con cuatro hilos
  salga más rápido que en secuencial.
- Parte 4: que las dos series coincidan, que la versión en dos pasadas
  termine antes que la secuencial y que haya corrido en varios procesadores.

Cada parte es un job aparte: la lista de verificaciones del commit dice cuál
quedó en verde y cuál no, y la pestaña del run trae un resumen con la salida
de cada programa y el conteo de partes en verde. Cuando una verificación de
tiempos falla, el flujo repite la corrida una vez antes de marcar rojo, y el
error queda anotado sobre el archivo de esa parte. Un push nuevo cancela el
run anterior.

Los tiempos del registro son los de un servidor compartido con cuatro
procesadores; los que valen para la discusión son los de su máquina. Lo que se
mira en clase es lo que el flujo no puede mirar: si la tabla de tiempos tiene
sentido y si la explicación se sostiene.

## Lo que hay que poder explicar

- Por qué con cien millones de elementos pasar de uno a ocho hilos no divide
  el tiempo entre ocho, y qué cambia respecto a un millón.
- Qué grano dejó el mejor tiempo en TBB y por qué el de diez millones rinde
  menos.
- Cuál reparto perdió en cada patrón y cuánto, con la cuenta del trabajo que
  le tocó al hilo más cargado.
- La profundidad de corte que eligió para el máximo, y en qué se diferencia
  esa curva de la del mergesort.
- Qué reporta cada hilo en la primera pasada del máximo acumulado, qué recibe
  para la segunda, y en qué máquina esta versión perdería contra la
  secuencial.
