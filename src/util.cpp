////////////////////////////////////////////////////////////////////////////////
// taskwarrior - a command line task list manager.
//
// Copyright 2006 - 2011, Paul Beckingham, Federico Hernandez.
// All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the
//
//     Free Software Foundation, Inc.,
//     51 Franklin Street, Fifth Floor,
//     Boston, MA
//     02110-1301
//     USA
//
////////////////////////////////////////////////////////////////////////////////

#define L10N                                           // Localization complete.

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <string>
#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>

#include <Date.h>
#include <text.h>
#include <main.h>
#include <i18n.h>
#include <util.h>
#include <cmake.h>

extern Context context;

////////////////////////////////////////////////////////////////////////////////
// Uses std::getline, because std::cin eats leading whitespace, and that means
// that if a newline is entered, std::cin eats it and never returns from the
// "std::cin >> answer;" line, but it does display the newline.  This way, with
// std::getline, the newline can be detected, and the prompt re-written.
bool confirm (const std::string& question)
{
  std::vector <std::string> options;
  options.push_back ("yes");
  options.push_back ("no");

  std::string answer;
  std::vector <std::string> matches;

  do
  {
    std::cout << question
              << STRING_UTIL_CONFIRM_YN;

    std::getline (std::cin, answer);
    answer = std::cin.eof() ? STRING_UTIL_CONFIRM_NO : lowerCase (trim (answer));

    autoComplete (answer, options, matches, 1); // Hard-coded 1.
  }
  while (matches.size () != 1);

  return matches[0] == STRING_UTIL_CONFIRM_YES ? true : false;
}

////////////////////////////////////////////////////////////////////////////////
// 0 = no
// 1 = yes
// 2 = all
int confirm3 (const std::string& question)
{
  std::vector <std::string> options;
  options.push_back (STRING_UTIL_CONFIRM_YES_U);
  options.push_back (STRING_UTIL_CONFIRM_YES);
  options.push_back (STRING_UTIL_CONFIRM_NO);
  options.push_back (STRING_UTIL_CONFIRM_ALL_U);
  options.push_back (STRING_UTIL_CONFIRM_ALL);

  std::string answer;
  std::vector <std::string> matches;

  do
  {
    std::cout << question
              << " ("
              << options[1] << "/"
              << options[2] << "/"
              << options[4]
              << ") ";

    std::getline (std::cin, answer);
    answer = trim (answer);
    autoComplete (answer, options, matches, 1); // Hard-coded 1.
  }
  while (matches.size () != 1);

       if (matches[0] == STRING_UTIL_CONFIRM_YES_U) return 1;
  else if (matches[0] == STRING_UTIL_CONFIRM_YES)   return 1;
  else if (matches[0] == STRING_UTIL_CONFIRM_ALL_U) return 2;
  else if (matches[0] == STRING_UTIL_CONFIRM_ALL)   return 2;
  else                                              return 0;
}

////////////////////////////////////////////////////////////////////////////////
// 0 = no
// 1 = yes
// 2 = all
// 3 = quit
int confirm4 (const std::string& question)
{
  std::vector <std::string> options;
  options.push_back (STRING_UTIL_CONFIRM_YES_U);
  options.push_back (STRING_UTIL_CONFIRM_YES);
  options.push_back (STRING_UTIL_CONFIRM_NO);
  options.push_back (STRING_UTIL_CONFIRM_ALL_U);
  options.push_back (STRING_UTIL_CONFIRM_ALL);
  options.push_back (STRING_UTIL_CONFIRM_QUIT);

  std::string answer;
  std::vector <std::string> matches;

  do
  {
    std::cout << question
              << " ("
              << options[1] << "/"
              << options[2] << "/"
              << options[4] << "/"
              << options[5]
              << ") ";

    std::getline (std::cin, answer);
    answer = trim (answer);
    autoComplete (answer, options, matches, 1); // Hard-coded 1.
  }
  while (matches.size () != 1);

       if (matches[0] == STRING_UTIL_CONFIRM_YES_U) return 1;
  else if (matches[0] == STRING_UTIL_CONFIRM_YES)   return 1;
  else if (matches[0] == STRING_UTIL_CONFIRM_ALL_U) return 2;
  else if (matches[0] == STRING_UTIL_CONFIRM_ALL)   return 2;
  else if (matches[0] == STRING_UTIL_CONFIRM_QUIT)  return 3;
  else                           return 0;
}

