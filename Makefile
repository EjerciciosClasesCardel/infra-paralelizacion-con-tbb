CXX = g++
FLAGS = -std=c++17 -O2

# Parte 1: producto de Hadamard con std::thread y con TBB, dos tamaños.
hadamard:
	$(CXX) $(FLAGS) -pthread -o hadamard_hilos hadamard_hilos.cpp
	$(CXX) $(FLAGS) -o hadamard_tbb hadamard_tbb.cpp -ltbb
	( ./hadamard_hilos 1000000; \
	  ./hadamard_hilos 100000000; \
	  ./hadamard_tbb 1000000; \
	  ./hadamard_tbb 100000000 1 1; \
	  ./hadamard_tbb 100000000 1 2; \
	  ./hadamard_tbb 100000000 1 4; \
	  ./hadamard_tbb 100000000; \
	  ./hadamard_tbb 100000000 1000; \
	  ./hadamard_tbb 100000000 100000; \
	  ./hadamard_tbb 100000000 10000000 ) | tee hadamard.txt
	rm -f hadamard_hilos hadamard_tbb

# Parte 2: tres repartos sobre dos patrones de costo.
balanceo:
	$(CXX) $(FLAGS) -pthread -o balanceo balanceo.cpp
	./balanceo | tee balanceo.txt
	rm -f balanceo

# Parte 3: el máximo por divide y vencerás, con corte por profundidad.
maximo:
	$(CXX) $(FLAGS) -pthread -o maximo maximo.cpp
	./maximo | tee maximo.txt
	rm -f maximo

# Parte 4: el máximo acumulado en dos pasadas.
prefijos:
	$(CXX) $(FLAGS) -pthread -o prefijos prefijos.cpp
	./prefijos | tee prefijos.txt
	rm -f prefijos

todo: hadamard balanceo maximo prefijos

limpiar:
	rm -f hadamard_hilos hadamard_tbb balanceo maximo prefijos
	rm -f hadamard.txt balanceo.txt maximo.txt prefijos.txt
