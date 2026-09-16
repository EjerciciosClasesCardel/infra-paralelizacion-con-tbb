// Balanceo de carga: sesenta y cuatro tareas de costo desigual repartidas
// entre cuatro hilos de tres maneras, sobre dos patrones de costo.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

const int TAREAS = 64;
const int HILOS = 4;

// Trabajo sintético proporcional a `costo`.
long trabajo(long costo) {
  long s = 0;
  for (long k = 0; k < costo; k++) s += k % 3;
  return s;
}

// Patrón creciente: la tarea i cuesta proporcional a la cuarta potencia de i.
long creciente(int i) { return 8L * i * i * i * i; }

// Patrón periódico: una de cada cuatro tareas cuesta veinticinco veces las otras.
long periodico(int i) { return (i % 4 == 3) ? 25 * 3800000L : 3800000L; }

using Patron = long (*)(int);

// Bloques contiguos: el hilo h hace las tareas [h * 16, (h + 1) * 16).
long bloques(Patron costo) {
  vector<long> parciales(HILOS, 0);
  vector<thread> hilos;
  int por_hilo = TAREAS / HILOS;
  for (int h = 0; h < HILOS; h++)
    hilos.emplace_back([&, h] {
      long s = 0;
      for (int i = h * por_hilo; i < (h + 1) * por_hilo; i++) s += trabajo(costo(i));
      parciales[h] = s;
    });
  for (auto &t : hilos) t.join();
  long total = 0;
  for (long p : parciales) total += p;
  return total;
}

// TODO: por turnos. El hilo h hace las tareas h, h + 4, h + 8, ...
long turnos(Patron costo) {
  return 0;
}

// TODO: por demanda. Un contador compartido entrega la siguiente tarea
// libre; cada hilo toma una, la hace y vuelve por otra hasta que se acaban.
// Un atomic<int> con fetch_add reparte sin cerrojo.
long demanda(Patron costo) {
  return 0;
}

int main() {
  struct { const char *nombre; Patron costo; } patrones[] = {
      {"creciente", creciente}, {"periodico", periodico}};
  struct { const char *nombre; long (*reparto)(Patron); } repartos[] = {
      {"bloques", bloques}, {"turnos", turnos}, {"demanda", demanda}};

  for (auto &p : patrones)
    for (auto &r : repartos) {
      auto t0 = high_resolution_clock::now();
      long total = r.reparto(p.costo);
      auto t1 = high_resolution_clock::now();
      printf("%s %s %.1f ms total %ld\n", p.nombre, r.nombre,
             duration_cast<microseconds>(t1 - t0).count() / 1000.0, total);
    }
  return 0;
}
