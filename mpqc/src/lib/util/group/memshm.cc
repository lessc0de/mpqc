//
// memshm.cc
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

#ifndef _util_group_memshm_cc
#define _util_group_memshm_cc

#ifdef __GNUC__
#pragma implementation
#endif

#include <util/misc/formio.h>
#include <util/group/pool.h>
#include <util/group/memshm.h>
#include <limits.h>
#include <errno.h>

#ifndef SHMMAX
#define SHMMAX INT_MAX
#endif

#ifndef SHMMIN
#define SHMMIN 1
#endif

#ifndef SIMPLE_LOCK
#define SIMPLE_LOCK 1
#endif

#undef DEBUG

#define CLASSNAME ShmMemoryGrp
#define HAVE_KEYVAL_CTOR
#define PARENTS public MsgMemoryGrp
#include <util/class/classi.h>
void *
ShmMemoryGrp::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] =  MsgMemoryGrp::_castdown(cd);
  return do_castdowns(casts,cd);
}

ShmMemoryGrp::ShmMemoryGrp(const RefMessageGrp& msg):
  MsgMemoryGrp(msg)
{
  update_ = 0;
  data_ = 0;
  memory_ = 0;
  pool_ = 0;
  rangelock_ = 0;
  nregion_ = 0;
  shmid_ = 0;
  attach_address_ = 0;
}

ShmMemoryGrp::ShmMemoryGrp(const RefKeyVal& keyval):
  MsgMemoryGrp(keyval)
{
  update_ = 0;
  data_ = 0;
  memory_ = 0;
  pool_ = 0;
  rangelock_ = 0;
  nregion_ = 0;
  shmid_ = 0;
  attach_address_ = 0;
}

void
ShmMemoryGrp::set_localsize(int localsize)
{
  int i;

  cleanup();

  MsgMemoryGrp::set_localsize(localsize);

  const int poolallocation = 160000;

  update_ = new GlobalCounter[n()];

  // allocate memory both the data and the Pool
  int size = poolallocation + totalsize();
  // compute the number of shared memory regions that will be needed
  nregion_ = size/SHMMAX;
  if (size%SHMMAX) nregion_++;
  shmid_ = new int[nregion_];
  attach_address_ = new void*[nregion_];

  if (me() == 0) {

      int rsize = size;
      int isize;
      for (i=0; rsize>0; i++,rsize-=isize) {
          isize = rsize;
          if (isize > SHMMAX) isize = SHMMAX;
          else if (isize < SHMMIN) isize = SHMMIN;
          if (debug_) {
              cout << me() << ": ";
              cout << "ShmMemoryGrp: getting segment with " << isize
                   << " bytes" << endl;
            }
          shmid_[i] = shmget(IPC_PRIVATE, isize, IPC_CREAT | SHM_R | SHM_W);
          if (shmid_[i] == -1) {
              cout << me() << ": ";
              cout << "ShmMemoryGrp: shmget failed for "
                   << isize << " bytes: "
                   << strerror(errno) << endl;
              abort();
            }
        }

      rsize = size;
      void *ataddress = 0;
      for (i=0; rsize>0; i++,rsize-=isize) {
          isize = rsize;
          if (isize > SHMMAX) isize = SHMMAX;
          else if (isize < SHMMIN) isize = SHMMIN;
          if (debug_) {
              cout << me() << ": ";
              cout << "ShmMemoryGrp: attaching segment with "
                   << isize << " bytes at address " << (void*)ataddress
                   << endl;
            }
          attach_address_[i] = shmat(shmid_[i],(SHMTYPE)ataddress,0);
          if (debug_) {
              cout << me() << ": ";
              cout << "ShmMemoryGrp: got address "
                   << (void*)attach_address_[i]
                   << endl;
            }
          if (attach_address_[i] == 0
              || ataddress && attach_address_[i] != ataddress) {
              cout << "ShmMemoryGrp: shmat: problem attaching using address: "
                   << " " << (void*) ataddress
                   << ": got address: "
                   << (void*) attach_address_[i]
                   << endl;
              abort();
            }
          ataddress = (void*)((char*)(attach_address_[i]) + isize);
        }

      // attach the shared segments.
      memory_ = (void*) attach_address_[0];

      // initialize the pool
      pool_ = new(memory_) Pool(poolallocation);
      rangelock_ = new(pool_->allocate(sizeof(RangeLock))) RangeLock(pool_);
    }

  if (me() == 0) {
      lock_.initialize();
      lock_ = 1;
      char * stringrep = lock_.stringrep();
      int length = strlen(stringrep) + 1;
      msg_->bcast(&length, 1);
      msg_->bcast(stringrep, length);
#ifdef DEBUG
      cout << scprintf("%d: sent initialize string of \"%s\" (%d)\n",
                       me(), stringrep, length);
      cout.flush();
#endif // DEBUG
      delete[] stringrep;
      for (int i=0; i<n(); i++) {
          update_[i].initialize();
          char * stringrep = update_[i].stringrep();
          int length = strlen(stringrep) + 1;
          msg_->bcast(&length, 1);
          msg_->bcast(stringrep, length);
#ifdef DEBUG
          cout << scprintf("%d: sent initialize string of \"%s\" (%d) for %d\n",
                           me(), stringrep, length, i);
          cout.flush();
#endif // DEBUG
          delete[] stringrep;
        }
    }
  else {
      int length;
      msg_->bcast(&length, 1);
      char * stringrep = new char[length];
      msg_->bcast(stringrep, length);
#ifdef DEBUG
      cout << scprintf("%d: got initialize string of \"%s\" (%d)\n",
                       me(), stringrep, length);
      cout.flush();
#endif // DEBUG
      lock_.initialize(stringrep);
      delete[] stringrep;
      for (int i=0; i<n(); i++) {
          msg_->bcast(&length, 1);
          stringrep = new char[length];
          msg_->bcast(stringrep, length);
#ifdef DEBUG
          cout << scprintf("%d: got initialize string of \"%s\" (%d) for %d\n",
                           me(), stringrep, length, i);
          cout.flush();
#endif // DEBUG
          update_[i].initialize(stringrep);
          delete[] stringrep;
        }
    }

  msg_->bcast(shmid_, nregion_);
  msg_->raw_bcast((void*)attach_address_, nregion_*sizeof(void*));
  msg_->raw_bcast((void*)&memory_, sizeof(void*));
  msg_->raw_bcast((void*)&pool_, sizeof(void*));
  msg_->raw_bcast((void*)&rangelock_, sizeof(void*));

  if (debug_) {
      cout << scprintf("%d: memory_ = 0x%x shmid_[0] = %d\n",
                       me(), memory_, shmid_[0]);
    }
  
  if (me() != 0) {
      for (int i=0; i<nregion_; i++) {
          if (debug_) {
              cout << me() << ": ";
              cout << "ShmMemoryGrp: attaching segment at address "
                   << (void*)attach_address_[i] << endl;
            }
          // some versions of shmat return -1 for errors, 64 bit
          // compilers with 32 bit int's will complain.
          void *tmp = shmat(shmid_[i],(SHMTYPE)attach_address_[i],0);
          if (tmp != attach_address_[i]) {
              cout << me() << ": ";
              cout << "ShmMemoryGrp: shmat: problem attaching using address: "
                   << " " << (void*) attach_address_[i]
                   << ": got address: "
                   << (void*) tmp
                   << endl;
              abort();
            }
#ifdef DEBUG
          cerr << scprintf("%d: attached at 0x%x\n", me(), attach_address_[i]);
#endif // DEBUG
        }
    }

  data_ = (void *) &((char*)memory_)[poolallocation];
}

