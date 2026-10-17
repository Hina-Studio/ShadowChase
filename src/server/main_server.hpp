#ifndef SLASHCO_MAIN_SERVER_HPP
#define SLASHCO_MAIN_SERVER_HPP

// Embeddable dedicated-server core for mobile/in-process hosting.
// Clients link this TU with -DSLASHCO_SERVER_LIB, spawn slashcoServerMain on a
// background thread and stop it via slashcoServerStop().
#ifdef SLASHCO_SERVER_LIB
int slashcoServerMain(int argc, char** argv);
void slashcoServerStop();
#endif

#endif
