// 2003-04-26 Petur Runolfsson  <peturr02@ru.is>

// Copyright (C) 2003 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// 27.3 Standard iostream objects

// XXX cygwin does not support mkfifo
// { dg-do run { xfail *-*-cygwin* } }

#include <fstream>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <testsuite_hooks.h>

// Check that cout.flush() is called when last ios_base::Init is destroyed.
void test07()
{
  using namespace std;
  bool test __attribute__((unused)) = true;

  const char* name = "tmp_fifo4";

  signal(SIGPIPE, SIG_IGN);

  unlink(name);  
  mkfifo(name, S_IRWXU);
  
  int child = fork();
  VERIFY( child != -1 );

  if (child == 0)
    {
      filebuf fbout;
      sleep(1);
      fbout.open(name, ios_base::out);
      cout.rdbuf(&fbout);
      fbout.sputc('a');
      sleep(2);
      // NB: fbout is *not* destroyed here!
      exit(0);
    }
  
  filebuf fbin;
  fbin.open(name, ios_base::in);
  sleep(2);
  filebuf::int_type c = fbin.sbumpc();
  VERIFY( c != filebuf::traits_type::eof() );
  VERIFY( c == filebuf::traits_type::to_int_type('a') );

  fbin.close();
}

int
main()
{
  test07();
  return 0;
}
