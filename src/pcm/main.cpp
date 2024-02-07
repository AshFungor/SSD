#include <iostream>

// Protos
#include <hello-world.pb.h>

int main() {
    PHelloWorld message;
    message.set_data("Hello, world!\n");
    std::cout << message.data();

    return 0;
}