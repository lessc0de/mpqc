//
// formio.cc
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Curtis Janssen <cljanss@ca.sandia.gov>
// Maintainer: LPS
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#include <util/misc/formio.h>

#include <stdio.h> // for vsprintf
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

char *SCFormIO::default_basename_ = 0;
int  SCFormIO::ready_ = 0;
long SCFormIO::nindent_ = 0;
long SCFormIO::indent_size_ = 0;
long SCFormIO::skip_indent_ = 0;
int SCFormIO::node_to_print_ = 0;
int SCFormIO::debug_ = 0;
int SCFormIO::parallel_ = 0;
int SCFormIO::me_ = 0;
ofstream SCFormIO::nullstream_;

char *
SCFormIO::fileext_to_filename(const char *ext)
{
  const char *basename;

  if (default_basename_) basename = default_basename_;
  else basename = "SC";

  char * res = new char[strlen(basename) + strlen(ext) + 1];
  strcpy(res, basename);
  strcat(res, ext);

  return res;
}

void
SCFormIO::set_default_basename(const char *basename)
{
  if (default_basename_) delete[] default_basename_;

  if (basename)
      default_basename_ = strcpy(new char[strlen(basename)+1], basename);
  else
      default_basename_ = 0;
}

const char *
SCFormIO::default_basename()
{
  return default_basename_;
}

void
SCFormIO::set_printnode(int n)
{
  node_to_print_ = n;
}

void
SCFormIO::set_debug(int n)
{
  debug_ = n;
}

void
SCFormIO::init_mp(int me)
{
  if (!ready_) init();
  me_ = me;
  parallel_=1;
}
  
void
SCFormIO::init()
{
  ready_ = 1;
  nindent_ = ios::xalloc();
  indent_size_ = ios::xalloc();
  skip_indent_ = ios::xalloc();

  cout.iword(skip_indent_) = 0;
  cout.iword(indent_size_) = 0;
  cout.iword(nindent_) = 0;
  cerr.iword(skip_indent_) = 0;
  cerr.iword(indent_size_) = 0;
  cerr.iword(nindent_) = 0;

  if (nullstream_.bad() || nullstream_.fail())
    nullstream_.open("/dev/null");
}

ios&
SCFormIO::indent(ios&o)
{
  if (!ready_) init();
  long &skip = o.iword(skip_indent_);
  if (skip) {
      skip--;
      return o;
    }
  if (debug_ && parallel_) {
      char nn[24];
      sprintf(nn,"node %5d:",me_);
      for (int i=0; i < strlen(nn); i++) o.rdbuf()->sputc(nn[i]);
    }
  long n = o.iword(nindent_);
  for (int i=0; i<n; i++) o.rdbuf()->sputc(' ');
  return o;
}

ios&
SCFormIO::incindent(ios&o)
{
  if (!ready_) init();
  long &n = o.iword(nindent_);
  long size = o.iword(indent_size_);
  if (size == 0) size = 2;
  else if (size < 0) size = 0;
  n += size;
  return o;
}

ios&
SCFormIO::decindent(ios&o)
{
  if (!ready_) init();
  long &n = o.iword(nindent_);
  long size = o.iword(indent_size_);
  if (size == 0) size = 2;
  else if (size < 0) size = 0;
  n -= size;
  if (n<0) n=0;
  return o;
}

long
SCFormIO::getindent(ios&o)
{
  if (!ready_) init();
  return o.iword(nindent_);
}

void
SCFormIO::setindent(ios&o, long n)
{
  if (!ready_) init();
  o.iword(nindent_) = n;
}

ios&
SCFormIO::skipnextindent(ios&o)
{
  if (!ready_) init();
  o.iword(skip_indent_)++;
  return o;
}

ostream&
SCFormIO::node0(ostream& o)
{
  if (!ready_) init();
  
  if (!debug_ && node_to_print_ >= 0
      && parallel_ && node_to_print_ != me_)
    return nullstream_;

  return o;
}

ios&
indent(ios& o)
{
  return SCFormIO::indent(o);
}

ios&
decindent(ios& o)
{
  return SCFormIO::decindent(o);
}

ios&
incindent(ios& o)
{
  return SCFormIO::incindent(o);
}

ios&
skipnextindent(ios& o)
{
  return SCFormIO::skipnextindent(o);
}

ostream&
node0(ostream& o)
{
  return SCFormIO::node0(o);
}

/////////////////////////////////////////////////////////////////////////////

scprintf::scprintf(const char *fmt, ...)
{
  va_list args;
  
  va_start(args, fmt);

  str[0] = '\0';
  
  // hopefully this won't overflow
  if (fmt && fmt[0]!='\0') {
    if (vsprintf(str, fmt, args) > 1023) {
      cerr << indent << "scprintf overflow\n";
      abort();
    }
  }

  va_end(args);
}

ostream&
operator<<(ostream& o, const scprintf& s)
{
  o << s.str << flush;
  return o;
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// eval: (c-set-style "CLJ")
// End:
