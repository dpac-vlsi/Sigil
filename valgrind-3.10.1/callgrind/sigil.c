/*
   This file is part of Sigil, a tool for call graph profiling programs.
 
   Copyright (C) 2012, Siddharth Nilakantan, Drexel University
  
   This tool is derived from and contains code from Callgrind
   Copyright (C) 2002-2011, Josef Weidendorfer (Josef.Weidendorfer@gmx.de)
 
   This tool is also derived from and contains code from Cachegrind
   Copyright (C) 2002-2011 Nicholas Nethercote (njn@valgrind.org)
 
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
 
   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.
 
   The GNU General Public License is contained in the file COPYING.
*/

#include "config.h"
#include "global.h"
#include "sigil.h"
#include "valgrind.h"

#include "pub_tool_threadstate.h"
#include "pub_tool_libcfile.h"

/* GLOBAL VARIABLES ADDED TO PUT ALL DATA ACCESSES FOR EVERY ADDRESS IN A LINKED LIST - Sid */

#include "pub_tool_mallocfree.h"

/* FUNCTIONS ADDED TO PUT ALL DATA ACCESSES FOR EVERY ADDRESS IN A LINKED LIST - Sid */

drwglobvars* CLG_(thread_globvars)[MAX_THREADS];
ULong CLG_(pthread_thread_map)[MAX_THREADS];
/* pth_node* CLG_(pthread_create_calls_first) = 0; */
/* pth_node* CLG_(pthread_create_calls_last) = 0; */
/* pth_node* CLG_(thread_create_mapreq_first) = 0; */
/* pth_node* CLG_(thread_create_mapreq_last) = 0; */
pth_node *barrier_thread_sets_first = 0;
pth_node *barrier_thread_sets_last = 0;
pth_node *pthread_create_requests_first = 0;
pth_node *pthread_create_requests_last = 0;
pth_node *createtotidmap_requests_first = 0;
pth_node *createtotidmap_requests_last = 0;
Bool CLG_(in_pthread_api_call) = 0;
ULong CLG_(total_data_reads) = 0;
ULong CLG_(total_data_writes) = 0;
ULong CLG_(total_instrs) = 0;
ULong CLG_(total_flops) = 0;
ULong CLG_(total_iops) = 0;
ULong CLG_(num_eventlist_chunks) = 0;
ULong CLG_(num_eventaddrchunk_nodes) = 0;
ULong CLG_(tot_eventinfo_size) = 0;
int CLG_(num_threads) = 0;
drwevent* CLG_(drwevent_latest_event) = 0;
//drweventlist* CLG_(drw_eventlist_start) = 0;
//drweventlist* CLG_(drw_eventlist_end) = 0;
drwevent* CLG_(drw_eventlist_start) = 0;
drwevent* CLG_(drw_eventlist_end) = 0;
ULong CLG_(shared_reads_missed_pointer) = 0;
ULong CLG_(shared_reads_missed_dump) = 0;
int CLG_(num_eventdumps) = 0; //Keeps track of the number of times the events were dumped to file
int CLG_(max_search_depth) = 0;
int CLG_(min_search_depth) = 0;
ULong CLG_(tot_search_depth) = 0;
ULong CLG_(num_searches) = 0;
ULong CLG_(num_funcinsts) = 0;
ULong CLG_(num_callee_array_elements) = 0;
ULong CLG_(num_dependencelists) = 0;
ULong CLG_(num_funcinfos) = 0;
ULong CLG_(num_funccontexts) = 0;
ULong CLG_(num_addrchunks) = 0;
ULong CLG_(num_addrchunknodes) = 0;
ULong CLG_(num_sms) = 0;
ULong CLG_(num_written_bytes) = 0;
ULong CLG_(tot_memory_usage) = 0;
ULong CLG_(event_info_size) = 0;
Addr CLG_(drw_current_instr) = 0;
//HACK
ULong CLG_(debug_count_events) = 0;
int CLG_(L2_line_size) = 8; //64 Bytes according to cat /sys/devices/system/cpu/cpu0/cache/index2/coherency_line_size. This method obtained from: http://stackoverflow.com/questions/794632/programmatically-get-the-cache-line-size. I have used 8 because I don't expect that loads and stores will be bigger than that. This is also written in the shadow memory paper.
/*--------------------------------------------------------------------*/
/* Sparsely filled array;  index == syscallno */
SyscallInfo CLG_(syscall_info)[MAX_NUM_SYSCALLS];
// Remember some things about the syscall that we don't use until
// SK_(post_syscall) is called...

#define MAX_READ_MEMS  10

// Most distinct mem blocks read by a syscall is 5, I think;  10 should be safe
// This array is null-terminated.
ShW* CLG_(mem_reads_syscall)[MAX_READ_MEMS];
UInt CLG_(mem_reads_n_syscall) = 0;
Int  CLG_(current_syscall) = -1;
Int  CLG_(current_syscall_tid) = -1;
Addr CLG_(last_mem_write_addr) = INVALID_TEMPREG;
UInt CLG_(last_mem_write_len)  = INVALID_TEMPREG;
addrchunk* CLG_(syscall_addrchunks) = 0;
Int CLG_(syscall_addrchunk_idx) = 0;
drwglobvars* CLG_(syscall_globvars) = 0;

//Used when running in serial with no threads and events on
SysRes CLG_(drw_res);
int CLG_(drw_fd) = 0;
ULong CLG_(num_events) = 0;

void* CLG_(DSM) = 0;
void* CLG_(Dummy_SM) = 0;

//int CLG_(previous_bb_jmpindex) = 0;

/***NON-GLOBAL PROTOTYPE DECLARATIONS***/
static __inline__
//void insert_to_consumedlist (funcinst *current_funcinst_ptr, funcinst *consumed_fn, drwevent *consumed_event, Addr ea, Int datasize, ULong count_unique, ULong count_unique_event);
void insert_to_consumedlist (funcinst *current_funcinst_ptr, funcinst *consumed_fn, ULong consumed_event, Addr ea, Int datasize, ULong count_unique, ULong count_unique_event);
static __inline__
void insert_to_drweventlist( int type, int optype , funcinst *producer, funcinst *consumer, ULong count, ULong count_unique );
static int insert_to_evtaddrchunklist(evt_addrchunknode** chunkoriginal, Addr range_first, Addr range_last, ULong* refarg, int shared_read_tid, ULong shared_read_event_num);
static void handle_memory_overflow(void);
static void my_fwrite(Int fd, Char* buf, Int len);
static Char* filename = 0;
static void CLG_(pth_node_insert)(pth_node** pth_node_first, pth_node** pth_node_last, Addr addr, int tid);
static void CLG_(pth_node_outsert)(pth_node** pth_node_first, pth_node** pth_node_last, Addr *addr, ULong *threadmap);

static
void file_err(void)
{
   VG_(message)(Vg_UserMsg,
                "Error: can not open cache simulation output file `%s'\n",
                filename );
   VG_(exit)(1);
}

void CLG_(init_funcarray)()
{
  int i;
  SM* sm_temp;
  SM_event* sm_event_temp;
  funcinst* funcinsttemp;
  funcinfo* funcinfotemp;
  
  for(i = 0; i < MAX_THREADS; i++){
    CLG_(thread_globvars)[i] = 0;
    CLG_(pthread_thread_map)[i] = 0;
  }
  //DONE ADDITION BY SID
  
  //Initiallize syscall stuff here
  CLG_(syscall_addrchunks) = (addrchunk*) CLG_MALLOC("cl.init_funcarray.sys.1",sizeof(addrchunk) * SYSCALL_ADDRCHUNK_SIZE);
  CLG_(syscall_addrchunk_idx) = 0;
  CLG_(syscall_globvars) = (drwglobvars*) CLG_MALLOC("cl.init_funcarray.sys.2",sizeof(drwglobvars));
  funcinfotemp = CLG_(syscall_globvars)->funcinfo_first = (funcinfo*) CLG_MALLOC("cl.init_funcarray.sys.3",sizeof(funcinfo));
  CLG_(syscall_globvars)->funcinst_first  = (funcinst*) CLG_MALLOC("cl.init_funcarray.sys.4",sizeof(funcinst));
  funcinsttemp = CLG_(syscall_globvars)->funcinst_first;
  CLG_(syscall_globvars)->tid = STARTUP_FUNC;
  CLG_(syscall_globvars)->funcinfo_first->fn_number = 0;
  CLG_(syscall_globvars)->funcarray[0] = funcinfotemp;
  funcinsttemp->fn_number = 0; funcinsttemp->ip_comm_unique = funcinsttemp->ip_comm = funcinsttemp->instrs = funcinsttemp->flops = funcinsttemp->iops = 0;
  funcinsttemp->consumerlist = 0; funcinsttemp->consumedlist = 0; 
  funcinsttemp->funcinst_number = 0;
  funcinsttemp->tid = STARTUP_FUNC;
  funcinsttemp->function_info = CLG_(syscall_globvars)->funcinfo_first; //Store pointer to central info store of function
  funcinsttemp->num_callees = funcinsttemp->funcinst_number = 0;
  funcinsttemp->callees = 0;
  funcinsttemp->num_events = 0;

  //Initialize shadow memory here. Every Primary Map entry should point to the global empty secondary map 
  // and we should use memcpy to copy the emptiness to any new malloced SM first
  if(CLG_(clo).drw_events){
    CLG_(DSM) = (void*) CLG_MALLOC("cl.init_funcarray.sm.1",sizeof(SM_event));
    sm_event_temp = (SM_event*) CLG_(DSM);
  }
  else{
    CLG_(DSM) = (void*) CLG_MALLOC("cl.init_funcarray.sm.1",sizeof(SM));
    sm_temp = (SM*) CLG_(DSM);
  }
  CLG_(num_sms)++;
  if(CLG_(clo).drw_calcmem){
    CLG_(Dummy_SM) = (void*) CLG_MALLOC("cl.init_funcarray.sm.1",sizeof(SM));
  }
  for(i = 0; i < LWC_PM_SIZE; i++){
    if( i < LWC_SM_SIZE){
      if(CLG_(clo).drw_events){
	sm_event_temp->last_writer[i] = 0;
	sm_event_temp->last_writer_event[i] = 0;
	//sm_event_temp->last_writer_event_dumpnum[i] = 0;
	sm_event_temp->last_reader[i] = 0;
	sm_event_temp->last_reader_event[i] = 0;
      }
      else{
	sm_temp->last_writer[i] = 0;
	sm_temp->last_reader[i] = 0;
      }
    }
    PM[i] = CLG_(DSM);
  }

  if(CLG_(clo).drw_events){
    //VG_(printf)("Malloc of huge proportions: %lu %lu %d \n",(MAX_EVENTINFO_SIZE * 1024 * 1024 * 1024), sizeof(drwevent), (MAX_EVENTINFO_SIZE * 1024 * 1024 * 1024)/sizeof(drwevent));
    CLG_(event_info_size) = (MAX_EVENTINFO_SIZE * 1024 * 1024)/sizeof(drwevent);
    CLG_(drw_eventlist_start) = (drwevent*) CLG_MALLOC("cl.init_funcarray.drwevent.1",sizeof(drwevent) * CLG_(event_info_size) * 1024);
    if(!CLG_(drw_eventlist_start)) handle_memory_overflow();
    //Although drw_eventlist_end should point to the last valid entry + 1, since here we have no valid entries, it points location zero.
    CLG_(drw_eventlist_end) = CLG_(drw_eventlist_start);//CLG_(event_info_size) - 1 is the size of the array.
    CLG_(drwevent_latest_event) = CLG_(drw_eventlist_start);
  }
}

void CLG_(drwinit_thread)(int tid)
{
  drwglobvars *thread_globvar;
  int j;
  char drw_filename[50], buf[8192]; SysRes res;
  
  thread_globvar = (drwglobvars*) CLG_MALLOC("cl.drwinit_thread.gc.1",sizeof(drwglobvars));
  CLG_(thread_globvars)[tid] = thread_globvar;
  CLG_(num_threads)++;
  //VG_(printf)("Current tid: %d. Number of threads in the system: %d\n",tid, CLG_(num_threads));

  thread_globvar->funcinfo_first = 0;
  //funcinfo* CLG_(funcinfo_last);
  thread_globvar->funcinst_first = 0;
  thread_globvar->previous_funcinst = 0;
  thread_globvar->current_drwbbinfo.previous_bb_jmpindex = -1;
  thread_globvar->current_drwbbinfo.current_bb = 0;
  thread_globvar->current_drwbbinfo.previous_bb = 0;
  thread_globvar->current_drwbbinfo.previous_bbcc = 0;
  thread_globvar->current_drwbbinfo.expected_jmpkind = 0;
  thread_globvar->tid = tid;
  thread_globvar->in_pthread_api_call = 0;
  for (j = 0; j < NUM_FUNCTIONS; j++){
    thread_globvar->funcarray[j] = 0; //NULL might be better, but we'll just stick with 0 as GNU C Null pointer constant uses 0
    //CLG_(sortedfuncarray)[i] = 0;
  }

  if(CLG_(clo).drw_thread_or_func){
    CLG_(num_events) = 0;
    /***1. CREATE THE NECESSARY FILE***/
    if(CLG_(clo).drw_events){
      VG_(sprintf)(drw_filename, "sigil.events.out-%d",tid);
      res = VG_(open)(drw_filename, VKI_O_WRONLY|VKI_O_TRUNC, 0);
      
      if (sr_isError(res)) {
	res = VG_(open)(drw_filename, VKI_O_CREAT|VKI_O_WRONLY,
			VKI_S_IRUSR|VKI_S_IWUSR);
	if (sr_isError(res)) {
	  file_err(); // If can't open file, then create one and then open. If still erroring, Valgrind will die using this call - Sid
	}
      }
      
      CLG_(drw_res) = res;
      CLG_(drw_fd) = (Int) sr_Res(res);
      VG_(sprintf)(buf, "%s,%s,%s,%s,%s,%s,%s\nOR\n", "EVENT_NUM", "CONSUMER-FUNCTION NUMBER", "CONSUMER-FUNCINST NUMBER", "PRODUCER-FUNCTION NUMBER", "PRODUCER-FUNCINST NUMBER", "BYTES", "BYTES_UNIQ" );
      //VG_(write)(CLG_(drw_fd), (void*)buf, VG_(strlen)(buf));
      my_fwrite(CLG_(drw_fd), (void*)buf, VG_(strlen)(buf));
      VG_(sprintf)(buf, "%s,%s,%s,%s,%s,%s,%s\n\n", "EVENT_NUM", "FUNCTION NUMBER", "FUNC_INST NUM", "IOPS", "FLOPS", "LOC", "LOC_UNIQ" );
      //VG_(write)(CLG_(drw_fd), (void*)buf, VG_(strlen)(buf));
      my_fwrite(CLG_(drw_fd), (void*)buf, VG_(strlen)(buf));
      
      /*   VG_(sprintf)(buf, "%5s %15s %20s %20s %15s %15s %15s\n\n", "TID", "TYPE", "PRODUCER", "CONSUMER", "BYTES", "IOPS", "FLOPS"); */
      /*   //  my_fwrite(fd, (void*)buf, VG_(strlen)(buf)); // This will end up writing into the orignial callgrind.out file */
      /*   VG_(write)(funcinsttemp->fd, (void*)buf, VG_(strlen)(buf)); */
    }
    /***1. DONE CREATION***/
  }

}

static void free_recurse(funcinst *funcinstpointer){

  int i;
  if(funcinstpointer) {
    for(i = 0; i < funcinstpointer->num_callees; i++){
      free_recurse(funcinstpointer->callees[i]);
      VG_(free) (funcinstpointer->callees[i]);
    }
  }
}

void CLG_(free_funcarray)() //You didn't malloc it, so no need to do this!
{
  funcinfo *funcinfopointer, *funcinfopointer_next;
  funcinst *funcinstpointer;
  drwglobvars *thread_globvar;
  int i;
  for(i = 0; i < MAX_THREADS; i++){
    thread_globvar = CLG_(thread_globvars)[i];
    if (!thread_globvar) continue;
    funcinfopointer = thread_globvar->funcinfo_first;
    funcinstpointer = thread_globvar->funcinst_first;
    while(funcinfopointer){
      funcinfopointer_next = funcinfopointer->next;
      VG_(free) (funcinfopointer);
      funcinfopointer = funcinfopointer_next;
    }
    //Traverse the call tree that we have generated and free all of the data structures
    free_recurse(funcinstpointer);
    VG_(free) (funcinstpointer);
  }
}

