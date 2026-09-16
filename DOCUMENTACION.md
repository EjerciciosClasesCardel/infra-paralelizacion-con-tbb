# Documentación de apoyo: estrategias de paralelización con hilos y TBB

Aquí está lo que hace falta saber para completar cada `TODO` del repositorio:
las funciones y clases de `std::thread`, `std::atomic` y oneTBB que cada parte
usa, con su firma y para qué sirven. Hay una sección por parte del README, en
el mismo orden y con el mismo nombre. Cada ejemplo es un programa completo que
resuelve un problema vecino al de la parte, con el comando para compilarlo y
la salida que dio en una máquina de seis núcleos y doce hilos; se copia, se
corre y se adapta. Al final de cada sección van los enlaces a la
documentación oficial, y al cierre, cómo dejar la máquina propia lista para
compilar.

## Parte 1: producto de Hadamard, dos versiones

### Lo que se usa

**Con `std::thread`** (`#include <thread>`, `<functional>` y `<vector>`):

- `std::thread(f, args...)`: crea un hilo que arranca de inmediato ejecutando
  `f(args...)`. Los argumentos se copian dentro del hilo, y una función que
  recibe `vector<int> &` o `long &` no puede trabajar sobre una copia; para
  eso están los dos envoltorios siguientes.
- `std::ref(x)`: envuelve `x` para que el hilo reciba una referencia y no una
  copia. Va con lo que el hilo escribe: el vector de salida o la
  variable donde deja su parcial.
- `std::cref(x)`: lo mismo, pero como referencia constante. Va con lo que
  el hilo solo lee, y evita copiar un vector de cien millones de enteros
  por cada hilo.
- `std::vector<std::thread> hilos; hilos.emplace_back(f, cref(u), ref(w), ini, fin);`:
  construye el hilo directamente dentro del vector, con los mismos argumentos
  que llevaría el constructor. `std::thread` no se puede copiar, así que
  `push_back(thread(...))` obliga a un movimiento y `emplace_back` lo evita.
- `t.join()`: bloquea hasta que el hilo termina. Un `std::thread` que se
  destruye sin `join` aborta el programa. Después del `join` el vector se
  puede vaciar con `hilos.clear()` para lanzar una segunda tanda.
- El reparto de `[0, n)` en `k` trozos: `paso = n / k`, el trozo `h` es
  `[h * paso, (h + 1) * paso)` y el último llega hasta `n` para recoger el
  resto cuando `n` no es múltiplo de `k`.

**Con oneTBB** (`#include <tbb/blocked_range.h>`, `<tbb/parallel_for.h>`,
`<tbb/parallel_reduce.h>`, `<tbb/global_control.h>`, `<tbb/task_arena.h>`):

- `tbb::blocked_range<size_t>(begin, end, grainsize)`: describe el rango de
  índices `[begin, end)` y el tamaño por debajo del cual un subrango ya no se
  parte más. Con `grainsize = 1` (el valor por omisión) el particionador
  automático decide cuánto partir según cuántos hilos estén libres. Un
  subrango `r` expone `r.begin()` y `r.end()`.
- `tbb::parallel_for(rango, cuerpo)`: parte el rango, reparte los subrangos
  entre los hilos del planificador y llama `cuerpo(r)` con cada uno. El
  cuerpo es un lambda `[&](const tbb::blocked_range<size_t> &r) { ... }` que
  recorre `r` con un ciclo de `r.begin()` a `r.end()`. Quien escribe el
  cuerpo no decide cuántos hilos hay ni qué trozo le toca a cada uno.
- `tbb::parallel_reduce(rango, identidad, cuerpo, combinacion)`, en su forma
  funcional: `identidad` es el valor con el que arranca cada parcial;
  `cuerpo` es `[&](const tbb::blocked_range<size_t> &r, T parcial) -> T`,
  que recibe el acumulado hasta ahora, le agrega lo del subrango y lo
  devuelve; `combinacion` es `[](T a, T b) -> T`, que junta dos parciales.
  El tipo del resultado es el tipo de `identidad`: `0L` da `long`, `0` da
  `int`.
- `tbb::global_control tope(tbb::global_control::max_allowed_parallelism, k)`:
  mientras el objeto exista, el planificador no usa más de `k` hilos. El
  tope se levanta cuando el objeto se destruye, así que tiene que vivir en un
  alcance que abarque las llamadas que se quieren acotar.
- `tbb::this_task_arena::max_concurrency()`: cuántos hilos puede usar el
  planificador en este momento; sin tope, el número de procesadores lógicos.

### Ejemplo con std::thread

Con qué dígito terminan los cuadrados de `0` a `n - 1`, contados con `k`
hilos. Cada hilo recibe el vector por `cref`, su trozo `[ini, fin)` y por
`ref` el vector de diez casillas donde deja su conteo; el hilo principal suma
los `k` conteos casilla por casilla.

