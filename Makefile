lazy: 
	g++ SimpleAlloc.cpp test.cpp fusion_tree.cpp HelperFuncs.cpp FusionBTree.cpp -march=icelake-client -std=c++17 -O3