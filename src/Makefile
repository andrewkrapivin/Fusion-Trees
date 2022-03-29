lazy: 
	g++ SimpleAlloc.cpp test.cpp fusion_tree.cpp HelperFuncs.cpp FusionBTree.cpp FusionQSort.cpp -march=icelake-client -std=c++17 -O3

no_opt: 
	g++ SimpleAlloc.cpp test.cpp fusion_tree.cpp HelperFuncs.cpp FusionBTree.cpp FusionQSort.cpp -march=icelake-client -std=c++17

parallel:
	g++ -pthread SimpleAlloc.cpp test_parallel.cpp fusion_tree.cpp HelperFuncs.cpp FusionBTree.cpp FusionQSort.cpp -march=icelake-client -std=c++17

parallel_opt:
	g++ -pthread SimpleAlloc.cpp test_parallel.cpp fusion_tree.cpp HelperFuncs.cpp FusionBTree.cpp FusionQSort.cpp -march=icelake-client -std=c++17 -O3

debug: 
	g++ SimpleAlloc.cpp test.cpp fusion_tree.cpp HelperFuncs.cpp FusionBTree.cpp FusionQSort.cpp -march=icelake-client -std=c++17 -ggdb