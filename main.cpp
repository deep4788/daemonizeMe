// Copyright (c) 2017 Deep Aggarwal
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>

static char *applicationName = NULL;
static char *logDirectory = NULL;
static char *pidFileName = NULL;

// TODO: add comments through out the while

/**
 * @brief Callback function for handling signals.
 *
 * @param sig Identifier of a signal
 */
void handleSignal(int sig) {
  if (sig == SIGINT) {
    syslog(LOG_INFO, "Stopping %s", applicationName);

    // Unlock and close lockfile
    if (pidFd != -1) {
      lockf(pidFd, F_ULOCK, 0);
      close(pidFd);
    }

    // Delete lockfile
    if (pidFileName != NULL) {
      unlink(pidFileName);
    }

    // Reset signal handling to default behavior
    signal(SIGINT, SIG_DFL);
  }
}

static uid_t getUserID(const char *name) {
  struct passwd *pwentry = 0;
  // TODO: I should use getpwnam_r(...) for improved thread safety
  pwentry = getpwnam(name);
  if (pwentry == NULL) {
    std::string sname = name;
    throw std::runtime_error("User \""+sname+"\" is not found");
  }

  return pwentry->pw_uid;
}

static gid_t getGroupID(const char *name) {
  struct group *grentry = 0;
  // TODO: I should use getgrnam_r(...) for improved thread safety
  grentry = getgrnam("haldaemon");
  if (grentry == NULL) {
    std::string sname = name;
    throw std::runtime_error("Group \""+sname+"\" is not found");
  }

  return grentry->gr_gid;
}

/**
 * @brief This function will daemonize this application
 */
static void daemonizeMe() {
  pid_t pid = 0;
  int fd;

  // Fork off the parent process to ensures that
  //   the child process is not a process group leader, so
  //   that it is possible for that process tocreate
  //   a new session and become a session leader
  pid = fork();

  // If error occurred
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }

  // On success, terminate the parent
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  // On success, the child process becomes processgroup
  // and session group leader. Since a controlling terminal
  // is associated with a session, and this new session
  // has not yet acquired a controlling terminal our
  // process now has no controlling terminal,
  // which is a good thing for daemons
  if (setsid() < 0) {
    exit(EXIT_FAILURE);
  }

  // Ignore signal sent from child to parent process
  signal(SIGCHLD, SIG_IGN);

  // Fork off for the second time to ensure that
  // the new child process is not a session leader,
  // so it won't be able to (accidentally) allocate
  // a controlling terminal, since daemons are not
  // supposed to ever have a controlling terminal
  pid = fork();

  // If error occurred
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }

  // On success, terminate the parent
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  // Set new file permissions
  //
  //  "0" means that new files this daemon will
  //  create will have read and write permission for everyone
  //  (0666 or -rw-rw-rw-), and new directories that
  //  this daemon will create will have read, write and
  //  search permissions for everyone (0777 or drwxrwxrwx)
  //  This measn that we have complete control over the
  //  permissions of anything we write
  umask(0);

  // Change the working directory to the root directory
  //  This is to ensure that our process doesn't keep any directory in use.
  //  Failure to do this could make it so that an administrator couldn't unmount
  //  a filesystem, because it was our current directory
  if (chdir("/") != 0) {
    exit(EXIT_FAILURE);
  }

  // Drop priviledges since running network based daemons
  // with root permissions is considered to be a serious risk
  if (getuid() == 0) {
    // If here that means process is running as root,
    // so drop the root privileges
    uid_t userid = NULL;
    gid_t groupid = NULL;
    try {
      // FIXME: Hard-coded since this is the user that we will deal with
      userid = getUserID("mydaemons");
      groupid = getGroupID("mydaemons");
    }
    catch(const std::runtime_error& error) {
      syslog(LOG_ERR, "\"mydaemons\" couldn't be found");
      exit(EXIT_FAILURE);
    }

    if (setgid(groupid) != 0) {
      exit(EXIT_FAILURE);
    }
    if (setuid(userid) != 0) {
      exit(EXIT_FAILURE);
    }
  }

  // If we try to get root privileges back, it
  // should fail. If it doesn't fail, we exit with failure
  if (setuid(0) != -1) {
    exit(EXIT_FAILURE);
  }

  // Close all open file descriptors
  for (fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--) {
    close(fd);
  }

  // Reopen stdin (fd = 0), stdout (fd = 1), stderr (fd = 2) as /dev/null
  stdin = fopen("/dev/null", "r");
  stdout = fopen("/dev/null", "w+");
  stderr = fopen("/dev/null", "w+");

  // Write PID of daemon to lockfile
  if (pidFileName != NULL) {
    char str[256];
    pidFd = open(pidFileName, O_RDWR|O_CREAT, 0640);
    if (pidFd < 0) {
      // Can't open lockfile
      exit(EXIT_FAILURE);
    }
    if (lockf(pidFd, F_TLOCK, 0) < 0) {
      // Can't lock file
      exit(EXIT_FAILURE);
    }

    // Get current PID
    sprintf(str, "%d\n", getpid());

    // Write PID to lockfile
    write(pidFd, str, strlen(str));
  }
}

static void startApplication() {
  while (true) {
    syslog(LOG_NOTICE, "I am daemonizeMe and I am writing to my syslog");
    auto start = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(30s);
    auto end = std::chrono::high_resolution_clock::now();
  }
}

static void cleanGlobalMemory() {
  if (pidFileName) {
    free(pidFileName);
  }
  if (logDirectory != NULL) {
    free(logDirectory);
  }

  return;
}

void printHelp(void) {
}

int main(int argc, char **argv) {
  // Get the application name
  applicationName = argv[0];

  static struct option commandLineOptions[] = {
    {"log_dir",     required_argument,  0, 'l'},
    {"pid_file",    required_argument,  0, 'p'},
    {"daemon",      no_argument,        0, 'd'},
    {"help",        no_argument,        0, 'h'},
    {NULL,          0,                  0,  0 }
  };

  // Process all the command line arguments
  int getopt_longRV;
  int optionIndex = 0;
  int daemonize = 0;  // 0 means do not daemonize
  while ((getopt_longRV = getopt_long(argc,
          argv,
          "l:p:dh",
          commandLineOptions,
          &optionIndex)) != -1) {
    switch (getopt_longRV) {
      case 'l':
        logDirectory = strdup(optarg);
        break;
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

  // If daemonize flag is passed
  if (daemonize == 1) {
    daemonizeMe();
  }

  // Open system log to write message to it
  openlog(argv[0], LOG_PID|LOG_CONS, LOG_DAEMON);
  syslog(LOG_INFO, "Started %s", applicationName);

  // Handle SIGINT by this daemon
  signal(SIGINT, handleSignal);

  // Main function to start the application
  startApplication();

  // Write final system log and close the log
  syslog(LOG_INFO, "Stopped %s", applicationName);
  closelog();

  // Free memory
  cleanGlobalMemory();

  return 0;
}
