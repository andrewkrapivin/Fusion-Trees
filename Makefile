CC = g++-11

lazy: 
	$(CC) SimpleAlloc.cpp test.cpp fusion_tree.cpp HelperFuncs.cpp FusionBTree.cpp FusionQSort.cpp -march=icelake-client -std=c++17 -O3

no_opt: 
	$(CC) SimpleAlloc.cpp test.cpp fusion_tree.cpp HelperFuncs.cpp FusionBTree.cpp FusionQSort.cpp -march=icelake-client -std=c++17

debug: 
	$(CC) SimpleAlloc.cpp test.cpp fusion_tree.cpp HelperFuncs.cpp FusionBTree.cpp FusionQSort.cpp -march=icelake-client -std=c++17 -ggdb