void
ShmMemoryGrp::cleanup()
{

  if (memory_) {
      for (int i=0; i<nregion_; i++) {
          shmdt((SHMTYPE)attach_address_[i]);
        }

      msg_->sync();

      if (me() == 0) {
          for (int i=0; i<nregion_; i++) {
              shmctl(shmid_[i],IPC_RMID,0);
            }
        }
      memory_ = 0;
    }

  delete[] update_;
  update_ = 0;

  nregion_ = 0;
  delete[] shmid_;
  delete[] attach_address_;
  shmid_ = 0;
  attach_address_ = 0;
}

ShmMemoryGrp::~ShmMemoryGrp()
{
  cleanup();

#ifdef DEBUG
  cout << scprintf("msg_->nreference() = %d\n", msg_->nreference());
  cout.flush();
#endif // DEBUG
  msg_ = 0;
}

void *
ShmMemoryGrp::obtain_readwrite(int offset, int size)
{
  if (offset + size > totalsize()) {
      cerr << scprintf("ShmMemoryGrp::obtain_readwrite: arg out of range\n");
      abort();
    }

  if (use_locks_) {
#if SIMPLE_LOCK
      obtain_lock();
#else // SIMPLE_LOCK
#ifdef DEBUG
      cout << scprintf("%d: clear_release_count\n", me());
      cout.flush();
#endif // DEBUG
      clear_release_count();
#ifdef DEBUG
      cout << scprintf("%d: obtain_lock\n", me());
      cout.flush();
#endif // DEBUG
      obtain_lock();
#ifdef DEBUG
      cout << scprintf("%d: checkeq\n", me());
      cout.flush();
#endif
      while (!rangelock_->checkeq(offset, offset + size, 0)) {
#ifdef DEBUG
          cout << scprintf("%d: range not zero -- waiting for release\n", me());
          cout.flush();
#endif // DEBUG
          //rangelock_->print();
          release_lock();
          wait_for_release();
          obtain_lock();
        }
      rangelock_->decrement(offset, offset + size);
#ifdef DEBUG
      cout << scprintf("%d: after obtain write\n", me());
      cout.flush();
      //rangelock_->print();
#endif // DEBUG
      release_lock();
#endif // SIMPLE_LOCK
    }

  return &((char*)data_)[offset];
}

