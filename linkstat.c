/*
 * This file is part of Linkstat.
 * Copyright (C) 2019 Chris Tallon
 *
 * This program is free software: You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#define DEFAULT_PING_IP "127.0.0.1"


#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ncurses.h>

void sigHandler(int signum);
int gai();
void pingMachine();
void draw();
void redraw();

#define WHITEBLUE 1
#define WHITEGREEN 2
#define WHITERED 3

int lastPingResult = -1;
time_t lastPingTime = 0;
time_t lastResponseTime = 0;

int interval = 5;
char* intervalStr;

char ip[1024];
int ip6 = 0;

int winchFlag = 0;

int main(int argc, char** argv)
{
  int c;
  while((c = getopt(argc, argv, "6n:")) != -1)
  {
    switch(c)
    {
      case 'n':
        interval = atoi(optarg);
        break;
      case '6':
        ip6 = 1;
        break;
    }
  }

  if (interval < 1) { fprintf(stderr, "Interval\n"); exit(-1); }

  asprintf(&intervalStr, "%ds", interval);
  if (strlen(intervalStr) > 20) { fprintf(stderr, "Interval string\n"); exit(-1); }

  if ((argc - optind) > 1) { fprintf(stderr, "Parameters\n"); exit(-1); }
  else if (optind == argc) strcpy(ip, DEFAULT_PING_IP);
  else
  {
    strncpy(ip, argv[optind], 1024);
    ip[1023] = '\0';
  }

  if (gai()) { fprintf(stderr, "GAI\n"); exit(-1); }

  signal(SIGWINCH, sigHandler);

  initscr();
  curs_set(0);
  start_color();
  init_pair(WHITEBLUE, COLOR_WHITE, COLOR_BLUE);
  init_pair(WHITEGREEN, COLOR_WHITE, COLOR_GREEN);
  init_pair(WHITERED, COLOR_WHITE, COLOR_RED);

  draw();

  struct timeval sleepTime;

  while(1)
  {
    pingMachine();
    if (winchFlag) redraw();
    else draw();

    sleepTime.tv_sec = interval;
    sleepTime.tv_usec = 0;
    while ((select(0, NULL, NULL, NULL, &sleepTime) == -1) && (errno == EINTR)) redraw();
  }
  
  endwin();
  return 0;
}

int gai()
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));

  if (!ip6)
  {
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_ADDRCONFIG; // must have a confd if for any returned ip
  }
  else
  {
    hints.ai_family = AF_INET6;
  }

  struct addrinfo* ai;

  int gai = getaddrinfo(ip, NULL, &hints, &ai);
  if (gai) return gai;

  if (ai->ai_family == AF_INET)
  {
    struct sockaddr_in* in = (struct sockaddr_in*)ai->ai_addr;
    strcpy(ip, inet_ntoa(in->sin_addr));
  }
  else if (ai->ai_family == AF_INET6)
  {
    ip6 = 1;
    struct sockaddr_in6* in6 = (struct sockaddr_in6*)ai->ai_addr;
    char buf[128];
    inet_ntop(AF_INET6, in6->sin6_addr.s6_addr, buf, 128);
    strcpy(ip, buf);
  }

  freeaddrinfo(ai);
  return 0;
}

void redraw()
{
  winchFlag = 0;
  endwin();
  refresh();
  clear();
  draw();
}

void draw()
{
  int rows, cols, prow;
  getmaxyx(stdscr, rows, cols);
  prow = rows / 2;

  struct tm lastResponseTimeTm;
  char lastResponseTimeStr[128];
  localtime_r(&lastResponseTime, &lastResponseTimeTm);

  struct tm lastPingTimeTm;
  char lastPingTimeStr[100];
  localtime_r(&lastPingTime, &lastPingTimeTm);
  strftime(lastPingTimeStr, 50, "%H:%M:%S ", &lastPingTimeTm);
  strcat(lastPingTimeStr, intervalStr);

  move(prow, 0);
  clrtoeol();
  move(rows - 1, 0);
  clrtoeol();

  if (lastPingResult == -1)
  {
    bkgd(COLOR_PAIR(WHITEBLUE));
    attron(A_BOLD);
    mvprintw(prow, (cols - 7) / 2, "INIT...");
    lastResponseTimeStr[0] = '\0';

    strcpy(lastPingTimeStr, intervalStr);
  }
  else if (lastPingResult == 0)
  {
    bkgd(COLOR_PAIR(WHITEGREEN));
    attron(A_BOLD);
    mvprintw(rows / 2, (cols - 7) / 2, "LINK UP");
    strftime(lastResponseTimeStr, 128, "%Y-%m-%d %H:%M:%S", &lastResponseTimeTm);
  }
  else if (lastPingResult == 1)
  {
    bkgd(COLOR_PAIR(WHITERED));
    attron(A_BOLD);
    mvprintw(rows / 2, (cols - 9) / 2, "LINK DOWN");

    if (lastResponseTime == 0)
      lastResponseTimeStr[0] = '\0';
    else
      strftime(lastResponseTimeStr, 128, "DS: %Y-%m-%d %H:%M:%S", &lastResponseTimeTm);
  }
  else
  {
    bkgd(COLOR_PAIR(WHITEBLUE));
    attron(A_BOLD);
    mvprintw(rows / 2, (cols - 5) / 2, "ERROR");
    lastResponseTimeStr[0] = '\0';
  }

  mvprintw(rows - 1, 0, "%s  %s", ip, lastResponseTimeStr);
  mvprintw(rows - 1, cols - strlen(lastPingTimeStr), lastPingTimeStr);

  refresh();
}

void pingMachine()
{
  // Returns: 0 - host up, 1 - host down, 2 - error

  pid_t child = fork();
  if (child == -1) goto pingError;
  if (child == 0) // I am the child
  {
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    int er = execl("/bin/ping", "ping", "-c", "1", "-w", "2", ip, NULL);
    if (er == -1) _exit(50); // Child forked but exec failed. Kill child, keep away from ping exit codes.
  } // execl should not return. so else: I am the parent

  lastPingTime = time(NULL);
  int childStatus;
  int wr = waitpid(child, &childStatus, 0);
  if (wr == -1) goto pingError;
  if (!WIFEXITED(childStatus)) goto pingError;
  lastPingResult = WEXITSTATUS(childStatus);
  if (lastPingResult == 50) goto pingError;

  if (lastPingResult == 0) lastResponseTime = time(NULL);

  return;

  pingError:
  lastPingResult = 2;
}

void sigHandler(int signum)
{
  ++winchFlag;
}
