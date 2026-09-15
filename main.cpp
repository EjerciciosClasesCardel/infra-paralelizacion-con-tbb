#include <cstdio>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <vector>

using namespace std::chrono;
using namespace std;
const int VECTOR_SIZE = 10000000;

vector<long> v(VECTOR_SIZE);
vector<long> u(VECTOR_SIZE);
vector<long> w(VECTOR_SIZE);

void llenar(vector<long> &vector) {
  tbb::parallel_for(tbb::blocked_range<size_t>(0, vector.size()),
                    [&](const tbb::blocked_range<size_t> &r) {
                      for (size_t i = r.begin(); i != r.end(); ++i) {
                        vector[i] = rand() % 100;
                      }
                    });
}

void producto_vector(vector<long> &v, vector<long> &u, vector<long> &w) {
  tbb::parallel_for(tbb::blocked_range<size_t>(0, v.size()),
                    [&](const tbb::blocked_range<size_t> &r) {
                      for (size_t i = r.begin(); i != r.end(); ++i) {
                        w[i] = v[i] * u[i];
                      }
                    });
}

void sumar_vector(vector<long> &w, long &suma) {
  long sum = tbb::parallel_reduce(
      tbb::blocked_range<size_t>(0, w.size()), 0L,
      [&](const tbb::blocked_range<size_t> &r, long local_sum) {
        for (size_t i = r.begin(); i != r.end(); ++i) {
          local_sum += w[i];
        }
        return local_sum;
      },
      std::plus<long>());
  suma = sum;
}

int main() {
  llenar(v);
  llenar(u);
  long suma = 0L;

  auto start = high_resolution_clock::now();

  producto_vector(v, u, w);
  sumar_vector(w, suma);
  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start);
  printf("Suma: %ld\n", suma);
  printf("Tiempo de ejecución: %ld ms\n", duration.count());

  // Secuencial
  start = high_resolution_clock::now();
  long suma_seq = 0L;
  for (size_t i = 0; i < VECTOR_SIZE; ++i) {
    suma_seq += v[i] * u[i];
  }
  end = high_resolution_clock::now();
  duration = duration_cast<milliseconds>(end - start);
  printf("Suma secuencial: %ld\n", suma_seq);
  printf("Tiempo de ejecución secuencial: %ld ms\n", duration.count());
  return 0;
}
