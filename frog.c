#define FROG_IMPLEMENTATION
#include "thirdparty/frog/frog.h"


#define FILENAME "grua"
#define FLAGS "-Wall", "-Wextra", "-Wno-unused-parameter"
#define LIBS "-lglad", "-lglfw"
#define SRC "main.cpp"
#define CC "g++"

int
main(int argc, char *argv[])
{
        frog_rebuild_itself(argc, argv);

        frog_cmd_wait(CC, SRC, LIBS, FLAGS, "-o", FILENAME, NULL);

        //UNREACHABLE("Just do not execute");

        frog_cmd_wait("sudo", "systemctl", "stop", "keyd", NULL);
        frog_cmd_wait("./" FILENAME, NULL);
        frog_cmd_wait("sudo", "systemctl", "start", "keyd", NULL);

        return 0;
}