static void print_debuginfo(void){
  //Print memory footprint information
  VG_(printf)("Print for %llu: \n\n",CLG_(total_instrs));
  VG_(printf)("Num SMs: %-10llu Num funcinsts: %-10llu Num Callee array elements(funcinst): %-10llu Num dependencelists(funcinst): %-10llu Num addrchunks(funcinst): %-10llu Num addrchunknodes(funcinst): %-10llu Num funcinfos: %-10llu Num funccontexts: %-10llu\n",CLG_(num_sms), CLG_(num_funcinsts), CLG_(num_callee_array_elements), CLG_(num_dependencelists), CLG_(num_addrchunks), CLG_(num_addrchunknodes), CLG_(num_funcinfos), CLG_(num_funccontexts));
  VG_(printf)("Size of PM: %-10lu = %lu(Mb) Size of SM: %-10lu = %lu(Kb) Size of SM_event: %-10lu  = %lu(Kb) Size of funcinst: %-10lu = %lu(Kb) Size of dependencelist: %-10lu = %lu(Kb) Size of addrchunk(funcinst): %-10lu Size of addrchunknode(funcinst): %-10lu Size of funcinfo: %-10lu = %lu(Kb)\n",sizeof(PM), sizeof(PM)/1024/1024, sizeof(SM), sizeof(SM)/1024, sizeof(SM_event), sizeof(SM_event)/1024, (sizeof(funcinst) + sizeof(funcinst*) * NUM_CALLEES), (sizeof(funcinst) + sizeof(funcinst*))/1024, sizeof(dependencelist), sizeof(dependencelist)/1024, sizeof(addrchunk), sizeof(addrchunknode), (sizeof(funcinfo) + sizeof(funccontext)*CLG_(clo).separate_callers), (sizeof(funcinfo) + sizeof(funccontext)*CLG_(clo).separate_callers)/1024);
  CLG_(tot_memory_usage) = sizeof(PM) + CLG_(num_funcinsts) * (sizeof(funcinst) + sizeof(funcinst*) * NUM_CALLEES) + CLG_(num_dependencelists) * sizeof(dependencelist) + CLG_(num_addrchunks) * sizeof(addrchunk) + CLG_(num_addrchunknodes)  * sizeof(addrchunknode) + CLG_(num_funcinfos) * (sizeof(funcinfo) + sizeof(funccontext)*CLG_(clo).separate_callers);
  if(CLG_(clo).drw_events)
    CLG_(tot_memory_usage) += CLG_(num_sms) * sizeof(SM_event);
  else
    CLG_(tot_memory_usage) += CLG_(num_sms) * sizeof(SM);
  VG_(printf)("Memory for SM(bytes): %-10llu = %llu(Mb) Memory for SM_event(bytes): %-10llu = %llu(Mb) Memory for funcinsts(bytes): %-10llu = %llu(Mb)\n", CLG_(num_sms) * sizeof(SM), CLG_(num_sms) * sizeof(SM)/1024/1024, CLG_(num_sms) * sizeof(SM_event), CLG_(num_sms) * sizeof(SM_event)/1024/1024, CLG_(num_funcinsts) * (sizeof(funcinst) + sizeof(funcinst*) * NUM_CALLEES), CLG_(num_funcinsts) * (sizeof(funcinst) + sizeof(funcinst*) * NUM_CALLEES)/1024/1024);
  VG_(printf)("  Memory for dependencelist(bytes): %-10llu = %llu(Mb) Memory for addrchunks(bytes): %-10llu = %llu(Mb) Memory for addrchunknodes(bytes): %-10llu = %llu(Mb) Memory for funcinfos(bytes): %-10llu = %llu(Mb)\n", CLG_(num_dependencelists) * sizeof(dependencelist), CLG_(num_dependencelists) * sizeof(dependencelist)/1024/1024, CLG_(num_addrchunks) * sizeof(addrchunk), CLG_(num_addrchunks) * sizeof(addrchunk)/1024/1024, CLG_(num_addrchunknodes)  * sizeof(addrchunknode), CLG_(num_addrchunknodes)  * sizeof(addrchunknode)/1024/1024, CLG_(num_funcinfos) * (sizeof(funcinfo) + sizeof(funccontext)*CLG_(clo).separate_callers), CLG_(num_funcinfos) * (sizeof(funcinfo) + sizeof(funccontext)*CLG_(clo).separate_callers)/1024/1024);
  VG_(printf)("Total Memory size(bytes): %-10llu = %llu(Mb)\n\n", CLG_(tot_memory_usage), CLG_(tot_memory_usage)/1024/1024);
}

static void handle_memory_overflow(void){
  VG_(printf)("Null pointer was returned for malloc! Dumping contents to file\n");
  CLG_(print_to_file)();
  tl_assert(0);
}

//The address range here must represent the range of addresses actually written into. Thus shared_unique will reflect rangelast - rangefirst + 1
//static int mark_event_shared(drwevent* producer_event, ULong shared_unique, Addr rangefirst, Addr rangelast){
static int mark_event_shared(ULong producer_event, ULong shared_unique, Addr rangefirst, Addr rangelast){
  ULong dummy;
  if(CLG_(clo).drw_thread_or_func)
    return 1;
  //if(DOLLAR_ON && CLG_(drwevent_latest_event)->producer->tid != STARTUP_FUNC)
  //insert_to_evtaddrchunklist(&producer_event->list, rangefirst, rangelast, &dummy, CLG_(drwevent_latest_event)->consumer->tid, CLG_(drwevent_latest_event)->event_num);
  //We need to write a pointer to the appropriate communication event as well, so that we can print it out
  //We need to add something in the latest communication event to say that this address range from the computation event corresponding to a certain tid and certain event number.
  //insert_to_evtaddrchunklist(&CLG_(drwevent_latest_event)->rlist, rangefirst, rangelast, &dummy, CLG_(drwevent_latest_event)->producer->tid, producer_event->event_num);
  insert_to_evtaddrchunklist(&CLG_(drwevent_latest_event)->rlist, rangefirst, rangelast, &dummy, CLG_(drwevent_latest_event)->producer->tid, producer_event);
  return 1;
}

static void* get_SM_for_reading(Addr ea)
{
  Addr temp = ea >> 38;
  if(temp){
    VG_(printf)("Address greater than 38-bit encountered!\n");
    tl_assert(0);
  }
  return PM[ea >> 16]; // use bits [31..16] of 'a'
}

/* dist_sm points to one of our three distinguished secondaries.  Make
   a copy of it so that we can write to it.
*/
static void* copy_for_writing ( void* dist_sm )
{
  void* new_sm;
  //SM* new_sm;
   //SM_event* new_sm_event;
   //SM_datareuse* new_sm_datareuse;
   if(CLG_(clo).drw_events){
     new_sm = CLG_MALLOC("cl.copy_for_writing.sm.1",sizeof(SM_event));
   }
   else{
     new_sm = CLG_MALLOC("cl.copy_for_writing.sm.1",sizeof(SM));
   }
   if (new_sm == NULL){
     VG_(printf)("Unable to allocate SM in copy_for_writing. Aborting!\n");
     tl_assert(0);
   }
   CLG_(num_sms)++;
   if(CLG_(clo).drw_events)
     VG_(memcpy)(new_sm, dist_sm, sizeof(SM_event));
   else
     VG_(memcpy)(new_sm, dist_sm, sizeof(SM));
   return new_sm;
}

/* static SM* get_SM_for_writing(Addr ea) */
/* { */
/*   Addr temp = ea >> 38; */
/*   if(temp){ */
/*     VG_(printf)("Address greater than 38-bit encountered!\n"); */
/*     tl_assert(0); */
/*   } */
/*   SM** sm_p = &PM[ea >> 16]; // bits [31..16] */
/*   if ((*sm_p) == CLG_(DSM)) */
/*     *sm_p = copy_for_writing(*sm_p); // copy-on-write */
/*   return *sm_p; */
/* } */

static void* get_SM_for_writing(Addr ea)
{
  Addr temp = ea >> 38;
  if(temp){
    VG_(printf)("Address greater than 38-bit encountered!\n");
    tl_assert(0);
  }
  void** sm_p = &PM[ea >> 16]; // bits [31..16]
  if ((*sm_p) == ((void*)CLG_(DSM)))
    *sm_p = (void*) copy_for_writing(*sm_p); // copy-on-write
  return *sm_p;
}

static void get_last_writer_event_singlesm(Addr ea, void* sm_temp, int datasize, funcinfo *current_funcinfo_ptr, funcinst *current_funcinst_ptr, int tid){
  SM_event *sm = (SM_event*) sm_temp;
  funcinst *temp, *candidate;
  //drwevent *temp_event, *candidate_event;
  ULong temp_event, candidate_event;
  //int temp_event_dumpnum, candidate_event_dumpnum;
  int i;
  ULong temp_rangefirst, temp_rangelast;
  ULong count_unique = 0, count_unique_event = 0;

/*   if(CLG_(num_eventdumps) > 0) */
/*     VG_(printf)("Moonj\n"); */

  //Unroll the loop once and use as a seed for the loop
  candidate_event = sm->last_writer_event[ea & 0xffff];
  //candidate_event_dumpnum = sm->last_writer_event_dumpnum[ea & 0xffff];
  candidate = sm->last_writer[ea & 0xffff];
  //If previous reader was not current funcinst ptr update the unique counts and change the current entity
  if(sm->last_reader[ea & 0xffff] != current_funcinst_ptr){
    count_unique++;
    sm->last_reader[ea & 0xffff] = current_funcinst_ptr;    
  }
  if(sm->last_reader_event[ea & 0xffff] != CLG_(drwevent_latest_event)){
    count_unique_event++;
    sm->last_reader_event[ea & 0xffff] = CLG_(drwevent_latest_event);
  }

/*   if(candidate_event){ */
/*     if(candidate_event->type) */
/*       candidate = candidate_event->consumer; */
/*     else */
/*       candidate = candidate_event->producer; */
/*   } */
  temp_rangefirst = ea;
  temp_rangelast = temp_rangefirst;
  for(i = 1; i < datasize; i++){
    temp_event = sm->last_writer_event[(ea + i) & 0xffff];
    //temp_event_dumpnum = sm->last_writer_event_dumpnum[(ea + i) & 0xffff];
    temp = sm->last_writer[(ea + i) & 0xffff];
    if(temp_event != candidate_event){ //Handles an extremely rare case because usually if the dumpnumbers are not equal it should mean that the events will not be the same either
      //process whatever you have so far and reset temp_rangefirst and temp_rangelast
      insert_to_consumedlist (current_funcinst_ptr, candidate, candidate_event, temp_rangefirst, temp_rangelast - temp_rangefirst + 1, count_unique, count_unique_event);
      candidate = temp;
      candidate_event = temp_event;
      //candidate_event_dumpnum = temp_event_dumpnum;
      temp_rangefirst = ea + i;
      temp_rangelast = temp_rangefirst;
      count_unique = count_unique_event = 0;
    }
    else
      temp_rangelast++;
    //The above code takes care of whether previous ranges and unique have to be saved and reset with a call to insert_to_consumedlist
    if(sm->last_reader[(ea + i) & 0xffff] != current_funcinst_ptr){
      count_unique++;
      sm->last_reader[(ea + i) & 0xffff] = current_funcinst_ptr;
    }
    if(sm->last_reader_event[(ea + i) & 0xffff] != CLG_(drwevent_latest_event)){
      count_unique_event++;
      sm->last_reader_event[(ea + i) & 0xffff] = CLG_(drwevent_latest_event);
    }
  }
  insert_to_consumedlist (current_funcinst_ptr, candidate, candidate_event, temp_rangefirst, temp_rangelast - temp_rangefirst + 1, count_unique, count_unique_event);
}

static void get_last_writer_event(Addr ea, int datasize, funcinfo *current_funcinfo_ptr, funcinst *current_funcinst_ptr, int tid){
  SM_event *sm = (SM_event*) get_SM_for_reading(ea);
  funcinst *temp, *candidate;
  //drwevent *temp_event, *candidate_event;
  ULong temp_event, candidate_event;
  //int temp_event_dumpnum, candidate_event_dumpnum;
  int i;
  ULong temp_rangefirst, temp_rangelast;
  ULong count_unique = 0, count_unique_event = 0;
  candidate_event = sm->last_writer_event[ea & 0xffff];
  //candidate_event_dumpnum = sm->last_writer_event_dumpnum[ea & 0xffff];
  candidate = sm->last_writer[ea & 0xffff];
  temp_rangefirst = ea;
  temp_rangelast = temp_rangefirst;
  if(sm->last_reader[ea & 0xffff] != current_funcinst_ptr){
    count_unique++;
    sm->last_reader[ea & 0xffff] = current_funcinst_ptr;    
  }
  if(sm->last_reader_event[ea & 0xffff] != CLG_(drwevent_latest_event)){
    count_unique_event++;
    sm->last_reader_event[ea & 0xffff] = CLG_(drwevent_latest_event);
  }
  for(i = 1; i < datasize; i++){
    sm = (SM_event*) get_SM_for_reading(ea + i);
    temp_event = sm->last_writer_event[(ea + i) & 0xffff];
    //temp_event_dumpnum = sm->last_writer_event_dumpnum[(ea + i) & 0xffff];
    temp = sm->last_writer[(ea + i) & 0xffff];
    if(temp_event != candidate_event){ //Handles an extremely rare case because usually if the dumpnumbers are not equal it should mean that the events will not be the same either
      //process whatever you have so far and reset temp_rangefirst and temp_rangelast
      insert_to_consumedlist (current_funcinst_ptr, candidate, candidate_event, temp_rangefirst, temp_rangelast - temp_rangefirst + 1, count_unique, count_unique_event);
      candidate = temp;
      candidate_event = temp_event;
      //candidate_event_dumpnum = temp_event_dumpnum;
      temp_rangefirst = ea + i;
      temp_rangelast = temp_rangefirst;
      count_unique = count_unique_event = 0;
    }
    else
      temp_rangelast++;
    //The above code takes care of whether previous ranges and unique have to be saved and reset with a call to insert_to_consumedlist
    if(sm->last_reader[(ea + i) & 0xffff] != current_funcinst_ptr){
      count_unique++;
      sm->last_reader[(ea + i) & 0xffff] = current_funcinst_ptr;
    }
    if(sm->last_reader_event[(ea + i) & 0xffff] != CLG_(drwevent_latest_event)){
      count_unique_event++;
      sm->last_reader_event[(ea + i) & 0xffff] = CLG_(drwevent_latest_event);
    }
  }
  insert_to_consumedlist (current_funcinst_ptr, candidate, candidate_event, temp_rangefirst, temp_rangelast - temp_rangefirst + 1, count_unique, count_unique_event);
}

static void get_last_writer(Addr ea, int datasize, funcinfo *current_funcinfo_ptr, funcinst *current_funcinst_ptr, int tid){
  SM *sm = (SM*) get_SM_for_reading(ea);
  funcinst *temp, *candidate;
  int i;
  ULong temp_rangefirst, temp_rangelast;
  ULong count_unique = 0, count_unique_event = 0;
  candidate = sm->last_writer[ea & 0xffff];
  temp_rangefirst = ea;
  temp_rangelast = temp_rangefirst;
  if(sm->last_reader[ea & 0xffff] != current_funcinst_ptr){
    count_unique++;
    sm->last_reader[ea & 0xffff] = current_funcinst_ptr;    
  }
  for(i = 1; i < datasize; i++){
    sm = (SM*) get_SM_for_reading(ea + i);
    temp = sm->last_writer[(ea + i) & 0xffff];
    if(temp != candidate){
      //process whatever you have so far and reset temp_rangefirst and temp_rangelast
      insert_to_consumedlist (current_funcinst_ptr, candidate, 0, temp_rangefirst, temp_rangelast - temp_rangefirst + 1, count_unique, count_unique_event);
      candidate = temp;
      temp_rangefirst = ea + i;
      temp_rangelast = temp_rangefirst;
      count_unique = count_unique_event = 0;
    }
    else
      temp_rangelast++;
    //The above code takes care of whether previous ranges and unique have to be saved and reset with a call to insert_to_consumedlist
    if(sm->last_reader[(ea + i) & 0xffff] != current_funcinst_ptr){
      count_unique++;
      sm->last_reader[(ea + i) & 0xffff] = current_funcinst_ptr;
    }
  }
  insert_to_consumedlist (current_funcinst_ptr, candidate, 0, temp_rangefirst, temp_rangelast - temp_rangefirst + 1, count_unique, count_unique_event);
}

static void get_last_writer_singlesm(Addr ea, void* sm_temp, int datasize, funcinfo *current_funcinfo_ptr, funcinst *current_funcinst_ptr, int tid){
  SM *sm = (SM*) sm_temp;
  funcinst *temp, *candidate;
  int i;
  ULong temp_rangefirst, temp_rangelast;
  ULong count_unique = 0, count_unique_event = 0;
  candidate = sm->last_writer[ea & 0xffff];
  temp_rangefirst = ea;
  temp_rangelast = temp_rangefirst;
  if(sm->last_reader[ea & 0xffff] != current_funcinst_ptr){
    count_unique++;
    sm->last_reader[ea & 0xffff] = current_funcinst_ptr;    
  }
  for(i = 1; i < datasize; i++){
    temp = sm->last_writer[(ea + i) & 0xffff];
    if(temp != candidate){
      //process whatever you have so far and reset temp_rangefirst and temp_rangelast
      insert_to_consumedlist (current_funcinst_ptr, candidate, 0, temp_rangefirst, temp_rangelast - temp_rangefirst + 1, count_unique, count_unique_event);
      candidate = temp;
      temp_rangefirst = ea + i;
      temp_rangelast = temp_rangefirst;
      count_unique = count_unique_event = 0;
    }
    else
      temp_rangelast++;
    //The above code takes care of whether previous ranges and unique have to be saved and reset with a call to insert_to_consumedlist
    if(sm->last_reader[(ea + i) & 0xffff] != current_funcinst_ptr){
      count_unique++;
      sm->last_reader[(ea + i) & 0xffff] = current_funcinst_ptr;
    }
  }
  insert_to_consumedlist (current_funcinst_ptr, candidate, 0, temp_rangefirst, temp_rangelast - temp_rangefirst + 1, count_unique, count_unique_event);
}