```cpp
// digitos_hilos.cpp
// Con qué dígito terminan los cuadrados de 0..n-1. Cada hilo cuenta los
// dígitos de su trozo en un vector propio y el hilo principal junta los k.
// Uso: ./digitos_hilos n
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

// Cada hilo recorre su trozo [ini, fin) y cuenta en `conteo`, que es suyo:
// diez casillas, una por dígito. Se acumula en un arreglo local y se copia
// una sola vez al final.
void contar(const vector<int> &v, size_t ini, size_t fin, vector<long> &conteo) {
  long local[10] = {0};
  for (size_t i = ini; i < fin; i++) local[v[i]]++;
  for (int d = 0; d < 10; d++) conteo[d] = local[d];
}

vector<long> digitos(const vector<int> &v, int k) {
  size_t n = v.size(), paso = n / k;
  vector<vector<long>> conteos(k, vector<long>(10, 0));  // uno por hilo
  vector<thread> hilos;
  for (int h = 0; h < k; h++) {
    size_t ini = h * paso;
    size_t fin = (h == k - 1) ? n : ini + paso;  // el último trozo toma el resto
    hilos.emplace_back(contar, cref(v), ini, fin, ref(conteos[h]));
  }
  for (auto &t : hilos) t.join();  // ningún conteo se lee antes de esto
  vector<long> total(10, 0);
  for (auto &c : conteos)
    for (int d = 0; d < 10; d++) total[d] += c[d];
  return total;
}

int main(int argc, char **argv) {
  size_t n = argc > 1 ? strtoull(argv[1], nullptr, 10) : 1000000;
  vector<int> v(n);
  for (size_t i = 0; i < n; i++) v[i] = (i * i) % 10;  // último dígito de i²

  for (int k : {1, 2, 4, 8}) {
    auto t0 = high_resolution_clock::now();
    vector<long> total = digitos(v, k);
    auto t1 = high_resolution_clock::now();
    printf("hilos %d n %zu %.1f ms digitos", k, n,
           duration_cast<microseconds>(t1 - t0).count() / 1000.0);
    for (int d = 0; d < 10; d++) printf(" %ld", total[d]);
    printf("\n");
  }
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -pthread -o digitos_hilos digitos_hilos.cpp
./digitos_hilos 100000000
```

```
hilos 1 n 100000000 78,0 ms digitos 10000000 20000000 0 0 20000000 10000000 20000000 0 0 20000000
hilos 2 n 100000000 40,4 ms digitos 10000000 20000000 0 0 20000000 10000000 20000000 0 0 20000000
hilos 4 n 100000000 21,4 ms digitos 10000000 20000000 0 0 20000000 10000000 20000000 0 0 20000000
hilos 8 n 100000000 11,5 ms digitos 10000000 20000000 0 0 20000000 10000000 20000000 0 0 20000000
```

Un cuadrado solo termina en 0, 1, 4, 5, 6 o 9, y de cada diez enteros
seguidos uno termina en 0, dos en 1, dos en 4, uno en 5, dos en 6 y dos en 9;
las diez casillas suman `n`. Aquí ocho hilos dejan el tiempo en la séptima
parte: por cada dato hay un incremento que depende del valor leído, y la
memoria alcanza a servir a todos. En el producto de la parte el trabajo por
dato es una multiplicación y nada más, y ahí lo que manda es cuánto tarda la
memoria en entregar 400 MB: la curva se dobla antes.

El programa de la parte hace dos tandas de hilos: una que escribe `w` y otra
que la suma. La segunda solo puede arrancar cuando la primera terminó por
completo, o sea, después del `join` de todos los hilos de la primera.

### Ejemplo con TBB

Cuántas veces baja una serie: `d[i] = v[i] - v[i-1]` con `parallel_for` sobre
un rango que arranca en 1, y después el conteo de los `d[i]` negativos con
`parallel_reduce`. Recibe `n`, el grano y el tope de hilos, igual que el
programa de la parte.

```cpp
// bajadas_tbb.cpp
// Cuántas veces baja una serie: d[i] = v[i] - v[i-1] con parallel_for, y
// después se cuentan los d[i] negativos con parallel_reduce.
// Uso: ./bajadas_tbb n [grano] [hilos]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <tbb/blocked_range.h>
#include <tbb/global_control.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/task_arena.h>
#include <vector>

using namespace std;
using namespace std::chrono;

// d[i] = v[i] - v[i-1] para i en [1, n). El rango arranca en 1, no en 0.
void diferencias(const vector<int> &v, vector<int> &d, size_t grano) {
  tbb::parallel_for(tbb::blocked_range<size_t>(1, v.size(), grano),
                    [&](const tbb::blocked_range<size_t> &r) {
                      for (size_t i = r.begin(); i < r.end(); i++)
                        d[i] = v[i] - v[i - 1];
                    });
}

// Cuenta los negativos: identidad 0L, cuerpo que acumula sobre un subrango,
// combinación que suma dos parciales.
long negativos(const vector<int> &d, size_t grano) {
  return tbb::parallel_reduce(
      tbb::blocked_range<size_t>(1, d.size(), grano), 0L,
      [&](const tbb::blocked_range<size_t> &r, long acumulado) -> long {
        for (size_t i = r.begin(); i < r.end(); i++)
          if (d[i] < 0) acumulado++;
        return acumulado;
      },
      [](long a, long b) -> long { return a + b; });
}

int main(int argc, char **argv) {
  size_t n = argc > 1 ? strtoull(argv[1], nullptr, 10) : 1000000;
  size_t grano = argc > 2 ? strtoull(argv[2], nullptr, 10) : 1;
  int hilos = argc > 3 ? atoi(argv[3]) : 0;

  // El tope vive mientras viva el objeto; con hilos = 0 no se crea.
  unique_ptr<tbb::global_control> tope;
  if (hilos > 0)
    tope = make_unique<tbb::global_control>(
        tbb::global_control::max_allowed_parallelism, hilos);
  int efectivos = hilos > 0 ? hilos : tbb::this_task_arena::max_concurrency();

  vector<int> v(n), d(n, 0);
  for (size_t i = 0; i < n; i++) v[i] = (i * 7) % 13;  // sube y baja

  auto t0 = high_resolution_clock::now();
  diferencias(v, d, grano);
  long bajadas = negativos(d, grano);
  auto t1 = high_resolution_clock::now();
  printf("tbb hilos %d grano %zu n %zu %.1f ms bajadas %ld\n", efectivos,
         grano, n, duration_cast<microseconds>(t1 - t0).count() / 1000.0,
         bajadas);
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -o bajadas_tbb bajadas_tbb.cpp -ltbb
./bajadas_tbb 100000000 1 1
./bajadas_tbb 100000000 1 2
./bajadas_tbb 100000000 1 4
./bajadas_tbb 100000000
./bajadas_tbb 100000000 100000
./bajadas_tbb 100000000 10000000
```

