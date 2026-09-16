// Producto de Hadamard con Intel TBB: el mismo trabajo, sin decidir cuántos
// hilos hay ni qué trozo le toca a cada uno.
//
// Uso: ./hadamard_tbb n [grano] [hilos]
//   grano: tercer argumento de blocked_range. Con 1, el particionador decide.
//   hilos: tope de hilos del planificador. Con 0, usa todos los núcleos.
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

const int A = 4;  // valor constante de u
const int B = 9;  // valor constante de v

// TODO: llena v con `valor` usando parallel_for sobre un blocked_range con
// el grano indicado.
void llenar(vector<int> &v, int valor, size_t grano) {
}

// TODO: w[i] = u[i] * v[i] con parallel_for.
void producto(const vector<int> &u, const vector<int> &v, vector<int> &w,
              size_t grano) {
}

// TODO: la suma de w con parallel_reduce. El valor inicial es 0L y los
// parciales se combinan sumando.
long sumar(const vector<int> &w, size_t grano) {
  return 0;
}

int main(int argc, char **argv) {
  size_t n = argc > 1 ? strtoull(argv[1], nullptr, 10) : 1000000;
  size_t grano = argc > 2 ? strtoull(argv[2], nullptr, 10) : 1;
  int hilos = argc > 3 ? atoi(argv[3]) : 0;

  unique_ptr<tbb::global_control> tope;
  if (hilos > 0)
    tope = make_unique<tbb::global_control>(
        tbb::global_control::max_allowed_parallelism, hilos);
  int efectivos = hilos > 0 ? hilos : tbb::this_task_arena::max_concurrency();

  vector<int> u(n), v(n), w(n);
  llenar(u, A, grano);
  llenar(v, B, grano);

  auto t0 = high_resolution_clock::now();
  producto(u, v, w, grano);
  long suma = sumar(w, grano);
  auto t1 = high_resolution_clock::now();
  printf("tbb hilos %d grano %zu n %zu %.1f ms suma %ld\n", efectivos, grano,
         n, duration_cast<microseconds>(t1 - t0).count() / 1000.0, suma);
  return 0;
}
