# Programming part 15/07/2016

Write a concurrent C program using processes, pipes and signals to simulate an assembly line with **K**machine tools, **K** is given as an argument to the parent process.  
**The main process is the controller** of these machines.  
It forks a child process for each machine tool, and an additional monitor process.  
The machine tool processes are cyclic, and exit, after completing their work, when the controller has sent the **stop** command.

The controller process sends to each machine tool process the **start** command. The machine tool process decides a working time duration (random between **1** and **4** seconds). When it terminates this work, it sends to the controller its identity and the time it has spent working.

The controller process manages the start of operations, and receives notice of the operation terminations.  
In particular, the controller process sends the **start** commands to the available machine tools only after all have finished their previous work. It has also the duty of **collecting the identity and procesing times of each machine**.  
It sends the **stop** command to the available machine tool processes after **5** minutes.  
After sending the stop commands, the controller process send a termination signal to the monitor process, and exits.


The monitor process loops allowing the plant operator to ask the average duration of the operations for all the machine tools **since the previous request**, simply by typing on an terminal a `'\n'` character.  
The monitor process collects these data from the controller process, and prints on the console a line including the current fatea and the average duration of the operations for all the machine tools.

Recall that **errno** can be tested for **EINTR** to check if a **read** system call was interrupted by a signal before any data was read, e.g. when it is waiting on a pipe.