```
tbb hilos 1 grano 1 n 100000000 97,5 ms bajadas 53846153
tbb hilos 2 grano 1 n 100000000 58,8 ms bajadas 53846153
tbb hilos 4 grano 1 n 100000000 44,9 ms bajadas 53846153
tbb hilos 12 grano 1 n 100000000 46,4 ms bajadas 53846153
tbb hilos 12 grano 100000 n 100000000 43,7 ms bajadas 53846153
tbb hilos 12 grano 10000000 n 100000000 45,1 ms bajadas 53846153
```

La serie `7i mod 13` baja siete veces por cada trece pasos, y 7/13 de cien
millones son 53.846.153. De cuatro hilos a doce el tiempo ya no baja: es el
mismo tope de memoria del ejemplo anterior. Con grano de diez millones
quedan diez trozos para doce hilos, y con doce hilos eso ya se nota poco.
El flujo corre con cuatro, y ahí se nota más.

### Lo que suele fallar

- **`error: static assertion failed: std::thread arguments must be invocable
  after conversion to rvalues`** al compilar. La función pide `vector<int> &`
  o `long &` y el hilo recibió la variable a secas, que se copia. Falta el
  `ref` o el `cref` en el `emplace_back`.
- **`terminate called without an active exception`** y el programa aborta
  con código 134. Un `std::thread` se destruyó sin `join`: se olvidó el ciclo
  de `join`, o el vector de hilos salió de alcance antes de él, o se hizo
  `hilos.clear()` con hilos todavía sin unir.
- **La suma da un número negativo o menor de lo esperado con TBB**, por
  ejemplo `-694967296` en lugar de 3.600.000.000. La identidad del
  `parallel_reduce` es `0` y no `0L`: el tipo del resultado se toma de ella,
  el acumulado se guarda en un `int` y desborda. Lo mismo pasa con
  `std::thread` si el parcial es `int`.
- **`undefined reference to tbb::detail::r1::...`** al enlazar. Falta `-ltbb`,
  o está antes del archivo fuente: en Ubuntu el enlazador descarta las
  bibliotecas que aparecen antes de que alguien las necesite, y `-ltbb` va
  al final, como en el `Makefile`.
- **El tope de hilos no hace nada**: el programa imprime `hilos 2` pero tarda
  lo mismo que con todos. El `global_control` se creó dentro de un `if` o de
  un bloque y se destruyó al salir de él, antes del `parallel_for`. Por eso
  el `main` de la parte lo guarda en un `unique_ptr` declarado afuera.

### Enlaces