////////////////////////////////////////////////////////////////////////////////
void delay (float f)
{
  struct timeval t;
  t.tv_sec = (int) f;
  t.tv_usec = int ((f - (int)f) * 1000000);

  select (0, NULL, NULL, NULL, &t);
}

////////////////////////////////////////////////////////////////////////////////
// Convert a quantity in seconds to a more readable format.
std::string formatBytes (size_t bytes)
{
  char formatted[24];

       if (bytes >=  995000000) sprintf (formatted, "%.1f %s", (bytes / 1000000000.0), STRING_UTIL_GIBIBYTES);
  else if (bytes >=     995000) sprintf (formatted, "%.1f %s", (bytes /    1000000.0), STRING_UTIL_MEBIBYTES);
  else if (bytes >=        995) sprintf (formatted, "%.1f %s", (bytes /       1000.0), STRING_UTIL_KIBIBYTES);
  else                          sprintf (formatted, "%d %s",   (int)bytes,             STRING_UTIL_BYTES);

  return commify (formatted);
}

////////////////////////////////////////////////////////////////////////////////
int autoComplete (
  const std::string& partial,
  const std::vector<std::string>& list,
  std::vector<std::string>& matches,
  int minimum/* = 2*/)
{
  matches.clear ();

  // Handle trivial case. 
  unsigned int length = partial.length ();
  if (length)
  {
    std::vector <std::string>::const_iterator item;
    for (item = list.begin (); item != list.end (); ++item)
    {
      // An exact match is a special case.  Assume there is only one exact match
      // and return immediately.
      if (partial == *item)
      {
        matches.clear ();
        matches.push_back (*item);
        return 1;
      }

      // Maintain a list of partial matches.
      else if (length >= (unsigned) minimum &&
               length <= item->length ()    &&
               partial == item->substr (0, length))
        matches.push_back (*item);
    }
  }

  return matches.size ();
}

////////////////////////////////////////////////////////////////////////////////
#ifdef HAVE_UUID

#ifndef HAVE_UUID_UNPARSE_LOWER
// Older versions of libuuid don't have uuid_unparse_lower(), only uuid_unparse()
void uuid_unparse_lower (uuid_t uu, char *out)
{
    uuid_unparse (uu, out);
    // Characters in out are either 0-9, a-z, '-', or A-Z.  A-Z is mapped to
    // a-z by bitwise or with 0x20, and the others already have this bit set
    for (size_t i = 0; i < 36; ++i) out[i] |= 0x20;
}
#endif

const std::string uuid ()
{
  uuid_t id;
  uuid_generate (id);
  char buffer[100] = {0};
  uuid_unparse_lower (id, buffer);

  // Bug found by Steven de Brouwer.
  buffer[36] = '\0';

  return std::string (buffer);
}

////////////////////////////////////////////////////////////////////////////////
#else

#ifdef HAVE_RANDOM
#define rand() random()
#endif