static void check_align_and_get_last_writer(Addr ea, int datasize, funcinfo *current_funcinfo_ptr, funcinst *current_funcinst_ptr, int tid){

/*   if(ea == 88473596) */
/*     VG_(printf)("Encountered the address\n"); */

//If we only want to calculate memory but not actually declare structures
  if(CLG_(clo).drw_calcmem){
    return;
  }

//  SM *sm_start = (SM*) get_SM_for_reading(ea);
//  SM *sm_end = (SM*) get_SM_for_reading(ea + datasize - 1);
  void *sm_start = get_SM_for_writing(ea);
  void *sm_end = get_SM_for_writing(ea + datasize - 1);
  if(!sm_start || !sm_end){
    if(CLG_(clo).drw_events)
      insert_to_consumedlist(current_funcinst_ptr, 0, 0, ea, datasize, datasize, 0);
    else
      insert_to_consumedlist(current_funcinst_ptr, 0, 0, ea, datasize, datasize, 0);
    return;
  }
  if(sm_start != sm_end){
    if(CLG_(clo).drw_events)
      get_last_writer_event(ea, datasize, current_funcinfo_ptr, current_funcinst_ptr, tid);      
    else
      get_last_writer(ea, datasize, current_funcinfo_ptr, current_funcinst_ptr, tid);
  }
  else{
    if(CLG_(clo).drw_events)
      get_last_writer_event_singlesm(ea, sm_start, datasize, current_funcinfo_ptr, current_funcinst_ptr, tid);
    else
      get_last_writer_singlesm(ea, sm_start, datasize, current_funcinfo_ptr, current_funcinst_ptr, tid);
  }
}

static void put_writer_event(Addr ea, int datasize, funcinst *current_funcinst_ptr, int tid){
  SM_event *sm;
  int i = 0;
  funcinst *current_previous_writer, *temp;
  ULong temp_rangefirst, temp_rangelast;
  //First traverse and remove the addresses in the current function's consumerlist
  //Then while overwriting, also determine the previous writer to a particular address and remove address chunks from its consumerlists
  //Do this efficiently in chunks
  sm = (SM_event*) get_SM_for_writing(ea);
  current_previous_writer = sm->last_writer[ea & 0xffff];
  temp_rangefirst = ea;
  temp_rangelast = temp_rangefirst;
  if(current_previous_writer != current_funcinst_ptr){
    sm->last_writer[ea & 0xffff] = current_funcinst_ptr;
    sm->last_writer_event[ea & 0xffff] = CLG_(drwevent_latest_event)->event_num;
    //sm->last_writer_event_dumpnum[ea & 0xffff] = CLG_(num_eventdumps);
    sm->last_reader[ea & 0xffff] = 0;
    sm->last_reader_event[ea & 0xffff] = 0;
  }
  for(i = 1; i < datasize; i++){
    sm = (SM_event*) get_SM_for_writing(ea + i);
    temp = sm->last_writer[(ea + i) & 0xffff];
    if(temp != current_previous_writer){
      //process whatever you have so far and reset temp_rangefirst and temp_rangelast
      //Make sure current_previous_writer is not zero and not local because we have already traversed and removed current_funcinst_ptr
      current_previous_writer = temp;
      temp_rangefirst = ea + i;
      temp_rangelast = temp_rangefirst;
    }
    else
      temp_rangelast++;
    //if(temp != current_funcinst_ptr){
    if(current_previous_writer != current_funcinst_ptr){
      sm->last_writer[(ea + i) & 0xffff] = current_funcinst_ptr;
      sm->last_writer_event[(ea + i) & 0xffff] = CLG_(drwevent_latest_event)->event_num;
      //sm->last_writer_event_dumpnum[(ea + i) & 0xffff] = CLG_(num_eventdumps);
      sm->last_reader[(ea + i) & 0xffff] = 0;
      sm->last_reader_event[(ea + i) & 0xffff] = 0;
    }
  }
}

static void put_writer_event_singlesm(Addr ea, void *sm_temp, int datasize, funcinst *current_funcinst_ptr, int tid){
  SM_event* sm = (SM_event*) sm_temp;
  int i = 0;
  funcinst *current_previous_writer, *temp;
  ULong temp_rangefirst, temp_rangelast;
  //First traverse and remove the addresses in the current function's consumerlist
  //Then while overwriting, also determine the previous writer to a particular address and remove address chunks from its consumerlists
  //Do this efficiently in chunks
  current_previous_writer = sm->last_writer[ea & 0xffff];
  temp_rangefirst = ea;
  temp_rangelast = temp_rangefirst;
  if(current_previous_writer != current_funcinst_ptr){
    sm->last_writer[ea & 0xffff] = current_funcinst_ptr;
    sm->last_writer_event[ea & 0xffff] = CLG_(drwevent_latest_event)->event_num;
    //sm->last_writer_event_dumpnum[ea & 0xffff] = CLG_(num_eventdumps);
    sm->last_reader[ea & 0xffff] = 0;
    sm->last_reader_event[ea & 0xffff] = 0;
  }
  for(i = 1; i < datasize; i++){
    temp = sm->last_writer[(ea + i) & 0xffff];
    if(temp != current_previous_writer){
      //process whatever you have so far and reset temp_rangefirst and temp_rangelast
      //Make sure current_previous_writer is not zero and not local because we have already traversed and removed current_funcinst_ptr
      current_previous_writer = temp;
      temp_rangefirst = ea + i;
      temp_rangelast = temp_rangefirst;
    }
    else
      temp_rangelast++;
    //if(temp != current_funcinst_ptr){
    if(current_previous_writer != current_funcinst_ptr){
      sm->last_writer[(ea + i) & 0xffff] = current_funcinst_ptr;
      sm->last_writer_event[(ea + i) & 0xffff] = CLG_(drwevent_latest_event)->event_num;
      //sm->last_writer_event_dumpnum[(ea + i) & 0xffff] = CLG_(num_eventdumps);
      sm->last_reader[(ea + i) & 0xffff] = 0;
      sm->last_reader_event[(ea + i) & 0xffff] = 0;
    }
  }
}

static void put_writer(Addr ea, int datasize, funcinst *current_funcinst_ptr, int tid){
  SM *sm;
  int i = 0;
  funcinst *current_previous_writer, *temp;
  ULong temp_rangefirst, temp_rangelast;
  //First traverse and remove the addresses in the current function's consumerlist
  //Then while overwriting, also determine the previous writer to a particular address and remove address chunks from its consumerlists
  //Do this efficiently in chunks
  sm = (SM*) get_SM_for_writing(ea);
  current_previous_writer = sm->last_writer[ea & 0xffff];
  temp_rangefirst = ea;
  temp_rangelast = temp_rangefirst;
  if(current_previous_writer != current_funcinst_ptr){
    sm->last_writer[ea & 0xffff] = current_funcinst_ptr;
    sm->last_reader[ea & 0xffff] = 0;
  }
  for(i = 1; i < datasize; i++){
    sm = (SM*) get_SM_for_writing(ea + i);
    temp = sm->last_writer[(ea + i) & 0xffff];
    if(temp != current_previous_writer){
      //process whatever you have so far and reset temp_rangefirst and temp_rangelast
      //Make sure current_previous_writer is not zero and not local because we have already traversed and removed current_funcinst_ptr
      current_previous_writer = temp;
      temp_rangefirst = ea + i;
      temp_rangelast = temp_rangefirst;
    }
    else
      temp_rangelast++;
    //if(temp != current_funcinst_ptr){
    if(current_previous_writer != current_funcinst_ptr){
      sm->last_writer[(ea + i) & 0xffff] = current_funcinst_ptr;
      sm->last_reader[(ea + i) & 0xffff] = 0;
    }
  }
}

static void put_writer_singlesm(Addr ea, void *sm_temp, int datasize, funcinst *current_funcinst_ptr, int tid){
  SM* sm = (SM*) sm_temp;
  int i = 0;
  funcinst *current_previous_writer, *temp;
  ULong temp_rangefirst, temp_rangelast;
  //First traverse and remove the addresses in the current function's consumerlist
  //Then while overwriting, also determine the previous writer to a particular address and remove address chunks from its consumerlists
  //Do this efficiently in chunks
  current_previous_writer = sm->last_writer[ea & 0xffff];
  temp_rangefirst = ea;
  temp_rangelast = temp_rangefirst;
  if(current_previous_writer != current_funcinst_ptr){
    sm->last_writer[ea & 0xffff] = current_funcinst_ptr;
    sm->last_reader[ea & 0xffff] = 0;
  }
  for(i = 1; i < datasize; i++){
    temp = sm->last_writer[(ea + i) & 0xffff];
    if(temp != current_previous_writer){
      //process whatever you have so far and reset temp_rangefirst and temp_rangelast
      //Make sure current_previous_writer is not zero and not local because we have already traversed and removed current_funcinst_ptr
      current_previous_writer = temp;
      temp_rangefirst = ea + i;
      temp_rangelast = temp_rangefirst;
    }
    else
      temp_rangelast++;
    //if(temp != current_funcinst_ptr){
    if(current_previous_writer != current_funcinst_ptr){
      sm->last_writer[(ea + i) & 0xffff] = current_funcinst_ptr;
      sm->last_reader[(ea + i) & 0xffff] = 0;
    }
  }
}

static void check_align_and_put_writer(Addr ea, int datasize, funcinst *current_funcinst_ptr, int tid){

/*   if(ea == 88473596) */
/*     VG_(printf)("Encountered the address\n"); */

//If we only want to calculate memory but not actually declare structures
  if(CLG_(clo).drw_calcmem){
    Addr temp = ea >> 38;
    if(temp){
      VG_(printf)("Address greater than 38-bit encountered!\n");
      tl_assert(0);
    }
    void **sm_temp, **sm_start = &PM[ea >> 16], **sm_end = &PM[(ea + datasize - 1) >> 16]; // bits [31..16]
    sm_temp = sm_start;
    sm_end++;
    do{
      if ((*sm_temp) == ((void*)CLG_(DSM))){
	*sm_temp = (void*) CLG_(Dummy_SM);
	CLG_(num_sms)++;
      }
      sm_temp++;
    } while(sm_temp != sm_end);
    return;
  }

  void *sm_start = get_SM_for_writing(ea);
  void *sm_end = get_SM_for_writing(ea + datasize - 1);
  //SM *sm_start = (SM*) get_SM_for_reading(ea);
  //SM *sm_end = (SM*) get_SM_for_reading(ea + datasize - 1);

  if(sm_start != sm_end){
    if(CLG_(clo).drw_events)
      put_writer_event(ea, datasize, current_funcinst_ptr, tid);
    else
      put_writer(ea, datasize, current_funcinst_ptr, tid);
  }
  else{
    if(CLG_(clo).drw_events)
      put_writer_event_singlesm(ea, sm_start, datasize, current_funcinst_ptr, tid);
    else
      put_writer_singlesm(ea, sm_start, datasize, current_funcinst_ptr, tid);
  }
}

static funcinst* create_funcinstlist (funcinst* caller, int fn_num, int tid){

  int k; funcinst *funcinsttemp;
  funcinfo *funcinfotemp;
  drwglobvars *thread_globvar = CLG_(thread_globvars)[tid];
  char drw_filename[50]; SysRes res;

  //We are guaranteed that a funcinfo exists for this by now.
  funcinfotemp = thread_globvar->funcarray[fn_num];
  funcinsttemp  = (funcinst*) CLG_MALLOC("cl.funcinst.gc.1",sizeof(funcinst));
  CLG_(num_funcinsts)++;
  if(funcinsttemp == 0)
    handle_memory_overflow();
  //1b. Similar to constructor, lets put all the initialization inst for funcinst right here
  funcinsttemp->fn_number = fn_num;
  funcinsttemp->tid = tid;
  funcinsttemp->function_info = funcinfotemp; //Store pointer to central info store of function
  funcinsttemp->ip_comm_unique = 0;
  //funcinsttemp->op_comm_unique = 0;
  funcinsttemp->ip_comm = 0;
  //funcinsttemp->op_comm = 0;
  funcinsttemp->num_calls = 0;
  funcinsttemp->instrs = 0;
  funcinsttemp->iops = 0;
  funcinsttemp->flops = 0;
  //  funcinsttemp->consumedlist = 0; //Again, declare when needed and initialize. We might want to add at least the self function
  funcinsttemp->consumedlist = (dependencelist*) CLG_MALLOC("cl.funcinst.gc.1",sizeof(dependencelist));
  CLG_(num_dependencelists)++;
  funcinsttemp->num_dependencelists = 1;
  funcinsttemp->num_addrchunks = 0;
  funcinsttemp->num_addrchunknodes = 0;
  funcinsttemp->consumedlist->prev = 0;
  funcinsttemp->consumedlist->next = 0;
  funcinsttemp->consumedlist->size = 0;
  funcinsttemp->consumerlist = 0;
  funcinsttemp->caller = caller; //If caller is defined, then go ahead and put that in. (Otherwise it might error out when doing the first ever function)
  funcinsttemp->callees = (funcinst**) CLG_MALLOC("cl.funcinst.gc.1",sizeof(funcinst*) * NUM_CALLEES);
  for(k = 0; k < NUM_CALLEES; k++)
    funcinsttemp->callees[k] = 0;
  funcinsttemp->num_callees = 0;
  CLG_(num_callee_array_elements) += NUM_CALLEES;
  if(funcinfotemp != 0){
    //    funcinsttemp->funcinst_list_next = funcinfotemp->funcinst_list;
    //    funcinsttemp->funcinst_list_prev = 0;

    //    if(funcinfotemp->funcinst_list != 0)
    //      funcinfotemp->funcinst_list->funcinst_list_prev = funcinsttemp;
    //    funcinfotemp->funcinst_list = funcinsttemp;

    funcinsttemp->funcinst_number = funcinfotemp->number_of_funcinsts;
    funcinfotemp->number_of_funcinsts++;
  }
  else{
    funcinsttemp->funcinst_number = -1; //This indicates it is not instrumented
    //    funcinsttemp->funcinst_list_next = 0;
    //    funcinsttemp->funcinst_list_prev = 0;
  }

  //These are not used as a central list is used instead
  //funcinsttemp->latest_event = 0;
  //funcinsttemp->drw_eventlist_start = 0;
  //funcinsttemp->drw_eventlist_end = 0;
  funcinsttemp->num_events = 0;
  //Only do this if we are running threads. If we run function and events, do not do this
  if(!CLG_(clo).drw_thread_or_func){
    /***1. CREATE THE NECESSARY FILE***/
    if(CLG_(clo).drw_events){
      VG_(sprintf)(drw_filename, "sigil.events.out-%d",tid);
      res = VG_(open)(drw_filename, VKI_O_WRONLY|VKI_O_TRUNC, 0);
      
      if (sr_isError(res)) {
	res = VG_(open)(drw_filename, VKI_O_CREAT|VKI_O_WRONLY,
			VKI_S_IRUSR|VKI_S_IWUSR);
	if (sr_isError(res)) {
	  file_err(); // If can't open file, then create one and then open. If still erroring, Valgrind will die using this call - Sid
	}
      }
      
      funcinsttemp->res = res;
      funcinsttemp->fd = (Int) sr_Res(res);
    }
    /***1. DONE CREATION***/
  }

  return funcinsttemp;
}

/***
 * Search the list to see if the context is present in the list
 * If it is present, returns 1, otherwise insert and return 0
 */
static int insert_to_funcinstlist (funcinst** funcinstoriginal, Context* func_cxt, int cxt_size, funcinst** refarg, int tid, Bool full_context){


  //funcinst *funcinstpointer = *funcinstoriginal;
  funcinst *funcinsttemp, *current_funcinst_ptr; //funcinstpointer is not used since we have an easy way of indexing the functioninsts via a fixed size array
  int i, j, ilimit, temp_cxt_num, entire_context_found = 1, foundflag = 0, contextcache_missflag = 0;
  drwglobvars *thread_globvar = CLG_(thread_globvars)[tid];
  funcinfo *current_funcinfo_ptr = thread_globvar->funcarray[func_cxt->fn[0]->number];

  //CLG_(current_cxt) = CLG_(current_state)->bbcc->bb->jmpkind; //Save the context, for quick checking when storeDRWcontext is invoked in the same context.
  if(thread_globvar->funcinst_first == 0){
    //cxt_size should be 1 in this case. Do an assert. //Found out later that this need not always necessarily be the case. This can be put back in when both storeDRWcontext and storeIcontext are "activated"
    if(func_cxt->size != 1)
      tl_assert(0);
    current_funcinst_ptr = create_funcinstlist(0, func_cxt->fn[0]->number, tid);
    *refarg = current_funcinst_ptr;
    thread_globvar->funcinst_first = current_funcinst_ptr;
    return 0;
  }

  //The cache should be used only when checking full contexts
  if(!full_context)
    contextcache_missflag = 1;

  //Here we need to check if the context specified is in full or not.
  if(func_cxt->fn[cxt_size - 1]->number == ((*funcinstoriginal)->fn_number)){ //Check if the top of the context is also the topmost function in the funcinst list. If its not, then the list is not full.
    current_funcinst_ptr = *funcinstoriginal;
    ilimit = cxt_size - 1;
    //ilimit = cxt_size - 1;
    //Before going over the entire context first do the topmost function.
    //if(func_cxt->fn[cxt_size - 1]->number == current_funcinst_ptr->fn_number) //This is the same as the other if, so must be true within that if
      ilimit--;
      //Check if cache has the appropriate first entry. We have a redundant check for cxt_size here as the sizes can be never be equal if the fn_numbers corresponding cxt_sizes are not equal
    if(full_context)
      if((cxt_size != current_funcinfo_ptr->context_size) || (current_funcinfo_ptr->context[cxt_size - 1].fn_number != func_cxt->fn[cxt_size - 1]->number)){//Cache context size does not match. Set the flag and initialize
	current_funcinfo_ptr->context[cxt_size - 1].fn_number = func_cxt->fn[cxt_size - 1]->number;
	current_funcinfo_ptr->context[cxt_size - 1].funcinst_ptr = current_funcinst_ptr;
	current_funcinfo_ptr->context_size = cxt_size;
	contextcache_missflag = 1;
      }
  }
  else{ //Not handled at the moment
    //func_num1 = func_cxt->fn[cxt_size - 1]->number;
    //func_num2 = thread_globvar->funcinst_first;
    //Traverse from funcnum2 down and find the first instance of func_num1
    //If found, set the current_funcinst_ptr to that guy and ilimit to cxt_size - 2.
    //If not found, set the current_funcinst_ptr to funcinst_first and ilimit to cxt_size - 2.
    VG_(printf)("\nPartial context not yet supported. Please increase the number of --separate-callers option when invoking callgrind\n");
    tl_assert(0);
  }

  for(i = ilimit; i >= 0; i--){ //Traverse the list of the context of the current function, knowing that it starts from the first ever function.
    temp_cxt_num = func_cxt->fn[i]->number;
    //First keep checking the cache and move forward with the appropriate funcinst pointers, if the miss flag has not been set
    if(full_context)
      if(!contextcache_missflag && current_funcinfo_ptr->context[i].fn_number == temp_cxt_num){
	current_funcinst_ptr = current_funcinfo_ptr->context[i].funcinst_ptr;
	continue;
      }
    //Cache miss, so mark the miss and re-cache the context in the function's data structure
    if(full_context){
      contextcache_missflag = 1;
      current_funcinfo_ptr->context[i].fn_number = temp_cxt_num;
    }
    for(j = 0; j < current_funcinst_ptr->num_callees; j++){ //Traverse all the callees of the currentfuncinst
      if(current_funcinst_ptr->callees[j]->fn_number == temp_cxt_num){ //If the callee matches what is seen in the context, then update the currentfuncinst_ptr
	current_funcinst_ptr = current_funcinst_ptr->callees[j];
	foundflag = 1;
	break;
      }
    }
    if(foundflag != 1){
      entire_context_found = 0;
      //Create a new funcinst and initialize it
      funcinsttemp = create_funcinstlist(current_funcinst_ptr, temp_cxt_num, tid);
      current_funcinst_ptr->callees[current_funcinst_ptr->num_callees++] = funcinsttemp;
      current_funcinst_ptr = funcinsttemp; //For the next iteration
    }
    //Since we are in this part of the code, we are re-caching, so update the cache
    if(full_context)
      current_funcinfo_ptr->context[i].funcinst_ptr = current_funcinst_ptr;
    foundflag = 0;
  }
  *refarg = current_funcinst_ptr;
  return entire_context_found;
}

