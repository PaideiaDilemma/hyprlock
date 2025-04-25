#include "harness.hpp"
#include "shared.hpp"

using namespace NTestSessionLock;

int main(int argc, char** argv) {
    int               ret = 0;

    std::cout << "Running Hyprlock test..." << std::endl;

    SSesssionLockTest test;
    test.m_clientPath = "build/hyprlock";
    test.m_configPath = "assets/example.conf";

    auto testResult = run(test);
    EXPECT(testResult, eTestResult::OK);

    return ret;
}
