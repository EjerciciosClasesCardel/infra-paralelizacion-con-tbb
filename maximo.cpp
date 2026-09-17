// Divide y vencerás: el máximo de cien millones de enteros, repartido con
// std::thread y corte por profundidad.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

const size_t N = 100000000;
const size_t MINIMO = 100000;  // por debajo de esto el tramo se resuelve directo

// Llenado determinista: la misma secuencia en todas las máquinas.
void llenar(vector<int> &v) {
  unsigned long long s = 20260915;
  for (size_t i = 0; i < v.size(); i++) {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    v[i] = (int)(s >> 33);  // 31 bits: entre 0 y 2147483647
  }
}

// El máximo del rango [ini, fin) es el mayor de los máximos de sus dos
// mitades. Los tramos pequeños se resuelven con un ciclo.
int maximo_sec(const vector<int> &v, size_t ini, size_t fin) {
  if (fin - ini <= MINIMO) {
    int mejor = v[ini];
    for (size_t i = ini + 1; i < fin; i++) mejor = max(mejor, v[i]);
    return mejor;
  }
  size_t med = ini + (fin - ini) / 2;
  int izq = maximo_sec(v, ini, med);
  int der = maximo_sec(v, med, fin);
  return max(izq, der);
}

// TODO: la misma recursión, con un hilo nuevo para la mitad izquierda
// mientras el hilo actual resuelve la derecha. `prof` es el presupuesto de
// niveles que todavía pueden crear hilos; cuando se acaba, o cuando el tramo
// baja de MINIMO, el tramo se resuelve con maximo_sec.
int maximo_par(const vector<int> &v, size_t ini, size_t fin, int prof) {
  if (prof == 0 || fin - ini <= MINIMO) return maximo_sec(v, ini, fin);
  size_t med = ini + (fin - ini) / 2;
  int izq = 0;
  thread hilo([&] { izq = maximo_par(v, ini, med, prof - 1); });  // mitad izquierda, en otro hilo
  int der = maximo_par(v, med, fin, prof - 1);  // mitad derecha, en este hilo
  hilo.join();  // `izq` solo se lee después del join
  return max(izq, der);
}

int main() {
  vector<int> v(N);
  llenar(v);

  auto t0 = high_resolution_clock::now();
  int base = maximo_sec(v, 0, N);
  auto t1 = high_resolution_clock::now();
  double ms_base = duration_cast<microseconds>(t1 - t0).count() / 1000.0;
  printf("secuencial %.1f ms max %d\n", ms_base, base);

  for (int prof = 0; prof <= 4; prof++) {
    auto a = high_resolution_clock::now();
    int m = maximo_par(v, 0, N, prof);
    auto b = high_resolution_clock::now();
    double ms = duration_cast<microseconds>(b - a).count() / 1000.0;
    printf("prof %d hilos %d %.1f ms max %d aceleracion %.2f\n", prof, 1 << prof,
           ms, m, ms_base / ms);
  }
  return 0;
}