void *
ShmMemoryGrp::obtain_readonly(int offset, int size)
{
  if (offset + size > totalsize()) {
      cerr << scprintf("ShmMemoryGrp::obtain_readonly: arg out of range\n");
      abort();
    }

  if (use_locks_) {
#if SIMPLE_LOCK
      obtain_lock();
#else // SIMPLE_LOCK
      clear_release_count();
      obtain_lock();
      while (!rangelock_->checkgr(offset, offset + size, -1)) {
#ifdef DEBUG
          cout << scprintf("%d: range is -1 -- waiting for release\n", me());
          cout.flush();
          //rangelock_->print();
#endif // DEBUG
          release_lock();
          wait_for_release();
          obtain_lock();
        }
      rangelock_->increment(offset, offset + size);
#ifdef DEBUG
      cout << scprintf("%d: after obtain read\n", me());
      cout.flush();
      //rangelock_->print();
#endif // DEBUG
      release_lock();
#endif // SIMPLE_LOCK
    }

  return &((char*)data_)[offset];
}

void
ShmMemoryGrp::release_read(void *data, int offset, int size)
{
  if (use_locks_) {
#if SIMPLE_LOCK
      release_lock();
#else // SIMPLE_LOCK
      obtain_lock();
      rangelock_->decrement(offset, offset + size);
      note_release();
#ifdef DEBUG
      cout << scprintf("%d: after release read\n", me());
      //rangelock_->print();
      cout.flush();
#endif // DEBUG
      release_lock();
#endif // SIMPLE_LOCK
    }
}

void
ShmMemoryGrp::release_write(void *data, int offset, int size)
{
  if (use_locks_) {
#if SIMPLE_LOCK
      release_lock();
#else // SIMPLE_LOCK
      obtain_lock();
      rangelock_->increment(offset, offset + size);
      note_release();
#ifdef DEBUG
      cout << scprintf("%d: after release write\n", me());
      //rangelock_->print();
      cout.flush();
#endif // DEBUG
      release_lock();
#endif // SIMPLE_LOCK
    }
}

void
ShmMemoryGrp::obtain_lock()
{
#ifdef DEBUG
  cout << scprintf("%d: lock val = %d\n", me(), lock_.val());
  cout.flush();
#endif // DEBUG
  lock_--;
#ifdef DEBUG
  cout << scprintf("%d: lock decremented\n", me());
  cout.flush();
#endif // DEBUG
}

void
ShmMemoryGrp::release_lock()
{
  lock_++;
#ifdef DEBUG
  cout << scprintf("%d: incremented lock\n", me());
  cout.flush();
#endif // DEBUG
}

void
ShmMemoryGrp::note_release()
{
  for (int i=0; i<n(); i++) {
      update_[i]++;
    }
#ifdef DEBUG
  cout << scprintf("%d: incremented release flags\n", me());
  cout.flush();
#endif // DEBUG
}

void
ShmMemoryGrp::wait_for_release()
{
#ifdef DEBUG
  cout << scprintf("%d: decrementing release flag\n", me());
  cout.flush();
#endif // DEBUG
  update_[me()]--;
#ifdef DEBUG
  cout << scprintf("%d: decremented release flag\n", me());
  cout.flush();
#endif // DEBUG
}

void
ShmMemoryGrp::clear_release_count()
{
  update_[me()] = 0;
#ifdef DEBUG
  cout << scprintf("%d: clearing release count\n", me());
  cout.flush();
#endif // DEBUG
}

void
ShmMemoryGrp::print(ostream &o)
{
  MemoryGrp::print(o);
  if (me() == 0) {
      if (use_locks_) {
          obtain_lock();
          //rangelock_->print(fp);
          release_lock();
        }
    }
}

void
ShmMemoryGrp::sum_reduction(double *data, int doffset, int dlength)
{
  int offset = doffset * sizeof(double);
  int length = dlength * sizeof(double);

  if (offset + length > totalsize()) {
      cerr << scprintf("MemoryGrp::sum_reduction: arg out of range\n");
      abort();
    }

  double *source_data = (double*) obtain_readwrite(offset, length);

  for (int i=0; i<dlength; i++) {
      source_data[i] += data[i];
    }

  release_write((void*) source_data, offset, length);
}

#endif

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// eval: (c-set-style "CLJ")
// End:
