#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <stdexcept>

static char *applicationName = NULL;
static char *pidFileName = NULL;

//TODO: add comments through out the while

static void daemonizeMe() {
}

static void cleanGlobalMemory() {
}

void printHelp(void) {
}

int main(int argc, char **argv) {
  //Get the application name
  applicationName = argv[0];

  static struct option commandLineOptions[] = {
    {"pid_file",    required_argument,  0, 'p'},
    {"daemon",      no_argument,        0, 'd'},
    {"help",        no_argument,        0, 'h'},
    {NULL,          0,                  0,  0 }
  };

  //Process all the command line arguments
  int getopt_longRV;
  int optionIndex = 0;
  int daemonize = 0; //0 means do not daemonize
  while((getopt_longRV = getopt_long(argc, argv, "p:dh", commandLineOptions, &optionIndex)) != -1) {
    switch(getopt_longRV) {
      case 'p':
        pidFileName = strdup(optarg);
        break;
      case 'd':
        daemonize = 1;
        break;
      case 'h':
        printHelp();
        return EXIT_SUCCESS;
      case '?':
        printHelp();
        return EXIT_FAILURE;
      default:
        break;
    }
  }

  //If daemonize flag is passed
  if(daemonize == 1) {
    daemonizeMe();
  }

  //Free memory
  cleanGlobalMemory();

  return 0;
}
