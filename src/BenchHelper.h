#include <random>
#include <functional>
#include <string>
#include <thread>
#include <vector>

class BenchHelper {
    private:
        std::mt19937 generator;
        int numThreads; //really just a suggestion I suppose
        std::vector<std::thread> threads;
        std::vector<std::function<void()>> tmpFunctionsForThreads;

    public:
        BenchHelper(int numThreads = 0);
        void timeFunction(std::function<void()> F, const std::string& name);
        void timeThreadedFunction(const std::string& name);
        void timeThreadedFunction(std::vector<std::function<void()>>& functions, const std::string& name);
        void addFunctionForThreadTest(std::function<void()> F);
};