/***
 * Search the list to see if the function is present in the list
 * If it is present, returns 1, otherwise insert and return 0
 */
static int insert_to_funcnodelist (funcinfo **funcinfooriginal, int func_num, funcinfo **refarg, fn_node* function, int tid){
  funcinfo *funcinfotemp; //funcinfopointer is not used since we have an easy way of indexing the functioninfos via a fixed size array
  int funcarrayindex = func_num, i = 0;
  drwglobvars *thread_globvar = CLG_(thread_globvars)[tid];

  if(thread_globvar->funcarray[funcarrayindex] != 0){ //If there is already an entry, just return positive
    *refarg = thread_globvar->funcarray[funcarrayindex];
    return 1;
  }

  funcinfotemp  = (funcinfo*) CLG_MALLOC("cl.funcinfo.gc.1",sizeof(funcinfo));
  CLG_(num_funcinfos)++;
  if(funcinfotemp == 0)
    handle_memory_overflow();
  //1b. Similar to constructor, lets put all the initialization info for funcinfo right here
  funcinfotemp->fn_number = funcarrayindex;
  funcinfotemp->function = function; //Store off the name indirectly
  //funcinfotemp->producedlist = 0; //Declare producer address chunks when an address is seen. That way the range can be allocated then as well.
  //funcinfotemp->consumerlist = 0; //Again, declare when needed and initialize. We might want to add at least the self function
  thread_globvar->funcarray[funcarrayindex] = funcinfotemp;

  funcinfotemp->next = *funcinfooriginal; //Insert new function at the head of the list. This will also work if the list is empty
  funcinfotemp->prev = 0;
  funcinfotemp->number_of_funcinsts = 0;
  //  funcinfotemp->funcinst_list = 0;
  funcinfotemp->context = (funccontext*) CLG_MALLOC("cl.funccontext.gc.1",sizeof(funccontext)*CLG_(clo).separate_callers);
  CLG_(num_funccontexts) += CLG_(clo).separate_callers;
  funcinfotemp->context_size = 0;
  for(i = 0; i < CLG_(clo).separate_callers; i++){
    funcinfotemp->context[i].fn_number = -1; //-1 indicates uninitialized context
    funcinfotemp->context[i].funcinst_ptr = 0;
  }
  //funcinfopointer->prev = funcinfotemp;
  if(*funcinfooriginal != 0)
    (*funcinfooriginal)->prev = funcinfotemp;
  *funcinfooriginal = funcinfotemp;
  *refarg = funcinfotemp;
  return 0;
}

static int remove_from_addrchunklist(addrchunk** chunkoriginal, Addr ea, Int datasize, ULong* refarg, int produced_list_flag, funcinst *vert_parent_fn) {

  addrchunk *chunk = *chunkoriginal; // chunk points to the first element of addrchunk array.
  addrchunknode *chunknodetemp, *chunknodecurr, *next_chunk;
  Addr range_first = ea, range_last = ea+datasize-1, curr_range_last;
  ULong return_count = datasize, firsthash, lasthash, chunk_arr_idx, arr_lasthash, firsthash_new;
  int partially_found =0, return_value2=0;


  // If the addrchunk array pointer is '0', indicating that there are no addresses inserted into the given addrchunk array,
  // allocate the array and point it to the chunk. Every element of this new array is initialized with first hash, last hash and an original
  // pointer that points to nothing
  if(chunk == 0) {
    *refarg = datasize;
    return 0;
  }
  
  /*Total addrchunk array elements = ADDRCHUNK_ARRAY_SIZE
    Each array element has a hash range of HASH_SIZE.
    i.e., hash range of element i is i*HASH_SIZE to ((i+1)*HASH_SIZE)-1
    Therefore, total hash range covered is 0 to HASH_SIZE * ADDRCHUNK_ARRAY_SIZE
    
    In the below code, hash of the given address range is calculated to
    determine which element of the array should be searched. */
  
  firsthash = range_first % (HASH_SIZE * ADDRCHUNK_ARRAY_SIZE);
  lasthash = range_last % (HASH_SIZE * ADDRCHUNK_ARRAY_SIZE);
  firsthash_new = firsthash;
  
  // do while iterates over addrchunk array elements till hash of the last range passed
  // falls into an element's hash range.
  do {
    //index of the array element that needs to be searched for the given address
    //range is calculated below.
    firsthash = firsthash_new;
    chunk_arr_idx = firsthash/HASH_SIZE;
    arr_lasthash=(chunk+chunk_arr_idx)->last;
    
    //hash of the last range is calculated and compared against the current array
    //element's last hash to determine if the given address range should be split
    if(lasthash > arr_lasthash || lasthash < firsthash) {
      curr_range_last = range_first + (arr_lasthash-firsthash);
      firsthash_new = (arr_lasthash+1)% (HASH_SIZE * ADDRCHUNK_ARRAY_SIZE);
    } else curr_range_last = range_last;
    
    if((chunk+chunk_arr_idx)->original != 0) {
      chunknodecurr = (chunk+chunk_arr_idx)->original;
      
      //while iterates over addrchunknodes present in the given array element until it finds
      // the desired address or it reaches end of the list.
      while(1) {
	// if the address range being removed is less than current node's address, then search terminates,
	//as the node's are in increasing order of addresses
	if(curr_range_last < (chunknodecurr->rangefirst))  break;
	
	// if the current node falls within the address range being searched , then current node is removed
	// and search continues with the next addrchunknode
	else if((range_first <= (chunknodecurr->rangefirst)) && (curr_range_last >= (chunknodecurr->rangelast))) {
	  return_value2 =1;
	  return_count -= chunknodecurr->count_unique;
	  
	  if(chunknodecurr->prev!=0) chunknodecurr->prev->next = chunknodecurr->next;
	  else (chunk+chunk_arr_idx)->original =chunknodecurr->next;
	  
	  if(chunknodecurr->next!=0) chunknodecurr->next->prev = chunknodecurr->prev;
	  next_chunk = chunknodecurr ->next;
	  VG_(free)(chunknodecurr);
	  CLG_(num_addrchunknodes)--;
	  vert_parent_fn->num_addrchunknodes--;
	  chunknodecurr = next_chunk;
	}
	
	// if the address range being searched is after current node, then search continues with the next node
	else if(range_first > (chunknodecurr->rangelast)) chunknodecurr = chunknodecurr->next;
	
	// if the address being searched falls within current node's address range, then the portion of the
	// address being searched is removed and the current nodes address range is adjusted appropriately.
	
	else if((range_first >= chunknodecurr->rangefirst) && (curr_range_last <= chunknodecurr->rangelast)) {
	  chunknodecurr ->count -= curr_range_last - range_first + 1;//datasize;
	    chunknodecurr->count_unique -= curr_range_last - range_first + 1;//datasize;
	  return_count -= (curr_range_last - range_first +1);
	  return_value2=1;
	  if((range_first > chunknodecurr->rangefirst) && (curr_range_last < chunknodecurr->rangelast)){
	    chunknodetemp = (addrchunknode*) CLG_MALLOC("cl.addrchunknode.gc.1",sizeof(addrchunknode));
	    CLG_(num_addrchunknodes)++;
	    vert_parent_fn->num_addrchunknodes++;
	    if(chunknodecurr->prev == 0) 	{ (chunk+chunk_arr_idx)->original = chunknodetemp;
	    }
	    else   chunknodecurr->prev->next = chunknodetemp;
	    
	    chunknodetemp->prev = chunknodecurr->prev;
	    chunknodetemp->next = chunknodecurr;
	    chunknodecurr->prev = chunknodetemp;
	    
	    chunknodetemp->rangefirst = chunknodecurr->rangefirst;
	    chunknodetemp->rangelast = range_first-1;
	    chunknodetemp->count = chunknodetemp->rangelast - chunknodetemp->rangefirst +1;
	    chunknodetemp->count_unique = chunknodetemp->rangelast - chunknodetemp->rangefirst +1;
	    chunknodecurr->rangefirst = curr_range_last +1;
	    chunknodecurr->count -= chunknodetemp->count;
	    chunknodecurr->count_unique -= chunknodetemp->count_unique;
	    
	  }
	  else if(range_first == (chunknodecurr->rangefirst)) {
	    chunknodecurr->rangefirst = curr_range_last+1;
	  } else if(curr_range_last == (chunknodecurr->rangelast)) {
	    chunknodecurr->rangelast = range_first-1;
	  }
	  chunknodecurr=chunknodecurr->next;
	}
	
	// Following two cases handle the cases where address range being searched either starts within current node
	// or ends within current node. return_count and address_array are appropriately initialized, and then search
	// continues with the next node.
	else {
	  if(range_first >= (chunknodecurr->rangefirst)) {
	    return_count -= ((chunknodecurr -> rangelast)- range_first + 1);
	    chunknodecurr->count -= (chunknodecurr->rangelast - range_first +1);
	    chunknodecurr->count_unique -= (chunknodecurr->rangelast - range_first +1);
	    chunknodecurr -> rangelast = range_first-1;
	    return_value2 =1;
	  }
	  if(curr_range_last <= (chunknodecurr->rangelast)) {
	    return_count -= (curr_range_last - chunknodecurr->rangefirst + 1);
	    chunknodecurr->count -= (curr_range_last - chunknodecurr->rangefirst +1);
	    chunknodecurr->count_unique -= (curr_range_last - chunknodecurr->rangefirst +1);
	    chunknodecurr -> rangefirst = curr_range_last+1;
	    return_value2=1;
	  }
	  chunknodecurr = chunknodecurr->next;
	}
	//break while loop if end of the addrchunknode list is reached
	if(chunknodecurr ==0) break;
      } // end of while (1)
    } // end of else corresponding to original ==0 check
      //Before moving to next array elements, range first is initialized to next element's range first
    range_first = curr_range_last+1;
    //while loop checks if the search should stop with the current array element, or it should continue.
    // if the hash of the last range of the address being searched lies within current element's hash range,
    // search stops. Second check takes care of the scenario where the address range being searched wraps around
    // addrchunk array
  } while (lasthash > arr_lasthash || lasthash < firsthash);
  //number of addresses not found are returned
  *refarg = return_count;
  // if the address range is fully found, return 2
  if (return_count ==0) partially_found =2;
  // if the address range is partially found return 1, else return 0.
  else if(return_value2 ==1) partially_found = 1;
  return partially_found;
}

/***
 * Given a funcinst, this function will check if any part of the address range passed in, is produced by the funcinst
 * Returns void
 */
static __inline__
dependencelist_elemt* insert_to_dependencelist(funcinst *current_funcinst_ptr, funcinst *consumed_fn){
  dependencelist *temp_list; int temp_funcinst_number, temp_fn_number, temp_tid;
  int i, found_in_list_flag = 0; dependencelist_elemt *return_list_elemt;
  funcinst *funcinsttemp = consumed_fn;

  if(consumed_fn){
    temp_funcinst_number = consumed_fn->funcinst_number;
    temp_fn_number = consumed_fn->fn_number;
    temp_tid = consumed_fn->tid;
  }
  else{
    temp_funcinst_number = NO_PROD;
    temp_fn_number = NO_PROD;
    temp_tid = NO_PROD;
  }
  //1. First search the list to see if you find this dependence.
  temp_list = current_funcinst_ptr->consumedlist;
  while(1){
    for(i = 0; i < temp_list->size; i++){
      if((temp_funcinst_number == temp_list->list_chunk[i].funcinst_number) &&
	 (temp_fn_number == temp_list->list_chunk[i].fn_number) && 
	 (temp_tid == temp_list->list_chunk[i].tid)){
	//2. If found, you can increment a count?
	return_list_elemt = &temp_list->list_chunk[i];
	found_in_list_flag = 1;
	break;
      }
    }
    if(!temp_list->next || found_in_list_flag) break;
    else temp_list = temp_list->next;  
  }
  
  //3. If not found, add
  if(!found_in_list_flag){
    if(temp_list->size == DEPENDENCE_LIST_CHUNK_SIZE){
      temp_list->next = (dependencelist*) CLG_MALLOC("cl.funcinst.gc.1",sizeof(dependencelist));
      CLG_(num_dependencelists)++;
      current_funcinst_ptr->num_dependencelists++;
      temp_list->next->next = 0;
      temp_list->next->size = 0;
      temp_list->next->prev = temp_list;
      temp_list = temp_list->next;
    }
    return_list_elemt = &temp_list->list_chunk[temp_list->size];
    return_list_elemt->fn_number = temp_fn_number;
    return_list_elemt->funcinst_number = temp_funcinst_number;
    return_list_elemt->tid = temp_tid;
    return_list_elemt->vert_parent_fn = current_funcinst_ptr;
    return_list_elemt->consumed_fn = consumed_fn;
    return_list_elemt->consumedlist = 0;
    return_list_elemt->count = 0;
    return_list_elemt->count_unique = 0;
    if(consumed_fn == 0){
      return_list_elemt->next_horiz = 0;
      return_list_elemt->prev_horiz = 0;
    }
    else{
      funcinsttemp = consumed_fn;
      return_list_elemt->next_horiz = funcinsttemp->consumerlist;
      return_list_elemt->prev_horiz = 0;
      if(funcinsttemp->consumerlist != 0)
	funcinsttemp->consumerlist->prev_horiz = return_list_elemt;
      funcinsttemp->consumerlist = return_list_elemt;
    }
    
    temp_list->size++;
  }
  return return_list_elemt;
}

/***
 * Given a funcinst, this function will check if any part of the address range passed in, is produced by the funcinst
 * Returns void
 */
static __inline__
void insert_to_consumedlist (funcinst *current_funcinst_ptr, funcinst *consumed_fn, ULong consumed_event, Addr ea, Int datasize, ULong count_unique, ULong count_unique_event){
  dependencelist_elemt *consumedfunc;
  ULong dummy; //int tid;

  //found! Search the current function's consumedlist for the name/number of the function, create if necessary and add the address to that list
  //insert_to_consumedfunclist(&current_funcinst_ptr->consumedlist, consumed_fn, current_funcinst_ptr, &consumedfunc);
  consumedfunc = insert_to_dependencelist(current_funcinst_ptr, consumed_fn);

  //Increment count in consumedfunclist and funcinfo
  consumedfunc->count += datasize;
  consumedfunc->count_unique += count_unique;
  if(consumed_fn == current_funcinst_ptr){
    insert_to_drweventlist(0, 1, consumed_fn, 0, datasize, count_unique);
    //To print out the range for local reads as well.
    insert_to_evtaddrchunklist(&CLG_(drwevent_latest_event)->rlist, ea, ea+datasize-1, &dummy, CLG_(drwevent_latest_event)->producer->tid, CLG_(drwevent_latest_event)->event_num);
    //SELF means consumed_fn == funcinstpointer. NO_PROD or NO_CONS means that consumed_fn == 0
  }
  if((consumed_fn != current_funcinst_ptr) && (consumed_fn != 0)){
    current_funcinst_ptr->ip_comm += datasize;
    current_funcinst_ptr->ip_comm_unique += count_unique;

    if(CLG_(clo).drw_events){
      insert_to_drweventlist(1, 0, consumed_fn, current_funcinst_ptr, datasize, count_unique);
/*       if(consumed_event) */
/* 	mark_event_shared(consumed_event, datasize, ea, ea + datasize - 1); //This should be changed to consumed_fn->tid, fn_number and funcinst_number so that we can also capture events for functions */
/*       else{ */
/* 	//LOG THAT WE HAVE MISSED A SHARED READ */
/* 	CLG_(shared_reads_missed_dump)++; */
/* 	VG_(printf)("Shared reads missed due to dumping of events\n"); */
/*       } */
      if(!consumed_event && CLG_(drwevent_latest_event)->producer->tid != STARTUP_FUNC){
	VG_(printf)("consumed_event is zero and the consumer is non-local!\n");
      }
      mark_event_shared(consumed_event, datasize, ea, ea + datasize - 1); //This should be changed to consumed_fn->tid, fn_number and funcinst_number so that we can also capture events for functions
    }
  }
  //else
  //if(!consumed_fn)
  //VG_(printf)("NO PRODUCER!\n");
}

