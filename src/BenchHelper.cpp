#include "BenchHelper.hpp"
#include <chrono>
#include <iostream>

using namespace std;

BenchHelper::BenchHelper(int numThreads) : numThreads(numThreads) {
    unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
    mt19937 generator (seed);
    threads.reserve(numThreads);
}

void BenchHelper::timeFunction(function<void()> F, const string& name) {
    auto start = chrono::high_resolution_clock::now();
    F();
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end-start);
    double ms = ((double)duration.count())/1000.0;
    cout << ms << " ms for " << name << endl;
}

void BenchHelper::timeThreadedFunction(const string& name) {
    timeThreadedFunction(tmpFunctionsForThreads, name);
    tmpFunctionsForThreads.clear();
}

void BenchHelper::timeThreadedFunction(vector<function<void()>>& functions, const string& name) {
    threads.clear();
    auto start = chrono::high_resolution_clock::now();
    for(size_t i = 0; i < functions.size(); i++) {
        threads.push_back(std::thread(functions[i]));
    }
    for(auto& th: threads) {
        th.join();
    }
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end-start);
    double ms = ((double)duration.count())/1000.0;
    cout << ms << " ms for " << name << " (" << functions.size() << " threads)" << endl;
}

void BenchHelper::addFunctionForThreadTest(function<void()> F) {
    tmpFunctionsForThreads.push_back(F);
}