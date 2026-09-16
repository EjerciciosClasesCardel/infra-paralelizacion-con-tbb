// Producto de Hadamard con std::thread: w[i] = u[i] * v[i] y después la suma
// de w. Las dos operaciones se reparten en k trozos disjuntos.
//
// Uso: ./hadamard_hilos n
// Imprime una línea por cada k en {1, 2, 4, 8}.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

const int A = 4;  // valor constante de u
const int B = 9;  // valor constante de v

// TODO: cada hilo calcula w[i] = u[i] * v[i] sobre su trozo [ini, fin).
void producto(const vector<int> &u, const vector<int> &v, vector<int> &w,
              size_t ini, size_t fin) {
}

// TODO: cada hilo suma su trozo de w y deja el parcial en `salida`. Acumular
// en una variable local y escribir `salida` una sola vez al final.
void sumar(const vector<int> &w, size_t ini, size_t fin, long &salida) {
}

// TODO: reparte [0, n) en k trozos, lanza los hilos del producto y los une;
// después lanza los de la suma, los une y combina los parciales. El último
// trozo se queda con el resto cuando n no es múltiplo de k.
long hadamard(const vector<int> &u, const vector<int> &v, vector<int> &w,
              int k) {
  return 0;
}

int main(int argc, char **argv) {
  size_t n = argc > 1 ? strtoull(argv[1], nullptr, 10) : 1000000;
  vector<int> u(n, A), v(n, B), w(n, 0);

  for (int k : {1, 2, 4, 8}) {
    auto t0 = high_resolution_clock::now();
    long suma = hadamard(u, v, w, k);
    auto t1 = high_resolution_clock::now();
    printf("hilos %d n %zu %.1f ms suma %ld\n", k, n,
           duration_cast<microseconds>(t1 - t0).count() / 1000.0, suma);
  }
  return 0;
}
