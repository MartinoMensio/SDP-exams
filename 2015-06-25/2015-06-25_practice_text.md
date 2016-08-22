# Practice part 25/06/2015

Write a concurrent C program `concurrent_file_processing.c` in the Unix environment which takes from the command line an argument `n`, which must be an even integer, and generates *n* `A_threads` and *n* `B_threads`.

These threads perform the same task, but belong to two different types.  
The synchronization among the threads follows these specifications:

* The main thread generates all the other threads, then it terminates.
* All the threads run concurrently, and are not cyclic.
* Then A_threads are created with an associated identifier (0 to n-1).
* Then B_threads are created with an associated identifier (0 to n-1).
* Each thread sleeps a random number of seconds (max 3), then it is supposed to process a file identified by the thread identifier, but in our case it does nothing.
* When a pair of threads of type A has processed their "files", one of them (the last) must concatenate the two files. In our case it simply prints for example: `A4 cats: A4 A8`
* When a pair of threads of type B has processed their “files”, one of them (the last) must concatenate the two file, in our case it simply prints for example: `B5 cats B5 BO`
* When a pair of A_threads and a pair of B_threads have completed their concatenate operation, one of them (the last) must combine the four file. In our case it simply prints for example: `A1 merges A1 A4 B3 B4`

This is an example of output for the command `concurrent_file_processing 12`

```
A9 cats A9 A4
B3 cats B3 B1
                B3 merges B3 B1 A4 A9
A2 cats A2 A10
B7 cats B7 B6
                B7 merges B7 B6 A2 A10
B9 cats B9 B8
A5 cats A5 A3
                A5 merges A5 A3 B8 B9
A7 cats A7 A6
A11 cats A11 A8
B0 cats B0 B11
                B0 merges B0 B11 A6 A7
B4 cats B4 B2
                B4 merges B4 B2 A8 A11
B10 cats B10 B5
A0 cats A0 A1
                A0 merges A0 Al B5 B10
```

Hint:

* Use an array of counters with one counter per each `A_thread`.
* Use an array of counters with one counter per each `B_thread`.
* Manage these counters to get your solution.