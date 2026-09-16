// Dependencias: el máximo acumulado de una serie. Para cada día, el saldo más
// alto visto hasta ese día, incluido él. El ciclo directo arrastra el máximo
// de una posición a la siguiente; la versión en dos pasadas lo reparte.
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdio>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

const size_t N = 20000000;

// Cuántos bloques: uno por núcleo, entre 2 y 16.
const int BLOQUES = max(2, min(16, (int)thread::hardware_concurrency()));

// El saldo del día se calcula a partir del registro x, y calcularlo cuesta.
// Sin ese costo el programa solo mueve memoria y ningún reparto se nota.
long saldo(long x) {
  long y = x;
  for (int k = 0; k < 40; k++) y = (y * 1103515245 + 12345) % 1000003;
  return x + y % 100;
}

void secuencial(const vector<long> &v, vector<long> &r) {
  long mejor = LONG_MIN;
  for (size_t i = 0; i < v.size(); i++) {  // cada paso necesita el anterior
    mejor = max(mejor, saldo(v[i]));
    r[i] = mejor;
  }
}

// TODO: en dos pasadas, con BLOQUES hilos. Primera: cada hilo calcula el
// máximo de su bloque. Entre las dos: para cada bloque, el máximo acumulado
// de los bloques anteriores es su punto de partida. Segunda: cada hilo rehace
// su bloque arrancando desde ese punto y escribe r.
void en_dos_pasadas(const vector<long> &v, vector<long> &r) {
}

// Llenado determinista: una serie que sube con ruido, la misma en todas las
// máquinas. El máximo acumulado cambia a lo largo de todo el recorrido.
void llenar(vector<long> &v) {
  unsigned long long s = 20260915;
  for (size_t i = 0; i < v.size(); i++) {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    v[i] = (long)(i / 10) + (long)(s >> 56);
  }
}

int main() {
  vector<long> v(N), a(N, 0), b(N, 0);
  llenar(v);

  auto t0 = high_resolution_clock::now();
  secuencial(v, a);
  auto t1 = high_resolution_clock::now();
  en_dos_pasadas(v, b);
  auto t2 = high_resolution_clock::now();

  bool iguales = a == b;
  printf("secuencial %.1f ms ultimo %ld\n",
         duration_cast<microseconds>(t1 - t0).count() / 1000.0, a[N - 1]);
  printf("dos_pasadas %.1f ms ultimo %ld bloques %d %s\n",
         duration_cast<microseconds>(t2 - t1).count() / 1000.0, b[N - 1],
         BLOQUES, iguales ? "coinciden" : "DIFIEREN");
  return 0;
}
