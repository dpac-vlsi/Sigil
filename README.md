#What is Sigil?

#####Overview

Sigil is a Valgrind tool designed to help the HW/SW partitioning problem.

This is a modified version of Sigil for use in the [SynchroTrace] framework.
For information on the previous version of Sigil, please see: 
https://github.com/snilakan/Sigil

#####Building Sigil

We offer a setup script to automatically detect missing package
dependencies and build the tool. 

```sh
$ cd setup
$ ./setup_sigil.sh
```

If you would rather build the tool manually, you can run the following:
   
```sh
$ cd valgrind-3.10.1
$ ./autogen.sh
$ ./configure
$ ./make check
```

#####Compiling User Programs

Prior to generating traces through executing Sigil on the user program, one must compile their program using debugging symbols (i.e. -g ). Also, the Valgrind intercept wrapper script must be linked on compilation. This wrapper script enables detecting and logging pthread calls during trace capture. The source code to this pthread wrapper is tools/wrapper.c and must first be compiled then statically linked during the compilation of the user program.

An example of compling this wrapper script follows:

```sh
$ gcc -static -Wall -g -DVGO_linux=1 -c wrapper.c -I <SIGIL PATH>/valgrind-3.10.1/include/ -I <SIGIL PATH>/valgrind-3.10.1/ -I <SIGIL PATH>/valgrind-3.10.1/callgrind -o wrapper.o
```

An example of the CC flags for compiling the user program for Sigil follows:

```sh
$ -O3 -lpthread -D_POSIX_C_SOURCE=200112 -g -static <SIGIL PATH>/tools/wrapper.o -D__USE_GNU
```

#####Running Sigil

An included script will run sigil with the most common options. Note that
the user must first edit this script with some default paths to let it know
where is the sigil directory. Make sure your binary is compiled with debug
flags.

```sh
$ ./runsigil_and_gz.py <sigil options> <user binary>
```

This results in trace files called "sigil.events.out-<thread_number>.gz"
For example if I wanted to get sigil traces for an 8-thread execution of the FFT benchmark:

```sh
$ ./runsigil_and_gz.py --fair-sched=yes --tool=callgrind --separate-callers=100 --toggle-collect=main --cache-sim=yes --dump-line=no --drw-func=no --drw-events=yes --drw-splitcomp=1 --drw-intercepts=yes --drw-syscall=no --branch-sim=yes --separate-threads=yes --callgrind-out-file=callgrind.out.threads ./FFT -m16 -p8 -l6 -t
```

After the traces are generated, a pthread meta-data file (sigil.pthread.out) must be created by using the generate_pthread_file.py script on the generated err.gz file.

```sh
$ ./generate_pthread_file.py err.gz
```

More information about running the tool and its options can be found in the
provided documentation.

#####Postprocessing

#####Restrictions

Sigil is a Valgrind tool, and as such, is 
only officially supported in LINUX.

What programs can be profiled by the tool? (Restrictions/Known issues)

As the tool incurs slowdown already, some restrictions were placed
purposefully on the user program, so that writing optimized code would be 
somewhat easier and the memory behavior of the tool is more determinate.

The restrictions are as follows:

   1. The maximum depth of the calltree in the user program (after main) 
      must not exceed the <number> - 10, specified in the
      --separate-callers=<number> option

      The default for <number> in the wrapper script is 400, but it can 
      simply be the maximum depth of the calltree in the user program + 10. 
      The additional 10 is just a buffer for the functions before main() 
      and after exit().

   2. The number of functions in the program must not exceed 10,000. 
      (hardcoded)

   3. Sigil cannot handle address values above 256G currently. 

   4. Applications with a large memory footprint can cause Sigil to exceed 
      memory bounds set by Valgrind or the system. 

      If such an error is encountered, we recommend running with smaller 
      input sets.

   5. Applications with very large call depth can also cause Sigil to exceed 
      memory bounds. Currently, there is no workaround for this, but we are 
      working on a long term solution that could mean tighter integration 
      with Callgrind.
