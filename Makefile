CXX = g++
CXXFLAGS = -MMD -march=icelake-client -std=c++17 -O3
CXXFLAGSPARALLEL = -pthread -MMD -march=icelake-client -std=c++17
 
SRCDIR = src
OBJDIR = bin
TESTDIR = test
TARGET = $(OBJDIR)/fusion_tree_test
TARGET_PARALLEL = $(OBJDIR)/parallel_fusion_tree_test


src = $(wildcard $(SRCDIR)/*.cpp)
obj = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(src))
srctest = $(wildcard $(TESTDIR)/*.cpp)
objtest = $(patsubst $(TESTDIR)/%.cpp, $(TESTDIR)/%.o, $(srctest))
dep = $(obj:.o=.d)

all:
	mkdir -p bin \
	
	make $(TARGET)
	make $(TARGET_PARALLEL)

test:
	mkdir -p bin \
	
	make $(TARGET)

parallel_test:
	mkdir -p bin \
	
	make $(TARGET_PARALLEL)

$(TARGET): test/test.o $(obj)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TARGET_PARALLEL): test/test_parallel.o $(obj)
	$(CXX) $(CXXFLAGS) $^ -o $@

test/test.o: test/test.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test/test_parallel.o: test/test_parallel.cpp
	$(CXX) $(CXXFLAGSPARALLEL) -c $< -o $@

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