//This function will be called everytime it is detected that control has moved to a new function. It will marshall
//all the dependences seen in the function that just completed. If there are fancy jumps in between function calls,
//it is still counted as a new call. The function that just completed (fnA) maintains a list of dependencies (functions it read from)
//that it encountered when executing during that call. This is checked against the global history of functions to see
//if it read from any functions that were called after some prior call to fnA. fnA's structure can be obtained from
//thread_globvar->previous_funcinst as it just completed and we have entered some new function

static __inline__
void handle_callreturn(funcinst *current_funcinst_ptr, funcinst *previous_funcinst_ptr, int call_return, int tid){ //call_return = 0 indicates a call, 1 indicates a return, 2 indicates a call and a return or a jump. Basically 2 is passed into this function when we don't know what could have happened exactly

  //1. Take fnA and compare global history with the dependency list of the call that just completed (of fnA)
  //If previous call to A exists in the array, and still points to the old call, then we can start comparing from there with all the elements of the dependency list for the call that just completed
  //If the call that just completed to the previous funcinst was the first, then just check the whole list

  if(!call_return || call_return == 2){
    current_funcinst_ptr->num_calls++;
  }
}

static __inline__ 
fn_node* create_fake_fn_node(int tid)
{
  HChar fnname[512];
  fn_node* fn = (fn_node*) CLG_MALLOC("cl.drw.nfnnd.1",
				      sizeof(fn_node));
  VG_(sprintf)(fnname, "Thread-%d", tid);
  fn->name = VG_(strdup)("cl.drw.nfnnd.2", fnname);
  
  fn->number   = 0;
  fn->last_cxt = 0;
  fn->pure_cxt = 0;
  fn->file     = 0;
  fn->next     = 0;
  
  fn->dump_before  = False;
  fn->dump_after   = False;
  fn->zero_before  = False;
  fn->toggle_collect = False;
  fn->skip         = False;
  fn->pop_on_jump  = CLG_(clo).pop_on_jump;
  fn->is_malloc    = False;
  fn->is_realloc   = False;
  fn->is_free      = False;
  
  fn->group        = 0;
  fn->separate_callers    = CLG_(clo).separate_callers;
  fn->separate_recursions = CLG_(clo).separate_recursions;
  
#if CLG_ENABLE_DEBUG
  fn->verbosity    = -1;
#endif
  
  return fn;
}

static Context* create_fake_cxt(fn_node** fn)
{
    Context* cxt;
    CLG_ASSERT(fn);

    cxt = (Context*) CLG_MALLOC("cl.drw.nc.1", sizeof(Context));

    cxt->fn[0] = *fn;
    cxt->size        = 1;
    cxt->base_number = CLG_(stat).context_counter;
    cxt->hash        = 0;
    cxt->next = 0;
    return cxt;
}

void CLG_(storeIDRWcontext) (InstrInfo* inode, int datasize, Addr ea, Bool WR, int opsflag) 
//opsflag to indicate what we are recording with this call. opsflag = 0 for instruction/nothing, 1 for mem read, 2 for mem write, and 3 for iop and 4 for flop, 5 for new_mem_startup, 6 for Pthread API entry event
//opsflag 6 - mutex_lock, 7 - mutex_unlock, 8 - create, 9 - join, 10 - barrier_wait, 11 - cond_wait, 12 - cond_signal, 13 - spin_lock, 14 - spin_unlock. This will go to optype in insert_to_drweventlist
{
  int funcarrayindex = 0, wasreturn_flag = 0, threadarrayindex = 0;
  funcinst *current_funcinst_ptr = 0;
  funcinfo *current_funcinfo_ptr = 0;
  funcinst *temp_caller_ptr, *temp_caller_ptr_2;
  drwglobvars *thread_globvar;
  fn_node* function;
  Context* cxt;
  int temp_num_splits = 1, temp_splits_rem = 0, temp_split_datasize = datasize, i = 0, temp_ea = ea; ULong dummy;
  //  int search_depth = 0;

  //Debug. Check if its break
/*   if(CLG_(current_state).cxt->fn[0]->number == 5 && opsflag == 1){ */
/*     VG_(printf)("=  storeidrwcontext brk read: %x..%x, %d, function: %s\n", ea, ea+datasize,datasize, CLG_(current_state).cxt->fn[0]->name); */
/*   } */
/*   else if(CLG_(current_state).cxt->fn[0]->number == 5 && opsflag == 2){ */
/*     VG_(printf)("=  storeidrwcontext brk write: %x..%x, %d, function: %s\n", ea, ea+datasize,datasize, CLG_(current_state).cxt->fn[0]->name); */
/*   } */

  //0. Figure out which thread first and pull in its last state if necessary
  threadarrayindex = CLG_(current_tid); //When setup_bbcc calls switch_thread this variable will be set to the correct current tid
  thread_globvar = CLG_(thread_globvars)[threadarrayindex];
  //As setup_bbcc has already made sure that whatever is accessed above is valid for the current thread we go ahead here

  //if(threadarrayindex == 3)
  //VG_(printf)("Thread 3!\n");

  //MUTEX DEBUGGING PLUS FUNCTIONALITY
  //char pth_name_hack[20] = "pthread_mutex_lock\0";
  char pth_name_hack1[20] = "mmap\0";
  char pth_name_hack2[20] = "malloc\0";
  if(CLG_(current_state).cxt){
    if(!VG_(strcmp)(pth_name_hack1,CLG_(current_state).cxt->fn[0]->name)){
      //VG_(printf)("Found pthread_mutex_lock function!\n");
      return;
    }
    else if(!VG_(strcmp)(pth_name_hack2,CLG_(current_state).cxt->fn[0]->name)){
      return;
    }
  }
  if(CLG_(clo).drw_intercepts){
    if((opsflag == 1 || opsflag == 2) && thread_globvar->in_pthread_api_call) //When mutex is being locked or unlocked disregard all memory events
      return;
  }

//ULTRA HACK DEBUGTRACE TO COMPARE WITH GEM5. Remove after tracing those problems out, unless this information proves useful
  if(CLG_(clo).drw_debugtrace){
    if(opsflag == 5)
      return;
    else if(!opsflag){
      CLG_(drw_current_instr) = ea;
      CLG_(total_instrs) += datasize;
    }
    else if(opsflag == 1){
      VG_(printf)("Thread %d PC: %lx Read: %lx.. %lx, %d func: %s\n", CLG_(current_tid), CLG_(drw_current_instr), ea, ea+datasize, datasize, CLG_(current_state).cxt->fn[0]->name);
    }
    else if(opsflag == 2){
      VG_(printf)("Thread %d PC: %lx Write: %lx.. %lx, %d func: %s\n", CLG_(current_tid), CLG_(drw_current_instr), ea, ea+datasize, datasize, CLG_(current_state).cxt->fn[0]->name);
    }
    else if (opsflag > 5){ //Pthread event
      switch(opsflag){
      case 0:
	break;
      case 6://mutex_lock
	thread_globvar->in_pthread_api_call = 1;
	VG_(printf)("Thread %d Pthread mutex lock: %lx\n", CLG_(current_tid), ea);
	break;
      case 7://mutex_unlock
	thread_globvar->in_pthread_api_call = 1;
	VG_(printf)("Thread %d Pthread mutex unlock: %lx\n", CLG_(current_tid), ea);
	break;
      case 8://pthread_create
	break;
      case 9://pthread_join
	break;
      case 10: //pthread_barrier_wait
	thread_globvar->in_pthread_api_call = 1;
	VG_(printf)("Thread %d Pthread barrier wait: %lx\n", CLG_(current_tid), ea);
	break;
      case 11: //pthread_cond_wait
	thread_globvar->in_pthread_api_call = 1;
	VG_(printf)("Thread %d Pthread cond wait: %lx\n", CLG_(current_tid), ea);
	break;
      case 12: //pthread_cond_signal
	thread_globvar->in_pthread_api_call = 1;
	VG_(printf)("Thread %d Pthread cond signal: %lx\n", CLG_(current_tid), ea);
	break;
      case 13: //pthread_spin_lock
	thread_globvar->in_pthread_api_call = 1;
	VG_(printf)("Thread %d Pthread spin lock: %lx\n", CLG_(current_tid), ea);
	break;
      case 14: //pthread_spin_unlock
	thread_globvar->in_pthread_api_call = 1;
	VG_(printf)("Thread %d Pthread spin unlock: %lx\n", CLG_(current_tid), ea);
	break;
      default:
	VG_(printf)("Incorrect optype specified when inserting pthread to drweventlist");
	tl_assert(0);
      }
    }
    return;
  }
//DONE WITH ULTRA HACK

  //-1. if opsflag is 5 then do special stuff.
  if(opsflag == 5){
    if(CLG_(clo).drw_debugtrace)
      return;
    //Create context and do things in storeIDRWcontext. This is for the new_mem_startup
    CLG_(syscall_globvars)->funcinfo_first->function = create_fake_fn_node(STARTUP_FUNC); //Store off the name indirectly
    cxt = create_fake_cxt(&function);
    thread_globvar = CLG_(syscall_globvars); //Hack to ensure variables that are useless in this use case still have legal values.
    //Treat as write
    //insert_to_drweventlist(0, 2, current_funcinst_ptr, 0, datasize, 0);
    check_align_and_put_writer(ea, datasize, CLG_(syscall_globvars)->funcinst_first, STARTUP_FUNC);
    //CLG_(put_in_last_write_cache)(datasize, ea, current_funcinst_ptr, threadarrayindex);
    CLG_(total_data_writes) += datasize; // 1;
    return;
  }

  if(!CLG_(clo).drw_thread_or_func){   //If we are only instrumenting threads (and not worried about functions), then do the needful
    funcarrayindex = 0; //For thread-level transactions only, keep a single dummy function whose function_number = 0
    if(thread_globvar->funcinst_first == 0){
      function = create_fake_fn_node(threadarrayindex);
      cxt = create_fake_cxt(&function);
      if(!insert_to_funcnodelist(&thread_globvar->funcinfo_first, funcarrayindex, &current_funcinfo_ptr, function, threadarrayindex)){
	insert_to_funcinstlist(&thread_globvar->previous_funcinst, cxt, 1, &current_funcinst_ptr, threadarrayindex, 0); //Then search for the current function, inserting if necessary
      }
    }
    else{
      current_funcinst_ptr = thread_globvar->previous_funcinst;
    }
  }
  else{ //If we do care about functions
    funcarrayindex = CLG_(current_state).cxt->fn[0]->number;
    function = CLG_(current_state).cxt->fn[0];
    //1. Figure out if a function structure has been allocated already. If not, allocate it
    //The real issue here is that this is not necessarily so as simply checking in storeIcontext and storeDRWcontext is not enough to ensure that a BB was not missed in between.
    //It is entirely possible that a return could trigger another return in the next immediate BB, which means neither storeIcontext and storeDRWcontext will be invoked. This
    //would further mean that the current funcinst is not necessarily the caller/callee of the current function. Also, it might be also good to check funcinst numbers. TODO: So there
    //should be a clever way of checking the calltree without resorting to checking from the top. In the interest of time, we will jsut check from the top for now.
    if(!insert_to_funcnodelist(&thread_globvar->funcinfo_first, funcarrayindex, &current_funcinfo_ptr, function, threadarrayindex)){
      
      if((thread_globvar->current_drwbbinfo.previous_bb == 0) || (thread_globvar->previous_funcinst == 0)){ //First check if pointers are valid
	insert_to_funcinstlist(&thread_globvar->funcinst_first, CLG_(current_state).cxt, CLG_(current_state).cxt->size, &current_funcinst_ptr, threadarrayindex, 1);
      }
      else if((thread_globvar->current_drwbbinfo.previous_bb->jmp[thread_globvar->current_drwbbinfo.previous_bb_jmpindex].jmpkind == jk_Call) && (thread_globvar->previous_funcinst->fn_number == CLG_(current_state).cxt->fn[1]->number)){ //Then check conditions
	insert_to_funcinstlist(&thread_globvar->previous_funcinst, CLG_(current_state).cxt, 2, &current_funcinst_ptr, threadarrayindex, 0); //Then search for the current function, inserting if necessary
      }
      else { //Fall through is default
	insert_to_funcinstlist(&thread_globvar->funcinst_first, CLG_(current_state).cxt, CLG_(current_state).cxt->size, &current_funcinst_ptr, threadarrayindex, 1);
      }
      handle_callreturn(current_funcinst_ptr, thread_globvar->previous_funcinst, 0, threadarrayindex);//For a call or the first time a function is seen!
    }
    else if(thread_globvar->previous_funcinst->fn_number != funcarrayindex){
      //else if(CLG_(current_cxt) != CLG_(current_state).bbcc->bb->jmpkind){
      //Check if the context has changed (above statement). Track context with CLG_(current_cxt)
      //Then do the same things in whatever is in the if statement. Thus this could be merged with the if statement and the following else condition can be removed.
      
      //Ignore the above 3 comments if there are any. The first step here is to figure out if the change in function was caused by a call or return. If it is caused by return or call, then we can easily just add or remove the funcinst.
      //If it is not a call or return which caused this, then we need to trace the entire function context just like in the if statement.
      
      if((thread_globvar->current_drwbbinfo.previous_bb->jmp[thread_globvar->current_drwbbinfo.previous_bb_jmpindex].jmpkind == jk_Call) && (thread_globvar->previous_funcinst->fn_number == CLG_(current_state).cxt->fn[1]->number)){
	insert_to_funcinstlist(&thread_globvar->previous_funcinst, CLG_(current_state).cxt, 2, &current_funcinst_ptr, threadarrayindex, 0); //Then search for the current function, inserting if necessary
	handle_callreturn(current_funcinst_ptr, thread_globvar->previous_funcinst, 0, threadarrayindex);
      }
      else if(thread_globvar->current_drwbbinfo.previous_bb->jmp[thread_globvar->current_drwbbinfo.previous_bb_jmpindex].jmpkind == jk_Return){
	temp_caller_ptr = thread_globvar->previous_funcinst->caller; //be careful, is there a condition where caller may not have been defined? I don't think so because a return would not have been recorded without previous funcinst having a caller.
	while(temp_caller_ptr){ //To handle the case where there were successive returns, but neither storeDRW or storeI functions were invoked. I am hoping this will not happen on successive calls as that case is not handled.
	  if(temp_caller_ptr->fn_number == CLG_(current_state).cxt->fn[0]->number){
	    wasreturn_flag = 1;
	    break;
	  }
	  temp_caller_ptr = temp_caller_ptr->caller;
	}
	
	if(wasreturn_flag){
	  current_funcinst_ptr = temp_caller_ptr;
	  temp_caller_ptr_2 = thread_globvar->previous_funcinst->caller;
	  
	  while(temp_caller_ptr_2){ //To handle the case where there were successive returns, but neither storeDRW or storeI functions were invoked. I am hoping this will not happen on successive calls as that case is not handled.
	    if(temp_caller_ptr_2 == temp_caller_ptr){
	      break;
	    }
	    handle_callreturn(current_funcinst_ptr, temp_caller_ptr_2, 1, threadarrayindex);
	    temp_caller_ptr_2 = temp_caller_ptr_2->caller;
	  }
	}
	else{ //Technically this should not be entered as it implies a return to a function that was never called! This could relate to the issue when all called functions are not stored off in my structure!
	  VG_(printf)("Warning: Return to a function with no matching call! \n");
	  insert_to_funcinstlist(&thread_globvar->funcinst_first, CLG_(current_state).cxt, CLG_(current_state).cxt->size, &current_funcinst_ptr, threadarrayindex, 1);
	  handle_callreturn(current_funcinst_ptr, thread_globvar->previous_funcinst, 2, threadarrayindex);
	}
      }
      else{ //This should also include 90112, when function changes according to that code number, it means execution just "walked in" as opposed to explicitly calling
	insert_to_funcinstlist(&thread_globvar->funcinst_first, CLG_(current_state).cxt, CLG_(current_state).cxt->size, &current_funcinst_ptr, threadarrayindex, 1);
	handle_callreturn(current_funcinst_ptr, thread_globvar->previous_funcinst, 2, threadarrayindex);
      }
    }
    else{
      current_funcinst_ptr = thread_globvar->previous_funcinst;
      //Do Nothing else
    }
    //1done. current_funcinfo_ptr and current_funcinst_ptr now point to the current function's structures
  }
  
  //Instrumentation or not:
  if(!CLG_(clo).drw_noinstr && !CLG_(clo).drw_debugtrace){
    //2. If its just to count instructions or even nothing. Just need to change the code below to do nothing
    if(!opsflag){
      CLG_(drw_current_instr) = ea;
      CLG_(total_instrs) += datasize;
      current_funcinst_ptr->instrs += datasize;
      if(CLG_(clo).drw_debug || CLG_(clo).drw_calcmem)
	if(!(CLG_(total_instrs)%1000000))
	  print_debuginfo();
    }
    //3. If this is a memory read
    else if(opsflag == 1){
      //CLG_(read_last_write_cache)(datasize, ea, current_funcinfo_ptr, current_funcinst_ptr, threadarrayindex); //not the way I wanted to implement it, but no time. so HAX
      check_align_and_get_last_writer(ea, datasize, current_funcinfo_ptr, current_funcinst_ptr, threadarrayindex);
      CLG_(total_data_reads) += datasize; // 1;
    }
    //3done.
    //4. If this is a memory write
    else if(opsflag == 2){
      //Take care of the cases when a write is too big in size. In such a case, split the write into multiple small writes
      if(datasize > CLG_(L2_line_size)){
	temp_num_splits = datasize/CLG_(L2_line_size);
	temp_splits_rem = datasize%CLG_(L2_line_size);
	temp_split_datasize = CLG_(L2_line_size);
      }
      for(i = 0; i < temp_num_splits; i++){
	insert_to_drweventlist(0, 2, current_funcinst_ptr, 0, temp_split_datasize, 0);
	insert_to_evtaddrchunklist(&CLG_(drwevent_latest_event)->wlist, temp_ea, temp_ea + temp_split_datasize - 1, &dummy, CLG_(drwevent_latest_event)->producer->tid, CLG_(drwevent_latest_event)->event_num);
	temp_ea += temp_split_datasize;
      }
      if(temp_splits_rem){
	insert_to_drweventlist(0, 2, current_funcinst_ptr, 0, temp_splits_rem, 0);
	insert_to_evtaddrchunklist(&CLG_(drwevent_latest_event)->wlist, temp_ea, temp_ea + temp_split_datasize - 1, &dummy, CLG_(drwevent_latest_event)->producer->tid, CLG_(drwevent_latest_event)->event_num);
      }
      
      check_align_and_put_writer(ea, datasize, current_funcinst_ptr, threadarrayindex);
      //CLG_(put_in_last_write_cache)(datasize, ea, current_funcinst_ptr, threadarrayindex);
      CLG_(total_data_writes) += datasize; // 1;
    }
    //4done.
    //5. If its a integer operation
    else if (opsflag == 3){
      CLG_(total_iops)++;
      current_funcinst_ptr->iops++;
      insert_to_drweventlist(0, 3, current_funcinst_ptr, 0, 1, 0);
    }
    //6. If its a floating point operation
    else if (opsflag == 4){
      CLG_(total_flops)++;
      current_funcinst_ptr->flops++;
      insert_to_drweventlist(0, 4, current_funcinst_ptr, 0, 1, 0);
    }
    else if (opsflag > 5){ //Pthread event
      if(opsflag == 8){
	////This is a pthread create, we need to defer to when the thread actually gets created, so that we can correlate the thread creation to the pthread create requests
	//We should be guaranteed that the thread would have actually been created since this is invoked immediately *after* a pthread call
	if(ea){
	  insert_to_drweventlist(2, opsflag - 5, current_funcinst_ptr, 0, 0, 0);
	  insert_to_evtaddrchunklist(&CLG_(drwevent_latest_event)->rlist, ea, ea, &dummy, CLG_(drwevent_latest_event)->producer->tid, CLG_(drwevent_latest_event)->event_num);
	}
      }
      else if(opsflag == 9){
	insert_to_drweventlist(2, opsflag - 5, current_funcinst_ptr, 0, 0, 0);
	insert_to_evtaddrchunklist(&CLG_(drwevent_latest_event)->rlist, ea, ea, &dummy, CLG_(drwevent_latest_event)->producer->tid, CLG_(drwevent_latest_event)->event_num);
      }
      else{ //To ensure its not pthread create or join
	insert_to_drweventlist(2, opsflag - 5, current_funcinst_ptr, 0, 0, 0);
	if(opsflag == 10)
	  CLG_(pth_node_insert)(&barrier_thread_sets_first, &barrier_thread_sets_last, ea, threadarrayindex);
	insert_to_evtaddrchunklist(&CLG_(drwevent_latest_event)->rlist, ea, ea, &dummy, CLG_(drwevent_latest_event)->producer->tid, CLG_(drwevent_latest_event)->event_num);
      }
    }
  }
  thread_globvar->previous_funcinst = current_funcinst_ptr; //When storeDRWcontext is invoked again, this variable will contain the number of the function in which we were previously residing.
  return;

}

