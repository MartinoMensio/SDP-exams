# UNIX

## Point 1

### Question

(3.0 marks) Which are the Unix commands for installing a new module, for listing the installed modules,
for removing modules? Standard IO dunctions, such as **printf** cannot be used in module programs, why?
Which function can be used instead? In which file does the output of a module go?

### Solution

* `insmod mymodule.ko` installs a new module corresponding to the file *mymodule.ko*
* `lsmod` lists the currently installed modules
* `rmmod mymodule` removes the module named *mymodule* from the installed ones

Standard I/O functions can't be used because:

* The code is executed in the memory space of the kernel, and therefore the standard I/O functions are not loaded
* The modules don't have standard handles (stdin, stdout, stderr) available because they don't have a terminal attached
* Modules are not designed to perform I/O on a terminal

The only output a module can have is the file var/log/messages that can be written by a module by using
the function **printk**. This file can be read by using normal editors or by using the command `dmesg`.
A module message written by using the **printk** function could appear on the current tty (not a pts) if
the warning level used is high.

## Point 2

### Question

(3.0 marks) Given this reference string:

`112233444412441221444322555543`

Compute the **page fault frequency and the mean resident set** for the **VMIN strategy**
with control parameter t=3. The VMIN strategy is similar to the Working-set strategy, but it looks
forward in time in the reference string rather than backward as the Working-set strategy does.

### Solution

VMIN strategy is described [on this page](http://www.ics.uci.edu/~bic/courses/NUS-OS/Lectures-on-line/ch08.pptx)
at page 28.

| Time           | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10| 11| 12| 13| 14| 15| 16| 17| 18| 19| 20| 21| 22| 23| 24| 25| 26| 27| 28| 29|
|:--------------:|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **String**     | 1 | 1 | 2 | 2 | 3 | 3 | 4 | 4 | 4 | 4 | 1 | 2 | 4 | 4 | 1 | 2 | 2 | 1 | 4 | 4 | 4 | 3 | 2 | 2 | 5 | 5 | 5 | 4 | 3 |
|                |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
|                | 1 | 1 | 2 | 2 | 3 | 3 | 4 | 4 | 4 | 4 | 4 | 4 | 4 | 4 |   |   |   |   | 4 | 4 | 4 | 3 | 2 | 2 | 5 | 5 | 5 | 4 | 3 |
|                |   |   |   |   |   |   |   |   |   | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |   |   |   |   |   |   |   |   |   |   |   |
|                |   |   |   |   |   |   |   |   |   |   | 2 | 2 | 2 | 2 | 2 | 2 | 2 |   |   |   |   |   |   |   |   |   |   |   |   |
| **Page Fault** | x |   | x |   | x |   | x |   |   | x | x |   |   |   |   |   |   |   | x |   |   | x | x |   | x |   |   | x | x |

Page fault rate = 12/29 = 41%

Mean Resident Set = 39/29 = 1.34

---

# WINDOWS

## Point 1

### Question
(2.0 marks) Briefly describe similarities and differences between WIN32 critical sections and mutexes.
Can critical sections be used to provide mutual exclusion between two threads? And two processes? *(motivate the answer)*

### Solution
TODO

## Point 2

### Question

(3.0 marks) Explain the role of filter expressions in `try...except` blocks. Why/when do the following block call the Filter functions?

```c
__try {
  __try {
    // statements
    ...
  } __except(Filter1(GetExceptionCode()));
} __except(Filter2(GetExceptionCode()));
```

Are `Filter1` and `Filter2` system routines or user-generated functions?  
What is the role of `GetExceptionCode()` ? Under which conditions is `Filter2` called?  
Is `Filter2` called when `Filter1` return `EXCEPTION_CONTINUE_EXECUTION` ? *(motivate all yes/no answers)*

### Solution

TODO

## Point 3

### Question

(4.0 marks) Explain the role of completion functions in extended (alertable) asynchronous IO.  
A given file contains a sequence of NR fixed length records that can be described by the following C struct:

```c
typedef struct {
  data_t value;
  int next;
}
```

Type `data_t` is given (another struct). Records are numbered from 0 to NR-1, and they are organized in circular
linked lists, i.e. each record has a next field, containing the index of another record in the file. A very simple
file could contain the sequence: `{v0,3}, {v1,2}, {v3,5}, {v4,0}, {v5,4}, {v6,6}, {v7,6}`. `(vi,j)` means that the
i-th record has a value field `vi` and a next index `j`. The file has 8 records organized in three circular lists
(0-3-5-4, 1-2, 6-7).  
Function `countCircularList` is in charge of counting the length of a circular list, given the file handle and the
index of one of the records in the list. Write the missing part of `countCircularList` and of the completion routine
`readDone` in order to achieve the counting task (use of global variables is allowed).

```c
DWORD countCircularList (HANDLE hFile, DWORD start) {
  ...
  ReadFileEx(hFile, ..., readDone);
  ...
}

VOID WINAPI readDone(DWORD Code, DWORD nBytes, LPOVERLAPPED pOv) {
  ...
  ReadFileEx(hFile, ..., readDone);
  ...
}
```

### Solution

TODO