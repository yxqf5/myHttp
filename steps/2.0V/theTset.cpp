#include <fmt/core.h>
#include <iostream>

int main() {
    std::cout << "fmt version: " << FMT_VERSION << std::endl;
    std::string str="sdfsadf";
     fmt::print("Hello, {}!\n", "World");
    fmt::println("{}",str);

    return 0;
}