#define FWRITE_BUFSIZE 32000
#define FWRITE_THROUGH 10000
static HChar fwrite_buf[FWRITE_BUFSIZE];
static Int fwrite_pos;
static Int fwrite_fd = -1;

static __inline__
void fwrite_flush(void)
{
    if ((fwrite_fd>=0) && (fwrite_pos>0))
	VG_(write)(fwrite_fd, (void*)fwrite_buf, fwrite_pos);
    fwrite_pos = 0;
}

static void my_fwrite(Int fd, Char* buf, Int len)
{
    if (fwrite_fd != fd) {
	fwrite_flush();
	fwrite_fd = fd;
    }
    if (len > FWRITE_THROUGH) {
	fwrite_flush();
	VG_(write)(fd, (void*)buf, len);
	return;
    }
    if (FWRITE_BUFSIZE - fwrite_pos <= len) fwrite_flush();
    VG_(strncpy)(fwrite_buf + fwrite_pos, buf, len);
    fwrite_pos += len;
}

static inline 
void dump_eventlist_to_file_serialfunc(void)
{
  char buf[4096];
  drwevent* event_list_temp;

  VG_(printf)("Printing events now\n");
  event_list_temp = CLG_(drw_eventlist_start);

  do{
    if(event_list_temp->type)
      VG_(sprintf)(buf, "c%llu,%d,%d,%d,%d,%llu,%llu\n", event_list_temp->event_num, event_list_temp->consumer->fn_number, event_list_temp->consumer->funcinst_number, event_list_temp->producer->fn_number, event_list_temp->producer->funcinst_number, event_list_temp->bytes, event_list_temp->bytes_unique);
    else
      VG_(sprintf)(buf, "o%llu,%d,%d,%llu,%llu,%llu,%llu\n", event_list_temp->event_num, event_list_temp->producer->fn_number, event_list_temp->producer->funcinst_number, event_list_temp->iops, event_list_temp->flops, event_list_temp->non_unique_local, event_list_temp->unique_local);

    //VG_(write)(CLG_(drw_fd), (void*)buf, VG_(strlen)(buf));
    my_fwrite(CLG_(drw_fd), (void*)buf, VG_(strlen)(buf));
    event_list_temp++;
  } while(event_list_temp != CLG_(drw_eventlist_end)); //Since drw_eventlist_end points to the next entry which has not yet been written, we must process all entries upto but not including drw_eventlist_end. The moment event_list_temp gets incremented and points to 'end', the loop will terminate as expected.
}

static __inline__
void dump_eventlist_to_file(void)
{
  //  int num_valid_threads = 0, i = 0;
  char buf[4096];
  drwevent* event_list_temp;
  evt_addrchunknode *addrchunk_temp, *addrchunk_next;
  drwglobvars *thread_globvar;

  VG_(printf)("Printing events now\n");
  event_list_temp = CLG_(drw_eventlist_start);

  do{
    //1. Print out the contents of the event
    if(!event_list_temp->type){
      //VG_(sprintf)(buf, "%lu,%d,%lu,%lu,%lu,%lu,%lu,%lu", event_list_temp->event_num, event_list_temp->producer->tid, event_list_temp->iops, event_list_temp->flops, event_list_temp->non_unique_local, event_list_temp->unique_local, event_list_temp->total_mem_reads, event_list_temp->total_mem_writes);
      VG_(sprintf)(buf, "%llu,%d,%llu,%llu,%llu,%llu", event_list_temp->event_num, event_list_temp->producer->tid, event_list_temp->iops, event_list_temp->flops, event_list_temp->total_mem_reads, event_list_temp->total_mem_writes);
      thread_globvar = CLG_(thread_globvars)[event_list_temp->producer->tid];
    }
    else if(event_list_temp->type == 1){
      //VG_(sprintf)(buf, "%lu,%d,%d,%lu,%lu", event_list_temp->event_num, event_list_temp->producer->tid, event_list_temp->consumer->tid, event_list_temp->bytes, event_list_temp->bytes_unique);
      VG_(sprintf)(buf, "%llu,%d", event_list_temp->event_num, event_list_temp->consumer->tid);
      thread_globvar = CLG_(thread_globvars)[event_list_temp->consumer->tid];
    }
    else if(event_list_temp->type == 2){
      VG_(sprintf)(buf, "%llu,%d,pth_ty: %d", event_list_temp->event_num, event_list_temp->producer->tid, event_list_temp->optype);
      thread_globvar = CLG_(thread_globvars)[event_list_temp->producer->tid];
    }

    //VG_(write)(thread_globvar->previous_funcinst->fd, (void*)buf, VG_(strlen)(buf));
    my_fwrite(thread_globvar->previous_funcinst->fd, (void*)buf, VG_(strlen)(buf));
    
    //2. Print out the contents of the list of computation and communication events
    addrchunk_temp = event_list_temp->wlist;
    while(addrchunk_temp){
      //Print out the dependences for this event in the same line
      if(event_list_temp->type)
	VG_(sprintf)(buf, " WHAAT. This is wrong");
      else
	VG_(sprintf)(buf, " $ %lu %lu", addrchunk_temp->rangefirst, addrchunk_temp->rangelast );
      //VG_(write)(thread_globvar->previous_funcinst->fd, (void*)buf, VG_(strlen)(buf));
      my_fwrite(thread_globvar->previous_funcinst->fd, (void*)buf, VG_(strlen)(buf));
      addrchunk_next = addrchunk_temp->next;
      VG_(free)(addrchunk_temp);
      CLG_(num_eventaddrchunk_nodes)--;
      addrchunk_temp = addrchunk_next;
    }
    event_list_temp->wlist = 0;

    //3. Print out the contents of the list
    addrchunk_temp = event_list_temp->rlist;
    while(addrchunk_temp){
      //Print out the dependences for this event in the same line
      if(event_list_temp->type == 2)
	VG_(sprintf)(buf, " ^ %lu", addrchunk_temp->rangefirst);
      else if(event_list_temp->type == 1)
	VG_(sprintf)(buf, " # %d %llu %llu %llu", addrchunk_temp->shared_read_tid, addrchunk_temp->shared_read_event_num, addrchunk_temp->rangefirst, addrchunk_temp->rangelast );
      else
	VG_(sprintf)(buf, " * %lu %lu", addrchunk_temp->rangefirst, addrchunk_temp->rangelast );
      //VG_(write)(thread_globvar->previous_funcinst->fd, (void*)buf, VG_(strlen)(buf));
      my_fwrite(thread_globvar->previous_funcinst->fd, (void*)buf, VG_(strlen)(buf));
      addrchunk_next = addrchunk_temp->next;
      VG_(free)(addrchunk_temp);
      CLG_(num_eventaddrchunk_nodes)--;
      addrchunk_temp = addrchunk_next;
    }
    event_list_temp->rlist = 0;

    if(CLG_(clo).drw_debug){
      VG_(sprintf)(buf, " in %s and %s. ", event_list_temp->debug_node1->name, event_list_temp->debug_node2->name);
      if(!event_list_temp->type)
	thread_globvar = CLG_(thread_globvars)[event_list_temp->producer->tid];
      else if(event_list_temp->type == 1)
	thread_globvar = CLG_(thread_globvars)[event_list_temp->consumer->tid];
      else
	thread_globvar = CLG_(thread_globvars)[event_list_temp->producer->tid];
      //VG_(write)(thread_globvar->previous_funcinst->fd, (void*)buf, VG_(strlen)(buf));
      my_fwrite(thread_globvar->previous_funcinst->fd, (void*)buf, VG_(strlen)(buf));
    }

    //Print out the dependences for this event in the same line
    VG_(sprintf)(buf, "\n" );
    //VG_(write)(thread_globvar->previous_funcinst->fd, (void*)buf, VG_(strlen)(buf));
    my_fwrite(thread_globvar->previous_funcinst->fd, (void*)buf, VG_(strlen)(buf));    

    event_list_temp++;
  } while(event_list_temp != CLG_(drw_eventlist_end)); //Since drw_eventlist_end points to the next entry which has not yet been written, we must process all entries upto but not including drw_eventlist_end. The moment event_list_temp gets incremented and points to 'end', the loop will terminate as expected.
  if(CLG_(num_eventaddrchunk_nodes))
    VG_(printf)("Encountered %llu un-freed nodes\n", CLG_(num_eventaddrchunk_nodes));
}

static __inline__
void insert_to_drweventlist( int type, int optype , funcinst *producer, funcinst *consumer, ULong count, ULong count_unique ){ 
//type 0-operation 1-communication 2-pthread_event, optype - 0-nothing 1-memread 2-memwrite 3-iops 4-flops, count - represents bytes for type 1 and numops for type 0
//for pthread_event: optype - 0-nothing, 1 - mutex_lock, 2 - mutex_unlock
  int same_as_last_event_flag = 0;
  drwglobvars *thread_globvar = CLG_(thread_globvars)[CLG_(current_tid)];
  if(!CLG_(clo).drw_events){
    return;
  }
  //For function-level events do not collect computation events - for now. We should make this a command line option soon
  //if(CLG_(clo).drw_thread_or_func)
  //if(!type) return;
  //1. What kind of event? Operation or communication?
  //a. Is the kind of event same as the current event's kind?
  if(CLG_(drwevent_latest_event)){
    if(CLG_(drwevent_latest_event)->type == type){
      //aa. If yes, are the participants the same?
      if(CLG_(drwevent_latest_event)->producer == producer && CLG_(drwevent_latest_event)->consumer == consumer){
	//Adding a clause to check if we have exceeded 500 memory writes. If so, please create a new event
	if(CLG_(drwevent_latest_event)->total_mem_writes < CLG_(clo).drw_splitcomp && CLG_(drwevent_latest_event)->total_mem_reads < CLG_(clo).drw_splitcomp)
	  //Set a flag to indicate that the existing latest event can be used
	  same_as_last_event_flag = 1;
      }
    }
  }

  //2.
  //1.b.,bb.,bbb. Create new event while checking if we have reached the event memory limits.
  if(!same_as_last_event_flag){
    if((CLG_(drw_eventlist_end) == &CLG_(drw_eventlist_start)[(CLG_(event_info_size) * 1024) - 1] + 1) ||
       ((CLG_(num_eventaddrchunk_nodes) * (sizeof(evt_addrchunknode)/1024) >= MAX_EVENTADDRCHUNK_SIZE * 1024 * 1024))){
      //Handle the dumping and re-use of files
      if(!CLG_(clo).drw_thread_or_func)
	dump_eventlist_to_file();
      else dump_eventlist_to_file_serialfunc();
      CLG_(drw_eventlist_end) = CLG_(drw_eventlist_start);
      CLG_(num_eventdumps)++;
      VG_(printf)("Printed to output. Dump no. %d\n", CLG_(num_eventdumps));
    }
    //b. Create a new event, initializing type etc. and counts
    //DRWDEBUG
    if(CLG_(drwevent_latest_event) && CLG_(clo).drw_debug)
      CLG_(drwevent_latest_event)->debug_node2 = CLG_(current_state).cxt->fn[0];
    CLG_(drwevent_latest_event) = CLG_(drw_eventlist_end); //Take one from the end and increment the end
    CLG_(drw_eventlist_end)++;
    //Initialize variables in the latest event
    CLG_(drwevent_latest_event)->type = type;
    CLG_(drwevent_latest_event)->optype = optype;
    CLG_(drwevent_latest_event)->producer = producer;
    if(type == 2){ //Pthread event
      CLG_(drwevent_latest_event)->consumer = consumer;
      CLG_(drwevent_latest_event)->producer = producer;
      CLG_(drwevent_latest_event)->event_num = producer->num_events;
      producer->num_events++;
    }
    else if(type == 1){
      CLG_(drwevent_latest_event)->consumer = consumer;
      //CLG_(drwevent_latest_event)->event_num = CLG_(thread_globvars)[consumer->tid]->num_events;
      //CLG_(thread_globvars)[consumer->tid]->num_events++;
      CLG_(drwevent_latest_event)->event_num = consumer->num_events;
      consumer->num_events++;
    }
    else{
      CLG_(drwevent_latest_event)->consumer = 0;
      //CLG_(drwevent_latest_event)->event_num = CLG_(thread_globvars)[producer->tid]->num_events;
      //CLG_(thread_globvars)[producer->tid]->num_events++;
      CLG_(drwevent_latest_event)->event_num = producer->num_events;
      producer->num_events++;
    }
    CLG_(drwevent_latest_event)->bytes = CLG_(drwevent_latest_event)->bytes_unique = CLG_(drwevent_latest_event)->iops = CLG_(drwevent_latest_event)->flops = CLG_(drwevent_latest_event)->shared_bytes_written = 0;
    CLG_(drwevent_latest_event)->unique_local = CLG_(drwevent_latest_event)->non_unique_local = CLG_(drwevent_latest_event)->total_mem_reads = CLG_(drwevent_latest_event)->total_mem_writes = 0; //Unique will give the number of cold misses and non-unique can be used to calculate hits
    CLG_(drwevent_latest_event)->rlist = 0; CLG_(drwevent_latest_event)->wlist = 0;
    if(CLG_(clo).drw_debug){
      CLG_(drwevent_latest_event)->debug_node1 = CLG_(current_state).cxt->fn[0]; CLG_(drwevent_latest_event)->debug_node2 = CLG_(current_state).cxt->fn[0];
    }
  }

  //3. Update the counts.
  //1.aaa. If yes, then increment counts for the current event, if necessary add addresses etc and return
  if(type == 2){ //For a pthread event the optype means something else. Nothing to do here for now
    switch(optype){
    case 0:
      break;
    case 1://mutex_lock
      thread_globvar->in_pthread_api_call = 1;
      break;
    case 2://mutex_unlock
      thread_globvar->in_pthread_api_call = 1;
      break;
    case 3://pthread_create
      break;
    case 4://pthread_join
      break;
    case 5: //pthread_barrier_wait
      thread_globvar->in_pthread_api_call = 1;
      break;
    case 6: //pthread_cond_wait
      thread_globvar->in_pthread_api_call = 1;
      break;
    case 7: //pthread_cond_signal
      thread_globvar->in_pthread_api_call = 1;
      break;
    case 8: //pthread_spin_lock
      thread_globvar->in_pthread_api_call = 1;
      break;
    case 9: //pthread_spin_unlock
      thread_globvar->in_pthread_api_call = 1;
      break;
    default:
      VG_(printf)("Incorrect optype specified when inserting pthread to drweventlist");
      tl_assert(0);
    }
  }
  else if(type == 1 && !optype){
    CLG_(drwevent_latest_event)->bytes += count;
    CLG_(drwevent_latest_event)->bytes_unique += count_unique;
  }
  else{
    switch(optype){
    case 1:
      CLG_(drwevent_latest_event)->non_unique_local += count;
      CLG_(drwevent_latest_event)->unique_local += count_unique;
      CLG_(drwevent_latest_event)->total_mem_reads++;
      break;
    case 2:
      CLG_(drwevent_latest_event)->total_mem_writes++;
      break;
    case 3:
      CLG_(drwevent_latest_event)->iops += count;
      break;
    case 4:
      CLG_(drwevent_latest_event)->flops += count;
      break;
    default:
      VG_(printf)("Incorrect optype specified when inserting to drweventlist");
      tl_assert(0);
    }
  }
}

