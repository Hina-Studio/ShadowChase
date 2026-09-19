#include "ClientApp.hpp"

int main(int argc, char** argv) {
    ClientApp app;
    if (!app.init(argc, argv)) {
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}
