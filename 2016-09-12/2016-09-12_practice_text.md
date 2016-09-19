# Programming part 12/09/2016

Implement a program in concurrent C language using Pthreads and semaphores that creates **two** threads.

The first thread loops generating `k` times (`k` is given as argument in command line) a new thread that corresponds to an atom of Sodium (Na).

The creation requires a random times in the range `[0-4]` seconds. These threads are identified by numbers in range `0` to `k-1`. The second thread has the same behaviour of the first one, but it generates atoms of Chlorine (Cl). These threads are identified by numbers in range `k` to `2*k-1`.

When a Na atom (thread) is created, it must wait that at least one Cl atom exists. In this case, it can proceed to the combination of the two atoms, which simply consist in the thread printing its identifier and the identifier of the other combining thread, then the thread terminates.

A Cl thread has the same behaviour of a Na thread.

This is an example of the output of the program execution with `k=4`.

```text
id: 0 - Na 0 Cl 6    id: 6 - Na 0 Cl 6
id: 1 - Na 1 Cl 7    id: 7 - Na 1 Cl 7
id: 2 - Na 2 Cl 5    id: 5 - Na 2 Cl 5
id: 3 - Na 3 Cl 4    id: 4 - Na 3 Cl 4
```

Please notice that both the Na and the Cl threads print their identifiers with the same format: the Na identifier first.

Hints: manage two parallel arrays of the identifier of the threads that have produced their atoms, and synchronize the print operations of each pair of Na and Cl threads.