static void print_for_funcinst (funcinst *funcinstpointer, int fd, char* caller, drwglobvars *thread_globvar){
  char drw_funcname[4096], buf[8192];
  int drw_funcnum, drw_funcinstnum, drw_tid;
  ULong drw_funcinstr;
  dependencelist_elemt *consumerfuncinfopointer;
  dependencelist *dependencelistpointer;
  ULong zero = 0; int i;

  /****3. ITERATE OVER PRODUCER LIST OF ADDRESS CHUNKS****/
  /****3. DONE ITERATION OVER PRODUCER LIST OF ADDRESS CHUNKS****/

  //Added because when we run with --toggle-collect=main, some funcinst instances have their "function_info" variables as empty (0x0), because I suppose instrumentation does not happen for them, but funcinsts get created for them as their children may get created. We should actually be allowed to skip this and return without printing anything
  if(funcinstpointer->function_info == 0)
    VG_(sprintf)(drw_funcname, "%s", "NOT COLLECTED");
  else
    VG_(sprintf)(drw_funcname, "%s", funcinstpointer->function_info->function->name);

  VG_(sprintf)(buf, "%-20d %-20d %-20d %50s %20s %-20llu %-20llu %-20llu %-20llu %-20llu %-20llu %-20llu\n", funcinstpointer->tid, funcinstpointer->fn_number, funcinstpointer->funcinst_number, /*funcinstpointer->function_info->function->name*/drw_funcname, caller, funcinstpointer->instrs, funcinstpointer->flops, funcinstpointer->iops, funcinstpointer->ip_comm_unique, /*funcinstpointer->op_comm_unique*/zero, funcinstpointer->ip_comm, /*funcinstpointer->op_comm*/zero);
  //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));
  my_fwrite(fd, (void*)buf, VG_(strlen)(buf));

  /****4. ITERATE OVER CONSUMED LIST****/
  dependencelistpointer = funcinstpointer->consumedlist;
  while(1){
    for(i = 0; i < dependencelistpointer->size; i++){
      consumerfuncinfopointer = &dependencelistpointer->list_chunk[i];
      
      if(consumerfuncinfopointer->vert_parent_fn == consumerfuncinfopointer->consumed_fn){
	VG_(sprintf)(drw_funcname, "%s", "SELF");
	drw_funcnum = SELF;
	drw_funcinstnum = 0;
	drw_funcinstr = 0;
	drw_tid = 0;
      }
      else if(consumerfuncinfopointer->consumed_fn == 0){
	VG_(sprintf)(drw_funcname, "%s", "NO PRODUCER");
	drw_funcnum = NO_PROD;
	drw_funcinstnum = 0;
	drw_funcinstr = 0;
	drw_tid = 0;
      }
      else{
	VG_(sprintf)(drw_funcname, "%s", consumerfuncinfopointer->consumed_fn->function_info->function->name);
	drw_funcnum = consumerfuncinfopointer->consumed_fn->fn_number;
	drw_funcinstnum = consumerfuncinfopointer->consumed_fn->funcinst_number;
	drw_funcinstr = consumerfuncinfopointer->consumed_fn->instrs;
	drw_tid = consumerfuncinfopointer->consumed_fn->tid;
      }
      
      VG_(sprintf)(buf, "%-20d %-20d %-20d %50s %20s %-20llu %20s %20s %-20llu %-20d %-20llu %-20d\n", drw_tid, drw_funcnum, drw_funcinstnum, drw_funcname, " ", drw_funcinstr, " ", " ", consumerfuncinfopointer->count_unique, 0, consumerfuncinfopointer->count, 0);
      my_fwrite(fd, (void*)buf, VG_(strlen)(buf));
    }
    if(!dependencelistpointer->next) break;
    else dependencelistpointer = dependencelistpointer->next;  
  }
  
  /****4. DONE ITERATION OVER CONSUMED LIST****/
  VG_(sprintf)(buf, "\n");
  //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));
  my_fwrite(fd, (void*)buf, VG_(strlen)(buf));
  /****6. ITERATE OVER CONSUMER LIST****/
  consumerfuncinfopointer = funcinstpointer->consumerlist;
  i = 0;
  while(consumerfuncinfopointer){
    i++;
    if(consumerfuncinfopointer->vert_parent_fn == consumerfuncinfopointer->consumed_fn){
      consumerfuncinfopointer = consumerfuncinfopointer->next_horiz;
      continue;
    }
    else if(consumerfuncinfopointer->consumed_fn == 0){
      VG_(sprintf)(drw_funcname, "%s", "NO PRODUCER");
      drw_funcnum = NO_PROD;
      drw_funcinstnum = 0;
      drw_funcinstr = 0;
      drw_tid = 0;
    }
    else{
      VG_(sprintf)(drw_funcname, "%s", consumerfuncinfopointer->vert_parent_fn->function_info->function->name);
      drw_funcnum = consumerfuncinfopointer->vert_parent_fn->fn_number;
      drw_funcinstnum = consumerfuncinfopointer->vert_parent_fn->funcinst_number;
      drw_funcinstr = consumerfuncinfopointer->vert_parent_fn->instrs;
      drw_tid = consumerfuncinfopointer->vert_parent_fn->tid;
    }
    VG_(sprintf)(buf, "%-20d %-20d %-20d %50s %20s %-20lu %20s %20s %-20lu %-20lu %-20lu %-20lu\n", drw_tid, drw_funcnum, drw_funcinstnum, drw_funcname, " ", drw_funcinstr, " ", " ", 0, consumerfuncinfopointer->count_unique, 0, consumerfuncinfopointer->count);
    //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));
    my_fwrite(fd, (void*)buf, VG_(strlen)(buf));
    consumerfuncinfopointer = consumerfuncinfopointer->next_horiz;
  }
  /****6. DONE ITERATION OVER CONSUMER LIST****/
  VG_(sprintf)(buf, "\n\n");
  //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));
  my_fwrite(fd, (void*)buf, VG_(strlen)(buf));
}

static void print_recurse_data (funcinst *funcinstpointer, int fd, int depth, drwglobvars *thread_globvar){ //Depth indicates how many recursions have been done. 
  int j;
  char caller[3];
  VG_(sprintf)(caller, "*");
  print_for_funcinst(funcinstpointer, fd, caller, thread_globvar); //Print the current function and the functions it has consumed from. ** denotes that all the *s that follow will be the immediate callees of this function.
  for(j = 0; j < funcinstpointer->num_callees; j++) //Traverse all the callees of the currentfuncinst
    print_recurse_data(funcinstpointer->callees[j], fd, depth + 1, thread_globvar);
}

static void print_recurse_tree (funcinst *funcinstpointer, int fd, drwglobvars *thread_globvar){ //Prints by serializing the tree based on DWARF's debugging standard
  int j; char buf[8192];
  VG_(sprintf)(buf, "%d, %d, %s, %d\n", funcinstpointer->fn_number, funcinstpointer->funcinst_number, ((funcinstpointer->num_callees) ? "True":"False"), funcinstpointer->num_calls);
  //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));
  my_fwrite(fd, (void*)buf, VG_(strlen)(buf));
  for(j = 0; j < funcinstpointer->num_callees; j++) //Traverse all the callees of the currentfuncinst
    print_recurse_tree(funcinstpointer->callees[j], fd, thread_globvar);
  //If callees were present, the algorithm dictates that a "none" must be printed at the end
  if(funcinstpointer->num_callees){
    VG_(sprintf)(buf, "None\n");
    //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));
    my_fwrite(fd, (void*)buf, VG_(strlen)(buf));
  }
}

void CLG_(print_to_file) ()
{

  char drw_filename[50], buf[4096];
  SysRes res;
  int i, fd, num_valid_threads = 0;
  drwglobvars* thread_globvar = 0;
  //time_t t;

  if(CLG_(clo).drw_debugtrace){
    fwrite_flush();
    return;
  }
  VG_(printf)("PRINTING Sigil's output now\n");
  
  if(CLG_(clo).drw_events){
    VG_(printf)("Total Number of Shared Reads MISSED due to dumping of events: %lu and due to lost pointers: %lu\n", CLG_(shared_reads_missed_dump), CLG_(shared_reads_missed_pointer));
    //Close out any remaining events and close the file in the following loop
    if(!CLG_(clo).drw_thread_or_func)
      dump_eventlist_to_file();
    else dump_eventlist_to_file_serialfunc();
  }

  if(CLG_(clo).drw_intercepts){
    ULong threadmap; Addr addr;
    while(1){
      CLG_(pth_node_outsert)(&barrier_thread_sets_first, &barrier_thread_sets_last, &addr, &threadmap);
      if(addr)
	VG_(printf)("Barrier %ld in Threadmap %lu\n",addr,threadmap);
      else
	break;
    }
  }

  if(CLG_(clo).drw_calcmem){
    VG_(printf)("SUMMARY: \n\n");
    VG_(printf)("Total Memory Reads(bytes): %-20lu Total Memory Writes(bytes): %-20lu Total Instrs: %-20lu Total Flops: %-20lu Iops: %-20lu\n\n",CLG_(total_data_reads), CLG_(total_data_writes), CLG_(total_instrs), CLG_(total_flops), CLG_(total_iops));
/*     VG_(printf)("Num SMs: %-20lu Num funcinsts: %-20lu Memory for PM(bytes): %-20lu Size of SM: %-20lu Size of SM_event: %-20lu Size of funcinst: %-20lu\n\n",CLG_(num_sms), CLG_(num_funcinsts), sizeof(PM), sizeof(SM), sizeof(SM_event), sizeof(funcinst)); */
/*     if(CLG_(clo).drw_events) */
/*       CLG_(tot_memory_usage) = CLG_(num_sms) * sizeof(SM_event) + CLG_(num_funcinsts) * sizeof(funcinst) + sizeof(PM); */
/*     else */
/*       CLG_(tot_memory_usage) = CLG_(num_sms) * sizeof(SM) + CLG_(num_funcinsts) * sizeof(funcinst) + sizeof(PM); */
/*     VG_(printf)("Memory for SM(bytes): %-20lu = %-20lu(Mb) Memory for SM_event(bytes): %-20lu = %-20lu(Mb) Memory for funcinsts(bytes): %-20lu = %-20lu(Mb)\n\n", CLG_(num_sms) * sizeof(SM), CLG_(num_sms) * sizeof(SM)/1024/1024, CLG_(num_sms) * sizeof(SM_event), CLG_(num_sms) * sizeof(SM_event)/1024/1024, CLG_(num_funcinsts) * sizeof(funcinst), CLG_(num_funcinsts) * sizeof(funcinst)/1024/1024); */
    print_debuginfo();
    if(CLG_(clo).drw_calcmem)
      return;
  }

  //Perform printing for each thread separately

  for(i = 0; i < MAX_THREADS; i++){
    thread_globvar = CLG_(thread_globvars)[i];
    if(thread_globvar) num_valid_threads++;
    if (!thread_globvar) continue;

    //Close the event files for each thread
    fwrite_flush();
    if(!CLG_(clo).drw_thread_or_func)
      VG_(close)( (Int)sr_Res(thread_globvar->previous_funcinst->res) );
    else
      VG_(close)( (Int)sr_Res(CLG_(drw_res)) );

  /***1. CREATE THE NECESSARY FILE***/
    VG_(sprintf)(drw_filename, "sigil.totals.out-%d",i);
    res = VG_(open)(drw_filename, VKI_O_WRONLY|VKI_O_TRUNC, 0);
    
    if (sr_isError(res)) {
      res = VG_(open)(drw_filename, VKI_O_CREAT|VKI_O_WRONLY,
		      VKI_S_IRUSR|VKI_S_IWUSR);
      if (sr_isError(res)) {
	file_err(); // If can't open file, then create one and then open. If still erroring, Valgrind will die using this call - Sid
      }
    }
    
    fd = (Int) sr_Res(res);
    /***1. DONE CREATION***/
    
    //PRINT METADATA
    VG_(sprintf)(buf, "Tool: Sigil \nVersion: 1.0\n\n");
    my_fwrite(fd, (void*)buf, VG_(strlen)(buf)); // This will end up writing into the orignial callgrind.out file
    //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));
    
/*     time(&t); */
/*     VG_(sprintf)("Timestamp: %s",ctime(&t)); */
/*     //  my_fwrite(fd, (void*)buf, VG_(strlen)(buf)); // This will end up writing into the orignial callgrind.out file */
/*     VG_(write)(fd, (void*)buf, VG_(strlen)(buf)); */

    VG_(sprintf)(buf, "SUMMARY: \n\n");
    my_fwrite(fd, (void*)buf, VG_(strlen)(buf)); // This will end up writing into the orignial callgrind.out file
    //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));

    VG_(sprintf)(buf, "Total Memory Reads(bytes): %-20lu Total Memory Writes(bytes): %-20lu Total Instrs: %-20lu Total Flops: %-20lu Iops: %-20lu\n\n",CLG_(total_data_reads), CLG_(total_data_writes), CLG_(total_instrs), CLG_(total_flops), CLG_(total_iops));
    my_fwrite(fd, (void*)buf, VG_(strlen)(buf)); // This will end up writing into the orignial callgrind.out file
    //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));
    
    if(CLG_(clo).drw_events)
      CLG_(tot_memory_usage) = CLG_(num_sms) * sizeof(SM_event) + CLG_(num_funcinsts) * sizeof(funcinst) + sizeof(PM);
    else
      CLG_(tot_memory_usage) = CLG_(num_sms) * sizeof(SM) + CLG_(num_funcinsts) * sizeof(funcinst) + sizeof(PM);
    VG_(sprintf)(buf, "Num SMs: %-20lu Num funcinsts: %-20lu Memory for SM(bytes): %-20lu Memory for funcinsts(bytes): %-20lu Memory for PM(bytes): %-20lu Total Memory Usage: %-20lu\n\n",CLG_(num_sms), CLG_(num_funcinsts), CLG_(num_sms) * sizeof(SM), CLG_(num_funcinsts) * sizeof(funcinst), sizeof(PM), CLG_(tot_memory_usage));
    my_fwrite(fd, (void*)buf, VG_(strlen)(buf)); // This will end up writing into the orignial callgrind.out file
    //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));

    VG_(sprintf)(buf, "\n\nTREE DUMP\n\n");
    my_fwrite(fd, (void*)buf, VG_(strlen)(buf)); // This will end up writing into the orignial callgrind.out file
    //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));
    
    VG_(sprintf)(buf, "%s, %s, %s, %s\n", "FUNCTION NUMBER", "FUNC_INST NUM", "Children?", "Number of calls");
    my_fwrite(fd, (void*)buf, VG_(strlen)(buf)); // This will end up writing into the orignial callgrind.out file
    //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));
    
    print_recurse_tree(thread_globvar->funcinst_first, fd, thread_globvar);
    
    VG_(sprintf)(buf, "\n\nEND TREE DUMP\n\n");
    my_fwrite(fd, (void*)buf, VG_(strlen)(buf)); // This will end up writing into the orignial callgrind.out file
    //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));
    
    VG_(sprintf)(buf, "\nDATA DUMP\n\n");
    my_fwrite(fd, (void*)buf, VG_(strlen)(buf)); // This will end up writing into the orignial callgrind.out file
    //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));
    
    VG_(sprintf)(buf, "%20s %20s %20s %50s %20s %20s %20s %20s %20s %20s %20s %20s\n\n\n","THREAD NUMBER", "FUNCTION NUMBER", "FUNC_INST NUM", "FUNCTION NAME", "PROD?", "INSTRS", "FLOPS", "IOPS", "IPCOMM_UNIQUE", "OPCOMM_UNIQUE", "IPCOMM", "OPCOMM");
    my_fwrite(fd, (void*)buf, VG_(strlen)(buf)); // This will end up writing into the orignial callgrind.out file
    //VG_(write)(fd, (void*)buf, VG_(strlen)(buf));
    
    print_recurse_data(thread_globvar->funcinst_first, fd, 0, thread_globvar);
    
    fwrite_flush();
    VG_(close)( (Int)sr_Res(res) );
    if (num_valid_threads == CLG_(num_threads))
      break;
  }
}