- [std::thread en cppreference](https://en.cppreference.com/w/cpp/thread/thread):
  la clase, sus constructores y por qué destruir un hilo sin unir aborta.
- [std::ref y std::cref](https://en.cppreference.com/w/cpp/utility/functional/ref):
  cómo pasar una referencia a un hilo y qué es un `reference_wrapper`.
- [parallel_for en la guía de oneTBB](https://uxlfoundation.github.io/oneTBB/main/tbb_userguide/parallel_for_os.html):
  el ciclo paralelo con `blocked_range` y lambda, explicado desde cero.
- [Cómo controlar el grano](https://uxlfoundation.github.io/oneTBB/main/tbb_userguide/Controlling_Chunking_os.html)
  y [el particionador automático](https://uxlfoundation.github.io/oneTBB/main/tbb_userguide/Automatic_Chunking.html):
  qué significa el tercer argumento de `blocked_range` y qué decide TBB
  cuando vale 1.
- [parallel_reduce en la especificación](https://oneapi-spec.uxlfoundation.org/specifications/oneapi/latest/elements/onetbb/source/algorithms/functions/parallel_reduce_func):
  las dos formas de la función, con la firma exacta de la identidad, el
  cuerpo y la combinación.

## Parte 2: balanceo de carga

### Lo que se usa

- `std::atomic<int> siguiente{0}` (`#include <atomic>`): un entero que varios
  hilos pueden leer y modificar sin cerrojo y sin carrera. Cada operación
  sobre él es indivisible.
- `siguiente.fetch_add(1)`: suma 1 y devuelve el valor que había antes, no
  el nuevo. Es la operación que reparte por demanda: si dos hilos la llaman
  al mismo tiempo, uno recibe `i` y el otro `i + 1`, nunca los dos el mismo.
  El valor útil es el que devuelve la llamada, no una lectura posterior del
  contador.
- El mismo `fetch_add(1)` sirve para reservar una casilla en un vector de
  salida compartido: el valor que devuelve es una posición que ningún otro
  hilo recibió, y el hilo escribe ahí sin cerrojo. Es lo que hace el ejemplo.
- El lambda `[&, h] { ... }` que ya usa `bloques`: captura todo por
  referencia salvo `h`, que se copia. Cada hilo conserva el valor de `h` que
  tenía la vuelta del ciclo en que fue lanzado.
- `hilos.emplace_back(lambda)` y `t.join()`, igual que en la parte anterior.
- La medida que hace el flujo de Actions: `time` de bash con
  `TIMEFORMAT='%R %U %S'`, y el cociente `(usuario + sistema) / reloj`, que
  dice cuántos procesadores trabajaron en promedio. Con cuatro hilos que
  reparten bien queda cerca de 4; con uno solo, cerca de 1.

### Ejemplo

Los primos menores que cuatro millones, escritos en un vector compartido.
Cada hilo revisa un bloque contiguo de candidatos y, por cada primo, reserva
con `fetch_add` la siguiente casilla libre de la salida. Comprobar si un
número es primo cuesta más cuanto mayor es, así que el hilo del último bloque
recibe las tareas más caras; el programa mide cuánto tardó cada uno para que
eso se vea.

```cpp
// primos_bloques.cpp
// Los primos menores que N, en un vector compartido. Cada hilo revisa un
// bloque contiguo de candidatos y, por cada primo que encuentra, reserva una
// casilla de salida con fetch_add. Probar si un número es primo cuesta más
// cuanto mayor es, así que los bloques no cuestan lo mismo.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

const long N = 4000000;
const int HILOS = 4;

bool es_primo(long x) {
  if (x < 2) return false;
  for (long d = 2; d * d <= x; d++)
    if (x % d == 0) return false;
  return true;
}

int main() {
  vector<long> primos(N / 4);   // sobra sitio: hay muchos menos primos que eso
  atomic<long> escritos{0};     // la próxima casilla libre de `primos`
  vector<double> ms(HILOS, 0);  // cuánto tardó cada hilo
  vector<thread> hilos;
  for (int h = 0; h < HILOS; h++)
    hilos.emplace_back([&, h] {  // h por copia: cada hilo conserva el suyo
      auto t0 = high_resolution_clock::now();
      long ini = h * (N / HILOS), fin = (h + 1) * (N / HILOS);
      for (long x = ini; x < fin; x++)
        if (es_primo(x)) {
          long pos = escritos.fetch_add(1);  // el valor viejo: esa casilla es mía
          primos[pos] = x;
        }
      auto t1 = high_resolution_clock::now();
      ms[h] = duration_cast<microseconds>(t1 - t0).count() / 1000.0;
    });
  for (auto &t : hilos) t.join();

  primos.resize(escritos.load());  // solo valen las casillas reservadas
  sort(primos.begin(), primos.end());  // los hilos reservaron entremezclados
  for (int h = 0; h < HILOS; h++)
    printf("hilo %d candidatos [%ld, %ld) %.1f ms\n", h, h * (N / HILOS),
           (h + 1) * (N / HILOS), ms[h]);
  printf("primos %zu ultimo %ld\n", primos.size(), primos.back());
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -pthread -o primos_bloques primos_bloques.cpp
TIMEFORMAT='reloj %R s, usuario %U s, sistema %S s'; time ./primos_bloques
```

```
hilo 0 candidatos [0, 1000000) 343,6 ms
hilo 1 candidatos [1000000, 2000000) 576,2 ms
hilo 2 candidatos [2000000, 3000000) 723,1 ms
hilo 3 candidatos [3000000, 4000000) 829,4 ms
primos 283146 ultimo 3999971
reloj 0,845 s, usuario 2,475 s, sistema 0,003 s
```

Hay 283.146 primos por debajo de cuatro millones y el mayor es 3.999.971; si
el conteo cambiara de una corrida a otra, dos hilos habrían escrito en la
misma casilla. El hilo 3 tarda casi dos veces y media lo que el 0, y el
programa termina cuando termina él: el cociente 2,475 / 0,845 da 2,9
procesadores en promedio con cuatro hilos lanzados. Es la misma cuenta que
hace el flujo, y lo que la baja es el tiempo que los tres primeros hilos pasan
esperando. El reparto por demanda de la parte corrige eso con el mismo
contador: en lugar de reservar casillas de salida, `fetch_add` entrega el
número de la próxima tarea, y cada hilo vuelve por otra al terminar la suya.

### Lo que suele fallar

- **Los totales de `turnos` o `demanda` no coinciden con el de `bloques`**, y
  cambian de una corrida a otra. El contador se lee dos veces: una para
  comparar con `TAREAS` y otra para tomar la tarea, y entre las dos otro
  hilo avanzó. La tarea se toma del valor que devuelve `fetch_add`, en una
  sola expresión, y se compara ese valor.
- **Un hilo hace el trabajo de otro y una casilla de parciales queda en
  cero.** El lambda capturó `h` por referencia con `[&]`; cuando el hilo
  arrancó, el ciclo ya había cambiado `h`. Con `[&, h]` cada hilo se lleva su
  copia. Reproducido con cuatro hilos: la casilla 0 quedó sin escribir y la
  1 se escribió dos veces.
- **El reparto por demanda tarda más que los bloques en el patrón
  creciente.** El contador se protege además con un `mutex`, o cada tarea
  escribe en un `atomic<long>` compartido dentro del ciclo en lugar de
  acumular en una local y escribir una vez al final. Sesenta y cuatro
  `fetch_add` son baratos; sesenta y cuatro millones no.
- **El flujo reporta menos de 1,4 procesadores en promedio.** Los hilos se
  lanzan y se unen dentro del mismo ciclo: `emplace_back` seguido de `join`
  en la misma vuelta hace que corran de a uno. Primero se lanzan todos,
  después se unen todos.
- **`error: use of deleted function std::atomic<int>::atomic(const
  std::atomic<int>&)`**. Un `atomic` no se copia; el lambda intentó
  capturarlo por valor con `[=]`, o se pasó a una función por valor. Se
  captura por referencia.

### Enlaces

- [std::atomic en cppreference](https://en.cppreference.com/w/cpp/atomic/atomic):
  qué garantiza un entero atómico y qué operaciones tiene.
- [atomic::fetch_add](https://en.cppreference.com/w/cpp/atomic/atomic/fetch_add):
  la firma, y la frase que dice que devuelve el valor anterior.
- [Expresiones lambda](https://en.cppreference.com/w/cpp/language/lambda):
  la sintaxis de captura, con la diferencia entre `[&]`, `[=]` y `[&, h]`.
- [std::thread::join](https://en.cppreference.com/w/cpp/thread/thread/join):
  cuándo bloquea y qué pasa si se llama dos veces.
- [La cláusula schedule de OpenMP](https://www.openmp.org/spec-html/5.0/openmpsu41.html):
  `static`, `static, 1` y `dynamic`: los mismos tres repartos con otro
  nombre.

## Parte 3: el máximo por divide y vencerás

### Lo que se usa

- La recursión con corte por profundidad: la función recibe un
  presupuesto `prof` de niveles que todavía pueden crear hilos. Cada llamada
  que parte el tramo lanza un hilo para una mitad, resuelve la otra en el
  hilo actual y pasa `prof - 1` a las dos. Cuando `prof` llega a cero, o el
  tramo baja de `MINIMO`, la llamada sigue con la versión secuencial. Con
  presupuesto `p` trabajan `2^p` hilos a la vez.
- `std::thread hilo([&] { resultado = f(v, ini, med, prof - 1); });`: el hilo
  se crea con un lambda que captura por referencia la variable donde deja su
  resultado. La variable se declara antes del hilo y se lee después
  del `join`, nunca entre los dos.
- `hilo.join()`: en la recursión va antes de combinar las dos mitades. Sin
  él, el `max` se calcula con un valor que el otro hilo todavía no escribió.
- `std::max(a, b)` (`#include <algorithm>`): la combinación de esta parte,
  que cuesta una comparación.
- `std::thread::hardware_concurrency()`: cuántos hilos puede correr la
  máquina a la vez. Sirve para saber a partir de qué profundidad los hilos
  nuevos ya solo se turnan un procesador.

### Ejemplo

El número 42 de Fibonacci por divide y vencerás: `fib(n)` es
`fib(n - 1) + fib(n - 2)`, y las dos llamadas son independientes. La versión
paralela lanza un hilo para `fib(n - 1)` mientras el hilo actual calcula
`fib(n - 2)`, con el mismo presupuesto `prof` de la parte. No hay vector ni
tramo: lo que se parte es el problema.

```cpp
// fib_dyv.cpp
// El n-ésimo número de Fibonacci por divide y vencerás: fib(n) es
// fib(n - 1) + fib(n - 2). La versión paralela lanza un hilo para una de las
// dos llamadas mientras el hilo actual hace la otra, con corte por profundidad.
#include <chrono>
#include <cstdio>
#include <thread>

using namespace std;
using namespace std::chrono;

const int N = 42;
const int MINIMO = 20;  // por debajo de esto no vale la pena crear hilos

long fib_sec(int n) {
  if (n < 2) return n;
  return fib_sec(n - 1) + fib_sec(n - 2);
}

// `prof` es cuántos niveles más pueden crear hilos. Cada nivel que parte
// resta uno; al llegar a cero la llamada sigue en secuencial.
long fib_par(int n, int prof) {
  if (prof == 0 || n < MINIMO) return fib_sec(n);
  long grande = 0;
  thread hilo([&] { grande = fib_par(n - 1, prof - 1); });  // en otro hilo
  long chica = fib_par(n - 2, prof - 1);  // en este, sin esperar
  hilo.join();  // `grande` solo se lee después del join
  return grande + chica;
}

int main() {
  auto t0 = high_resolution_clock::now();
  long base = fib_sec(N);
  auto t1 = high_resolution_clock::now();
  double ms_base = duration_cast<microseconds>(t1 - t0).count() / 1000.0;
  printf("secuencial %.1f ms fib %ld\n", ms_base, base);

  for (int prof = 0; prof <= 4; prof++) {
    auto a = high_resolution_clock::now();
    long f = fib_par(N, prof);
    auto b = high_resolution_clock::now();
    double ms = duration_cast<microseconds>(b - a).count() / 1000.0;
    printf("prof %d hilos %d %.1f ms fib %ld aceleracion %.2f\n", prof,
           1 << prof, ms, f, ms_base / ms);
  }
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -pthread -o fib_dyv fib_dyv.cpp
./fib_dyv
```

```
secuencial 411,1 ms fib 267914296
prof 0 hilos 1 412,1 ms fib 267914296 aceleracion 1,00
prof 1 hilos 2 239,1 ms fib 267914296 aceleracion 1,72
prof 2 hilos 4 158,2 ms fib 267914296 aceleracion 2,60
prof 3 hilos 8 91,1 ms fib 267914296 aceleracion 4,51
prof 4 hilos 16 76,9 ms fib 267914296 aceleracion 5,34
```

Las seis filas dan 267.914.296. Con dos hilos la aceleración se queda en 1,7
y no en 2: `fib(n - 1)` cuesta 1,6 veces lo que `fib(n - 2)`, así que el hilo
que hace la rama chica termina antes y se queda esperando en el `join`. Cada
nivel de profundidad reparte mejor ese desequilibrio, y por eso la
aceleración sigue subiendo con dieciséis hilos en seis núcleos: aquí el límite
es el cálculo, no la memoria. En la parte las dos mitades cuestan lo mismo, el
límite lo pone la lectura de 400 MB y el codo cae en otra profundidad.

### Lo que suele fallar

- **Todas las filas dan el mismo tiempo que la secuencial y aceleración
  cercana a 1.** El `join` está justo después de crear el hilo, antes de
  resolver la otra mitad: el hilo actual espera en vez de trabajar. El
  orden es lanzar, resolver la propia mitad, unir, combinar.
- **El máximo sale como 0 o como basura en las profundidades mayores que
  0.** El resultado del hilo se lee antes del `join`, o la variable que
  captura el lambda está declarada dentro de un bloque que ya terminó cuando
  el hilo escribe.
- **`terminate called without an active exception`** en la fila de `prof 1`
  o más. Una rama de la recursión devuelve sin unir el hilo, por ejemplo con
  un `return` temprano después de crearlo.
- **La aceleración con `prof 4` es peor que con `prof 2`, o el programa se
  arrastra.** `prof` no se resta en la llamada recursiva y cada nivel vuelve
  a crear hilos hasta que el tramo baja de `MINIMO`: son mil hilos para cien
  millones de enteros.
- **`prof 0` tarda lo mismo que `prof 4`.** El corte comprueba `prof < 0` o
  no lo comprueba: con presupuesto cero la función tiene que delegar en la
  versión secuencial sin crear ningún hilo.

### Enlaces

- [Constructor de std::thread](https://en.cppreference.com/w/cpp/thread/thread/thread):
  cómo se crea un hilo a partir de un lambda y qué se copia.
- [std::thread::join](https://en.cppreference.com/w/cpp/thread/thread/join):
  la espera que hace visible el resultado del otro hilo.
- [hardware_concurrency](https://en.cppreference.com/w/cpp/thread/thread/hardware_concurrency):
  cuántos hilos soporta la máquina, y por qué puede devolver 0.
- [std::max](https://en.cppreference.com/w/cpp/algorithm/max): la combinación
  de dos mitades en esta parte.
- [Expresiones lambda](https://en.cppreference.com/w/cpp/language/lambda):
  la captura por referencia que lleva el resultado de vuelta.

## Parte 4: el máximo acumulado

### Lo que se usa

- El esquema de dos pasadas con `BLOQUES` hilos. Primera pasada: cada
  hilo recorre su bloque y reporta un resumen (aquí, el máximo del bloque).
  Entre las dos, en el hilo principal: para cada bloque, el punto de partida
  se calcula a partir de los resúmenes de los bloques anteriores. Segunda
  pasada: cada hilo rehace su bloque arrancando desde ese punto y escribe la
  salida. El trabajo se hace dos veces, y aun así con varios núcleos termina
  antes que la cadena de dependencias.
- `std::vector<std::thread> hilos` con dos tandas: después del ciclo de
  `join` de la primera, `hilos.clear()` deja el vector listo para la
  segunda. Un vector con hilos ya unidos se puede vaciar; uno con hilos
  sin unir, no.
- `LONG_MIN` (`#include <climits>`): el valor con el que arranca un máximo
  que todavía no ha visto nada. Es la identidad del máximo, como `0` lo es
  de la suma.
- `thread::hardware_concurrency()` fija `BLOQUES` en el programa: un bloque
  por procesador lógico, entre 2 y 16.
- `a == b` sobre dos `vector<long>`: la comparación posición por posición que
  imprime `coinciden` o `DIFIEREN`.

### Ejemplo

Los registros que cumplen una condición, copiados a un vector de salida en
el orden en que aparecen. La posición de cada uno depende de cuántos pasaron
antes, y esa es la cadena que la versión secuencial arrastra con `push_back`.
En dos pasadas: cada bloque cuenta cuántos de los suyos pasan; el punto de
partida de un bloque es la suma de los conteos de los anteriores; y en la
segunda pasada cada bloque vuelve a decidir y escribe desde ahí. Decidir si
un registro pasa cuesta, y ese costo es lo que hace que el reparto se note.

```cpp
// filtro_dos_pasadas.cpp
// Los registros que cumplen una condición, en el orden en que aparecen. La
// posición de cada uno en la salida depende de cuántos pasaron antes; en dos
// pasadas cada bloque cuenta los suyos, se calcula dónde arranca cada bloque
// y cada bloque escribe los suyos desde ahí.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

const size_t N = 20000000;
const int BLOQUES = max(2, min(16, (int)thread::hardware_concurrency()));

// Decidir si un registro pasa cuesta; sin ese costo solo se mueve memoria y
// ningún reparto se nota.
bool pasa(long x) {
  long y = x;
  for (int k = 0; k < 40; k++) y = (y * 48271 + 11) % 2147483647;
  return y % 5 == 0;
}

vector<long> secuencial(const vector<long> &v) {
  vector<long> r;
  for (long x : v)
    if (pasa(x)) r.push_back(x);  // dónde va cada uno depende de los anteriores
  return r;
}

vector<long> en_dos_pasadas(const vector<long> &v) {
  size_t paso = v.size() / BLOQUES;
  auto limites = [&](int h, size_t &ini, size_t &fin) {
    ini = h * paso;
    fin = (h == BLOQUES - 1) ? v.size() : ini + paso;
  };

  // Primera pasada: cada bloque cuenta cuántos de los suyos pasan.
  vector<size_t> cuantos(BLOQUES, 0);
  vector<thread> hilos;
  for (int h = 0; h < BLOQUES; h++)
    hilos.emplace_back([&, h] {
      size_t ini, fin;
      limites(h, ini, fin);
      size_t c = 0;
      for (size_t i = ini; i < fin; i++) c += pasa(v[i]);
      cuantos[h] = c;
    });
  for (auto &t : hilos) t.join();

  // Entre pasadas: el bloque h arranca donde terminan los bloques 0 a h - 1.
  vector<size_t> desde(BLOQUES, 0);
  for (int h = 1; h < BLOQUES; h++) desde[h] = desde[h - 1] + cuantos[h - 1];
  vector<long> r(desde[BLOQUES - 1] + cuantos[BLOQUES - 1]);

  // Segunda pasada: cada bloque vuelve a decidir y escribe desde su punto.
  hilos.clear();  // los hilos ya unidos se descartan; el vector queda vacío
  for (int h = 0; h < BLOQUES; h++)
    hilos.emplace_back([&, h] {
      size_t ini, fin;
      limites(h, ini, fin);
      size_t pos = desde[h];
      for (size_t i = ini; i < fin; i++)
        if (pasa(v[i])) r[pos++] = v[i];
    });
  for (auto &t : hilos) t.join();
  return r;
}

int main() {
  vector<long> v(N);
  for (size_t i = 0; i < N; i++) v[i] = (i * 7919) % 100003;

  auto t0 = high_resolution_clock::now();
  vector<long> a = secuencial(v);
  auto t1 = high_resolution_clock::now();
  vector<long> b = en_dos_pasadas(v);
  auto t2 = high_resolution_clock::now();

  printf("secuencial %.1f ms pasan %zu\n",
         duration_cast<microseconds>(t1 - t0).count() / 1000.0, a.size());
  printf("dos_pasadas %.1f ms pasan %zu bloques %d %s\n",
         duration_cast<microseconds>(t2 - t1).count() / 1000.0, b.size(),
         BLOQUES, a == b ? "coinciden" : "DIFIEREN");
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -pthread -o filtro_dos_pasadas filtro_dos_pasadas.cpp
TIMEFORMAT='reloj %R s, usuario %U s, sistema %S s'; time ./filtro_dos_pasadas
```

```
secuencial 2724,3 ms pasan 3999084
dos_pasadas 559,0 ms pasan 3999084 bloques 12 coinciden
reloj 3,320 s, usuario 8,888 s, sistema 0,037 s
```

Pasan 3.999.084 registros, uno de cada cinco, y las dos salidas coinciden
posición por posición. Con doce bloques la versión en dos pasadas decide cada
registro dos veces y termina 4,9 veces antes. Con dos bloques haría el doble
de trabajo entre dos y tardaría lo mismo que la secuencial; el flujo corre en
cuatro procesadores y ahí la ganancia ya se ve. En la parte el resumen de
cada bloque no es un conteo y el punto de partida no sale de una suma; lo que
se conserva es la forma: un resumen por bloque, el acumulado de los resúmenes
anteriores y la segunda pasada desde ese punto.

### Lo que suele fallar

- **`DIFIEREN`, y el último valor sí coincide.** La segunda pasada arranca
  cada bloque desde su propio resumen en lugar del acumulado de los
  anteriores, o el bloque 0 arranca desde algo distinto de la identidad. El
  último valor sale bien porque el último bloque ya vio todo; los del medio
  no.
- **`DIFIEREN` en las primeras posiciones de cada bloque.** El punto de
  partida se calculó con los resúmenes hasta el bloque actual incluido, la
  versión inclusiva del acumulado, cuando hace falta la exclusiva: el bloque
  `h` arranca de lo que vieron los bloques `0` a `h - 1`.
- **`DIFIEREN` en el último bloque.** `N` no es múltiplo de `BLOQUES` y el
  último bloque termina en `(h + 1) * paso` en vez de en `N`; las últimas
  posiciones de `r` quedan en cero.
- **La versión en dos pasadas tarda más que la secuencial en el flujo.** Los
  hilos se unen en la misma vuelta del ciclo en que se lanzan, o la segunda
  pasada no reparte y la hace el hilo principal completa. El cociente de
  procesadores que reporta el flujo lo delata.
- **`terminate called without an active exception`** entre las dos pasadas.
  `hilos.clear()` se llamó antes del ciclo de `join` de la primera pasada.

### Enlaces

- [std::inclusive_scan y exclusive_scan](https://en.cppreference.com/w/cpp/algorithm/inclusive_scan):
  el acumulado con un operador cualquiera; la diferencia entre inclusivo y
  exclusivo es la del punto de partida de cada bloque.
- [vector::clear](https://en.cppreference.com/w/cpp/container/vector/clear):
  vaciar el vector de hilos entre las dos tandas.
- [Comparación de vectores](https://en.cppreference.com/w/cpp/container/vector/operator_cmp):
  qué hace `a == b` con dos `vector<long>`.
- [climits](https://en.cppreference.com/w/cpp/types/climits): `LONG_MIN` y
  los demás límites de los enteros.
- [hardware_concurrency](https://en.cppreference.com/w/cpp/thread/thread/hardware_concurrency):
  de dónde sale `BLOQUES`.

## Cómo compilar y ejecutar en la máquina propia

**Debian y Ubuntu.** Hace falta el compilador, `make` y los encabezados de
oneTBB; el `script.sh` del repositorio instala el último:

```bash
sudo apt install -y g++ make
bash script.sh          # sudo apt update && sudo apt install -y libtbb-dev
```

Las banderas son las del `Makefile`: `-std=c++17 -O2`, más `-pthread` para
los programas con `std::thread` y `-ltbb` para el de TBB, al final de la
línea:

```bash
g++ -std=c++17 -O2 -pthread -o hadamard_hilos hadamard_hilos.cpp
g++ -std=c++17 -O2 -o hadamard_tbb hadamard_tbb.cpp -ltbb
```

Cada regla del `Makefile` compila, corre y borra el ejecutable, y deja la
salida en el `.txt` de la parte; `make -n hadamard` muestra los comandos sin
ejecutarlos. Sin `libtbb-dev` la compilación se detiene en el `#include` con
`fatal error: tbb/blocked_range.h: No such file or directory`; con el paquete
pero sin `-ltbb`, en el enlazado con `undefined reference to
tbb::detail::r1::...`. En Arch el paquete se llama `onetbb`.

Las mediciones se hacen con la máquina tranquila, sin navegador cargando ni
compilación en otra terminal. Cada corrida se repite dos o tres veces y la
primera se descarta. `nproc` dice cuántos procesadores lógicos hay;
`hardware_concurrency()` devuelve ese mismo número.

**Windows con WSL2.** Se instala Ubuntu desde la tienda o con `wsl --install`,
y adentro todo es igual que arriba, `script.sh` incluido. El repositorio se
clona dentro del sistema de archivos de Linux (`~/`, no `/mnt/c/...`), porque
el acceso a los discos de Windows es lento y los tiempos salen inflados. WSL2
ve por omisión todos los procesadores; si `nproc` muestra menos, el tope
está en `.wslconfig`.

**macOS.** El `g++` que trae Xcode es Clang con otro nombre, y compila los
programas con `std::thread` tal cual, con `-pthread` o sin él. oneTBB se
instala con Homebrew, y como Homebrew no la deja en las rutas por omisión del
compilador hay que indicarlas:

```bash
brew install tbb
g++ -std=c++17 -O2 -I"$(brew --prefix tbb)/include" -o hadamard_tbb hadamard_tbb.cpp \
    -L"$(brew --prefix tbb)/lib" -ltbb
```

Para que `make hadamard` funcione sin tocar el `Makefile`, se exportan
`CPATH="$(brew --prefix tbb)/include"` y `LIBRARY_PATH="$(brew --prefix tbb)/lib"`
antes de llamarlo. En los Mac con chip de Apple `hardware_concurrency()`
cuenta juntos los núcleos de rendimiento y los de eficiencia, y la curva de
aceleración se dobla antes de lo que sugiere ese número.

- [libtbb-dev en Ubuntu](https://packages.ubuntu.com/noble/libtbb-dev): qué
  versión de oneTBB trae el paquete y qué archivos instala.
- [Cómo instalar oneTBB desde el código fuente](https://github.com/uxlfoundation/oneTBB/blob/master/INSTALL.md):
  para una distribución sin paquete.
- [Opciones de enlazado de GCC](https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html):
  `-l`, `-L` y `-pthread`, y por qué el orden de `-l` cuenta.
- [Instalar WSL](https://learn.microsoft.com/en-us/windows/wsl/install): la
  guía oficial, con `wsl --install`.
- [tbb en Homebrew](https://formulae.brew.sh/formula/tbb): la fórmula y las
  rutas donde queda instalada.
