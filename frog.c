#define FROG_IMPLEMENTATION
#include "thirdparty/frog/frog.h"


#define FILENAME "grua"
#define FLAGS "-Wall", "-Wextra"
#define LIBS "-lglad", "-lglfw"
#define SRC "main.cpp"
#define CC "g++"

int
main(int argc, char *argv[])
{
        frog_rebuild_itself(argc, argv);

        frog_cmd_wait(CC, SRC, LIBS, FLAGS, "-o", FILENAME);

        //UNREACHABLE("Just do not execute");

        frog_cmd_wait("sudo", "systemctl", "stop", "keyd");
        frog_cmd_wait("./" FILENAME);
        frog_cmd_wait("sudo", "systemctl", "start", "keyd");

        frog_cmd_wait("rm", FILENAME);

        return 0;
}