static int insert_to_evtaddrchunklist(evt_addrchunknode** chunkoriginal, Addr range_first, Addr range_last, ULong* refarg, int shared_read_tid, ULong shared_read_event_num) {

  evt_addrchunknode *chunk = *chunkoriginal; // chunk points to the first element of addrchunk array.
  evt_addrchunknode *chunknodetemp, *chunknodecurr, *next_chunk;
  Addr curr_range_last;
  ULong count = range_last - range_first + 1;
  ULong return_count = count ;
  int partially_found = 0, return_value2 = 0;

  curr_range_last = range_last;

    //if the determined array element contains any address chunks, then those address
    //chunks are searched to determine if the address range being inserted is already present or should be inserted
    // if the original pointer points to nothing, then a new node is inserted
    
    if(chunk == 0) {
      chunknodetemp = (evt_addrchunknode*) CLG_MALLOC("c1.evt_addrchunknode.gc.1",sizeof(evt_addrchunknode));
      chunknodetemp -> prev = 0;
      chunknodetemp -> next =0;
      chunknodetemp -> rangefirst = range_first;
      chunknodetemp -> rangelast = curr_range_last;
      chunknodetemp -> shared_read_tid = shared_read_tid;
      chunknodetemp -> shared_read_event_num = shared_read_event_num;
      *chunkoriginal = chunknodetemp;
      //CLG_(tot_eventinfo_size) += sizeof(evt_addrchunknode);
      CLG_(num_eventaddrchunk_nodes)++;
    } // end of if that checks if original == 0
    else {
      chunknodecurr = *chunkoriginal;
      
      //while iterates over evt_addrchunknodes present in the given array element until it finds
      // the desired address or it reaches end of the list.
      while(1) {
	// if the address being inserted is less than current node's address, then a new node is inserted,
	// and further search is terminated as the node's are in increasing order of addresses
	if(curr_range_last < (chunknodecurr->rangefirst-1)) {
	  chunknodetemp = (evt_addrchunknode*) CLG_MALLOC("c1.evt_addrchunknode.gc.2",sizeof(evt_addrchunknode));
	  chunknodetemp -> prev = chunknodecurr->prev;
	  chunknodetemp -> next = chunknodecurr;
	  
	  if(chunknodecurr->prev!=0) chunknodecurr -> prev->next = chunknodetemp;
	  else *chunkoriginal = chunknodetemp;
	  chunknodecurr -> prev = chunknodetemp;
	  chunknodetemp -> rangefirst = range_first;
	  chunknodetemp -> rangelast = curr_range_last;
	  chunknodetemp -> shared_read_tid = shared_read_tid;
	  chunknodetemp -> shared_read_event_num = shared_read_event_num;
	  //	  CLG_(tot_eventinfo_size) += sizeof(evt_addrchunknode);
	  CLG_(num_eventaddrchunk_nodes)++;
	  break;
	}
	// if current node falls within the address range being inserted, then the current node is deleted and the
	// counts are adjusted appropriately
	else if((range_first <= (chunknodecurr->rangefirst-1)) && (curr_range_last >= (chunknodecurr->rangelast +1))) {
	  return_value2 =1;
	  return_count -= chunknodecurr->rangelast - chunknodecurr->rangefirst + 1;
	  partially_found =1;
	  if(chunknodecurr->next ==0 ){
	    chunknodecurr -> rangefirst = range_first;
	    chunknodecurr -> rangelast = curr_range_last;
	    chunknodecurr -> shared_read_tid = shared_read_tid;
	    chunknodecurr -> shared_read_event_num = shared_read_event_num;
	    break;
	  }
	  else {
	    if(chunknodecurr->prev!=0) chunknodecurr->prev->next = chunknodecurr->next;
	    else *chunkoriginal = chunknodecurr->next;
	    
	    if(chunknodecurr->next!=0) chunknodecurr->next->prev = chunknodecurr->prev;
	    next_chunk = chunknodecurr ->next;
	    VG_(free)(chunknodecurr);
	    CLG_(num_eventaddrchunk_nodes)--;
	    chunknodecurr = next_chunk;
	  }
	}
	// if the address being inserted is more than current node's address range, search continues with the next
	// node
	else if(range_first > (chunknodecurr->rangelast +1)){
	  if(chunknodecurr->next != 0) chunknodecurr = chunknodecurr->next;
	  else {
	    
	    chunknodetemp = (evt_addrchunknode*) CLG_MALLOC("c1.evt_addrchunknode.gc.1",sizeof(evt_addrchunknode));
	    chunknodetemp -> prev = chunknodecurr;
	    chunknodetemp -> next = 0;
	    chunknodecurr -> next = chunknodetemp;
	    
	    chunknodetemp -> rangefirst = range_first;
	    chunknodetemp -> rangelast = curr_range_last;
	    chunknodetemp -> shared_read_tid = shared_read_tid;
	    chunknodetemp -> shared_read_event_num = shared_read_event_num;
	    //	    CLG_(tot_eventinfo_size) += sizeof(evt_addrchunknode);
	    CLG_(num_eventaddrchunk_nodes)++;
	    break;
	  }
	}
	// if the address range being inserted falls within the current node's address range, count of current node is adjusted
	// appropriately, and the search is terminated
	else if((range_first >= (chunknodecurr->rangefirst))&&(curr_range_last<=(chunknodecurr->rangelast))) {
	  return_count -= (curr_range_last- range_first+1);
	  return_value2=1;
	  break;
	}
	
	else {
	  //if last address of the address range being inserted falls within current node, then range and count of current node
	  // are adjusted appropriately and search is terminated
	  if(range_first <= (chunknodecurr->rangefirst -1))
	    {
	      if(curr_range_last != (chunknodecurr->rangefirst -1)) return_value2 =1;
	      return_count -= (curr_range_last - (chunknodecurr->rangefirst)+1);
	      chunknodecurr -> rangefirst = range_first;
	      break;
	    }
	  // if the last address of the address range being inserted is after the current node's address,
	  //and it is less than next node's start address, then current node's range is adjusted appropriately and the search terminates
	  //This implicitly takes care of the case where rangefirst is greater than chunknodecurr->rangefirst but less than chunknodecurr->rangelast. This is true by process of elimination above.
	  if((curr_range_last >= (chunknodecurr->rangelast +1)) &&((chunknodecurr->next ==0)||(curr_range_last < chunknodecurr->next->rangefirst -1)))  {
	    if(range_first != (chunknodecurr->rangelast+1)) return_value2 =1;
	    return_count -= ((chunknodecurr->rangelast)-range_first+1);
	    chunknodecurr -> rangelast = curr_range_last;
	    break;
	  }
	  
	  // if the last address of the address range being inserted falls exactly at the edge or within the next node's range, then
	  //current node is deleted and the search is continued
	  else if((curr_range_last >= (chunknodecurr->rangelast +1)) &&(chunknodecurr->next !=0)&&(curr_range_last >= (chunknodecurr->next->rangefirst -1))) {
	    if(range_first != (chunknodecurr->rangelast+1)) return_value2 =1;
	    return_count -= ((chunknodecurr -> rangelast)- range_first +1);
	    range_first = chunknodecurr->rangefirst;
	    if(chunknodecurr->prev!=0) chunknodecurr->prev->next = chunknodecurr->next;
	    else *chunkoriginal = chunknodecurr->next;
	    
	    if(chunknodecurr->next !=0) chunknodecurr->next->prev = chunknodecurr->prev;
	    next_chunk = chunknodecurr->next;
	    VG_(free)(chunknodecurr);
	    CLG_(num_eventaddrchunk_nodes)--;
	    chunknodecurr = next_chunk;
	  }
	}
	//break while loop if end of the evt_addrchunknode list is reached
	if(chunknodecurr == 0) break;
      } // end of while (1)
    } // end of else corresponding to original ==0 check

  //number of addresses not found are returned
  *refarg = return_count;
  // if the address range is fully found, return 2
  if (return_count == 0) partially_found = 2;
  // if the address range is partially found return 1, else return 0.
  else if(return_value2 == 1) partially_found = 1;
  return partially_found;
}

//For FIFO containers. Very simple. Don't check anything, just insert and leave.
static void CLG_(pth_node_insert)(pth_node** pth_node_first, pth_node** pth_node_last, Addr addr, int tid){
  pth_node *temp = *pth_node_first;
  pth_node *temp_malloc;
  //Searching types
  int found = 0;
  //1. Check the list first
  if(*pth_node_first) {
    while(temp->next){
      if(temp->addr == addr){
	found = 1;
	break;
      }
      temp = temp->next;
    }
    //Check the last node
    if(temp->addr == addr)
      found = 1;
  }
  //2. Check the flag and if its found, update the threadmap
  if(found)
    temp->threadmap |= 1 << (tid - 1);
  //If not found, then malloc, init threadmap, update pth_node_first/last etc.
  else{
  //Malloc the new node that has to go in anyway
    temp_malloc = (pth_node*) CLG_MALLOC("c1.pth_node.gc.1",sizeof(pth_node));
    temp_malloc->addr = addr; temp_malloc->threadmap = 0;
    temp_malloc->next = temp_malloc->prev = 0;
    temp_malloc->threadmap |= 1 << (tid - 1);
    if(*pth_node_last){
      (*pth_node_last)->next = temp_malloc;
      temp_malloc->prev = *pth_node_last;
    }
    *pth_node_last = temp_malloc; // Make sure it is the last node
    
    //1. If there is no node prior to this
    if(!(*pth_node_first)){
      *pth_node_first = temp_malloc;
    }
  }
}

//For FIFO containers. Very simple. Don't check anything, just insert and leave.
static void CLG_(pth_node_outsert)(pth_node** pth_node_first, pth_node** pth_node_last, Addr *addr, ULong *threadmap){
  pth_node *temp = *pth_node_first;
  *addr = 0;
  *threadmap = 0;
  if(*pth_node_last){
    temp = (*pth_node_last)->prev;
    if(temp){
      temp->next = 0;
    }
    *addr = (*pth_node_last)->addr;
    *threadmap = (*pth_node_last)->threadmap;
    VG_(free)(*pth_node_last);
    *pth_node_last = temp;
  }
}

//FUNCTION CALLS TO INSTRUMENT THIGNS DURING SYSTEM CALLS

void CLG_(new_mem_startup) ( Addr start_a, SizeT len,
			     Bool rr, Bool ww, Bool xx, ULong di_handle )
{
  Context *cxt;
  /* Ignore permissions */
  cxt = CLG_(current_state).cxt;
  if(CLG_(clo).drw_syscall){
    if(cxt){
      if(CLG_(clo).drw_debugtrace){
	VG_(printf)("DEBUG TRACE HAS BEEN TURNED ON. NO INSTRUMENTATION WILL OCCUR. A MEMORY TRACE WILL BE PRINTED\n");
	VG_(printf)("=  new startup mem: %x..%x, %d, function: %s\n", start_a, start_a+len,len, cxt->fn[0]->name);
      }
      CLG_(new_mem_mmap) (start_a, len, rr, ww, xx, di_handle);
    }
    else{
      if(CLG_(clo).drw_debugtrace){
	VG_(printf)("DEBUG TRACE HAS BEEN TURNED ON. NO INSTRUMENTATION WILL OCCUR. A MEMORY TRACE WILL BE PRINTED\n");
	VG_(printf)("=  new startup mem: %x..%x, %d, no function\n", start_a, start_a+len,len);
      }
      CLG_(storeIDRWcontext)( NULL, len, start_a, 1, 5); //Put it down as a memory write for the current function
    }
  }
  //  VG_(printf)("=  new startup mem: rr = %s, ww = %s, xx = %s\n",rr ? "True" : "False", ww ? "True" : "False", xx ? "True" : "False");
  //rx_new_mem("startup", make_lazy( RX_(specials)[SpStartup] ), start_a, len, False);

}

void CLG_(new_mem_mmap) ( Addr a, SizeT len, Bool rr, Bool ww, Bool xx, ULong di_handle )
{  
   // (Ignore permissions)
   // Make it look like any other syscall that outputs to memory
   
/*   CLG_(last_mem_write_addr) = a; */
/*   CLG_(last_mem_write_len)  = len; */
  //VG_(printf)("=  new mmap mem: %x..%x, %d, function: %s\n", a, a+len,len, CLG_(current_state).cxt->fn[0]->name);
/*   VG_(printf)("=  new mmap mem: rr = %s, ww = %s, xx = %s\n",rr ? "True" : "False", ww ? "True" : "False", xx ? "True" : "False"); */
  if(CLG_(clo).drw_syscall){
    CLG_(storeIDRWcontext)( NULL, len, a, 1, 2); //Put it down as a memory write for the current function
  }
}

void CLG_(new_mem_brk) ( Addr a, SizeT len, ThreadId tid )
{
  //rx_new_mem("brk",  RX_(specials)[SpBrk], a, len, False);
  //  VG_(printf)("=  new brk mem: %x..%x, %d, function: %s\n", a, a+len,len, CLG_(current_state).cxt->fn[0]->name);
  //VG_(printf)("=  new brk mem: rr = %s, ww = %s, xx = %s\n",rr ? "True" : "False", ww ? "True" : "False", xx ? "True" : "False");
}

// This may be called 0, 1 or several times for each syscall, depending on
// how many memory args it has.  For each memory arg, we record all the
// relevant information, including the actual words referenced.
static 
void rx_pre_mem_read_common(CorePart part, Bool is_asciiz, Addr a, UInt len)
{
  Context *cxt;
  /* Ignore permissions */
  cxt = CLG_(current_state).cxt;
  if(CLG_(clo).drw_syscall){
    if(cxt){
      //VG_(printf)("=  pre mem read %s: %x..%x, %d, function: %s\n", is_asciiz ? "asciiz":"normal", a, a+len,len, cxt->fn[0]->name);
      //Insert into a temp array and pop when doing the syscall if a syscall is not already underway
      if(-1 == CLG_(current_syscall)){
	CLG_(syscall_addrchunks)[CLG_(syscall_addrchunk_idx)].first = a;
	CLG_(syscall_addrchunks)[CLG_(syscall_addrchunk_idx)].last = a + len - 1;
	CLG_(syscall_addrchunks)[CLG_(syscall_addrchunk_idx)].original = 0;
	CLG_(syscall_addrchunk_idx)++;
      }
      else{
	CLG_(storeIDRWcontext)( NULL, len, a, 0, 1); //Put it down as a memory read for the current function
      }
    }
    else{
      //    VG_(printf)("=  pre mem read %s: %x..%x, %d, no function\n", is_asciiz ? "asciiz":"normal", a, a+len,len);
    }
  }
}

void CLG_(pre_mem_read_asciiz)(CorePart part, ThreadId tid, const HChar* s, Addr a)
{
   UInt len = VG_(strlen)( (HChar*)a );
   // Nb: no +1 for '\0' -- we don't want to print it on the graph
   rx_pre_mem_read_common(part, /*is_asciiz*/True, a, len);
}

void CLG_(pre_mem_read)(CorePart part, ThreadId tid, const HChar* s, Addr a, SizeT len)
{
  rx_pre_mem_read_common(part, /*is_asciiz*/False, a, len);
}

void CLG_(pre_mem_write)(CorePart part, ThreadId tid, const HChar* s, 
                             Addr a, SizeT len)
{
  //VG_(printf)("=  pre mem write: %x..%x, %d, function: %s\n", a, a+len,len, CLG_(current_state).cxt->fn[0]->name);
}

//I read somewhere that post functions are not called if the syscall fails or returns an errorl.
void CLG_(post_mem_write)(CorePart part, ThreadId tid,
			     Addr a, SizeT len)
{
   if (-1 != CLG_(current_syscall)) {

/*       // AFAIK, no syscalls write more than one block of memory;  check this */
/*       if (INVALID_TEMPREG != CLG_(last_mem_write_addr)) { */
/*          VG_(printf)("sys# = %d\n", CLG_(current_syscall)); */
/*          VG_(skin_panic)("can't handle syscalls that write more than one block (eg. readv()), sorry"); */
/*       } */
/*       tl_assert(INVALID_TEMPREG == CLG_(last_mem_write_len)); */

/*       CLG_(last_mem_write_addr) = a; */
/*       CLG_(last_mem_write_len)  = len; */
     //VG_(printf)("=  post mem write: %x..%x, %d, function: %s\n", a, a+len,len, CLG_(current_state).cxt->fn[0]->name);
     if(CLG_(clo).drw_syscall){
       CLG_(storeIDRWcontext)( NULL, len, a, 1, 2); //Put it down as a memory write for the current function
     }
   }
}

//FUNCTION REPLACEMENT

/* //First test */
/* ret_ty I_WRAP_SONAME_FNNAME_ZZ(VG_Z_LIBPTHREAD_SONAME,f)(args) */
/*    PTH_FUNC(int, pthreadZucreateZAZa, // pthread_create@* */
/*                  pthread_t *thread, const pthread_attr_t *attr, */
/*                  void *(*start) (void *), void *arg) { */
/*       return pthread_create_WRK(thread, attr, start, arg); */
/*    } */
/* int I_WRAP_SONAME_FNNAME_ZZ(VG_Z_LIBPTHREAD_SONAME,f) */
/* Done with FUNCTION CALLS - inserted by Sid */