////////////////////////////////////////////////////////////////////////////////
const std::string uuid ()
{
  uint32_t time_low = ((rand () << 16) & 0xffff0000) | (rand () & 0xffff);
  uint16_t time_mid = rand () & 0xffff;
  uint16_t time_high_and_version = (rand () & 0x0fff) | 0x4000;
  uint16_t clock_seq = (rand () & 0x3fff) | 0x8000;
  uint8_t node [6];
  for (size_t i = 0; i < 6; i++)
    node[i] = rand() & 0xff;

  char buffer[37];
  sprintf(buffer, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    time_low, time_mid, time_high_and_version, clock_seq >> 8, clock_seq & 0xff,
    node[0], node[1], node[2], node[3], node[4], node[5]);

  return std::string (buffer);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// On Solaris no flock function exists.
#ifdef SOLARIS
int flock (int fd, int operation)
{
  struct flock fl;

  switch (operation & ~LOCK_NB)
  {
  case LOCK_SH:
    fl.l_type = F_RDLCK;
    break;

  case LOCK_EX:
    fl.l_type = F_WRLCK;
    break;

  case LOCK_UN:
    fl.l_type = F_UNLCK;
    break;

  default:
    errno = EINVAL;
    return -1;
  }

  fl.l_whence = 0;
  fl.l_start  = 0;
  fl.l_len    = 0;

  return fcntl (fd, (operation & LOCK_NB) ? F_SETLK : F_SETLKW, &fl);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// The vector must be sorted first.  This is a modified version of the run-
// length encoding algorithm.
//
// This function converts the vector:
//
//   [1, 3, 4, 6, 7, 8, 9, 11]
//
// to ths string:
//
//   1,3-4,6-9,11
//
std::string compressIds (const std::vector <int>& ids)
{
  std::stringstream result;

  int range_start = 0;
  int range_end = 0;

  for (unsigned int i = 0; i < ids.size (); ++i)
  {
    if (i + 1 == ids.size ())
    {
      if (result.str ().length ())
        result << ",";

      if (range_start < range_end)
        result << ids[range_start] << "-" << ids[range_end];
      else
        result << ids[range_start];
    }
    else
    {
      if (ids[range_end] + 1 == ids[i + 1])
      {
        ++range_end;
      }
      else
      {
        if (result.str ().length ())
          result << ",";

        if (range_start < range_end)
          result << ids[range_start] << "-" << ids[range_end];
        else
          result << ids[range_start];

        range_start = range_end = i + 1;
      }
    }
  }

  return result.str ();
}

////////////////////////////////////////////////////////////////////////////////
void combine (std::vector <int>& dest, const std::vector <int>& source)
{
  // Create a map using the sequence elements as keys.  This will create a
  // unique list, with no duplicates.
  std::map <int, int> both;
  std::vector <int>::iterator i1;
  for (i1 = dest.begin (); i1 != dest.end (); ++i1) both[*i1] = 0;

  std::vector <int>::const_iterator i2;
  for (i2 = source.begin (); i2 != source.end (); ++i2) both[*i2] = 0;

  // Now make a sequence out of the keys of the map.
  dest.clear ();
  std::map <int, int>::iterator i3;
  for (i3 = both.begin (); i3 != both.end (); ++i3)
    dest.push_back (i3->first);

  std::sort (dest.begin (), dest.end ());
}

////////////////////////////////////////////////////////////////////////////////
// Run an external executable with execvp. This means stdio goes to 
// the child process, so that it can receive user input (e.g. passwords).
//
int execute(const std::string& executable, std::vector<std::string> arguments)
{
  if (executable == "")
    return -1;

  pid_t child_pid = fork();

  if (child_pid == 0)
  {
    // this is done by the child process
    char shell[] = "bash";
    char opt[]   = "-c";

    std::string cmdline = executable;

    std::vector <std::string>::iterator it;
    for (it = arguments.begin(); it != arguments.end(); ++it)
    {
      cmdline += " " + (std::string)*it;
    }

    char** argv = new char*[4];
    argv[0] = shell;                  // bash
    argv[1] = opt;                    // -c
    argv[2] = (char*)cmdline.c_str();	// e.g. scp undo.data user@host:.task/
    argv[3] = NULL;                   // required by execv

    int ret = execvp(shell, argv);
    delete[] argv;

    exit(ret);
  }
  else
  {
    // this is done by the parent process
    int child_status;

    pid_t pid = waitpid(child_pid, &child_status, 0);

    if (pid == -1)
      return -1;
    else
      return child_status;
  }
}

////////////////////////////////////////////////////////////////////////////////
