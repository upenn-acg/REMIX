# REMIX: Online Detection and Repair of Cache Contention for the JVM
## Ariel Eizenberg (1), Shiliang Hu (2), Gilles Pokam (3), Joseph Devietti (4)
### (1), (4) - University of Pennsylvania, (2), (3) - Intel 
### Published in PLDI 2016

As ever more computation shifts onto multicore architectures,
it is increasingly critical to find effective ways of dealing
with multithreaded performance bugs like true and false
sharing. Previous approaches to fixing false sharing in unmanaged
languages have employed highly-invasive runtime
program modifications. We observe that managed language
runtimes, with garbage collection and JIT code compilation,
present unique opportunities to repair such bugs directly,
mirroring the techniques used in manual repairs.
We present REMIX, a modified version of the Oracle
HotSpot JVM which can detect cache contention bugs
and repair false sharing at runtime. REMIX’s detection
mechanism leverages recent performance counter improvements
on Intel platforms, which allow for precise, unobtrusive
monitoring of cache contention at the hardware
level. REMIX can detect and repair known false sharing issues
in the LMAX Disruptor high-performance inter-thread
messaging library and the Spring Reactor event-processing
framework, automatically providing 1.5-2x speedups over
unoptimized code and matching the performance of handoptimization.
REMIX also finds a new false sharing bug
in SPECjvm2008, and uncovers a true sharing bug in the
HotSpot JVM that, when fixed, improves the performance
of three NAS Parallel Benchmarks by 7-25x. REMIX incurs
no statistically-significant performance overhead on other
benchmarks that do not exhibit cache contention, making
REMIX practical for always-on use.

A tutorial and usage instructions are available at (http://acg.cis.upenn.edu/wiki/index.php?n=Projects.REMIX).

