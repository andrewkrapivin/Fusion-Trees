CXX = g++-11
CXXFLAGS = -pthread -MMD -MP -march=icelake-client -std=c++20 -O3
 
SRCDIR = src
OBJDIR = bin
TESTDIR = test
TARGET = $(OBJDIR)/fusion_tree_test
TARGET_PARALLEL = $(OBJDIR)/parallel_fusion_tree_test
TARGET_VSIZE_PARALLEL = $(OBJDIR)/vsize_fusion_tree_test
TARGET_ILP = $(OBJDIR)/testILP


src = $(wildcard $(SRCDIR)/*.cpp)
obj = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(src))
srctest = $(wildcard $(TESTDIR)/*.cpp)
objtest = $(patsubst $(TESTDIR)/%.cpp, $(TESTDIR)/%.o, $(srctest))
dep = $(obj:.o=.d)

all:
	mkdir -p bin \
	
	make $(TARGET)
	make $(TARGET_PARALLEL)
	make $(TARGET_VSIZE_PARALLEL)

test:
	mkdir -p bin \
	
	make $(TARGET)

parallel_test:
	mkdir -p bin \
	
	make $(TARGET_PARALLEL)

vsize_parallel_test:
	mkdir -p bin \
	
	make $(TARGET_VSIZE_PARALLEL)

testILP:
	mkdir -p bin \
	
	make $(TARGET_ILP)

$(TARGET): test/test.o $(obj)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TARGET_PARALLEL): test/test_parallel.o $(obj)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TARGET_VSIZE_PARALLEL): test/test_vsize.o $(obj)
	$(CXX) $(CXXFLAGS) $^ -o $@
	
$(TARGET_ILP): test/testILP.o $(obj)
	$(CXX) $(CXXFLAGS) $^ -o $@

test/test.o: test/test.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test/test_parallel.o: test/test_parallel.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test/test_vsize.o: test/test_vsize.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test/testILP.o: test/testILP.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(dep) 

.PHONY: clean prints
clean:
	rm -r -f bin \
	
	rm -f $(obj) $(TARGET) $(dep) $(objtest)

prints:
	echo '$(OBJDIR)'
	echo '$(src) $(obj)'
	echo '$(objtest)'

