# Programming part 01/07/2016

An office automation system is organized as represented by the following picture.

![2016-07-01_practice_drawing.svg](https://cdn.rawgit.com/MartinoMensio/SDP-exams/master/2016-07-01/2016-07-01_practice_drawing.svg "System representation")

The Windows 32 system simulating the system is composed by:

* `N` input files `FileIi` and `N` output files `FileOi`,
* `N` threads `TAi`, `N` threads `TBi` and `N` threads `TCi`,
* `N` FIFO queue `QueueAi` and `N` FIFO queue `QueueBi`,

where `i = 1, ..., N`.
Every input file `FileIi` (with `i = 1, ..., N`) stores, in binary form, an undefined number of records. Each record includes a single 32-bit integer value `n` and a sequence of `n` characters.

Each one of the `N` threads `TAi` (with `i = 1, ..., N`) proceeds as follows:

* It awaits for a random number of seconds (from 1 to 10).
* It reads from one of the input files `FileIi` (randomly selected) the next (not yet read) record.
* It manipulates the record string by erasing all non alphabetic characters.  
For Example the record `23 123ab;-CaAbB56bcC??c(C)` will become `12 abCaAbBbcCcC`
* It awaits for a random number of seconds (from 1 to 10).
* It enqueues the same data read from the input file (but with the string manipulated) in a randomly selected `QueueA` FIFO queue.

Each thread `TB1` (with `i = 1, ..., N`), behaves **exactly** like the thread `TAi` with three main differences:

* It reads its input from a randomly selected `QueueAi` queue (and not from a file).
* It manipulated the record string by transforming all lower case letters into upper case letters.  
For example the record `12 abCaAbBbcCcC` will become `12 ABCAABBBCCCC`.
* It enqueues its output data on a randomly selected `QueueBi` FIFO queue.

Each thread `TC1` (with `i = 1, ..., N`), behaves **exactly** like the thread `TAi` with three main differences:

* It reads its input from a randomly selected `QueueBi` queue (and not from a file).
* It manipulated the record string by ordering all letters in ascending order.  
For example the record `12 ABCAABBBCCCC` will become `12 AAABBBBCCCCC`.
* It enqueues its output data on a randomly selected `FileOi` file (and not to a queue).

The integer value `N` is received by the application on the command line. Input and output files have pre-defined names as indicated (i.e., `FileIi` and `FileOi` where `i = 1, ..., N`). The application has to synchronize all threads and perform all file and queue accesses in a proper way, i.e., with mutual exclusion whenever necessary. When all input files `FileIi` have been manipulated all the threads `TAi`, `TBi` and `TCi` have to stop following a proper and clean procedure.

The way in which the program is organized, modularized and presented will be subject to evaluation. Concurrency has to be maximized.
