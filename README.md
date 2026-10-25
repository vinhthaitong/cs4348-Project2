# CS 4348 Project 2: Dining Philosophers

Team: 17  
Members: Hoa Doan and Thai Tong (confirm full names before submission)

## Build and run

Requires a C++17 compiler and POSIX thread support.

```sh
make
./my-dining 5
make clean
```

`make` runs `g++ -std=c++17 -pthread` with compiler warnings enabled. The program
accepts one integer argument, `n >= 2`, runs COARSE followed by FINE, and joins
every philosopher thread before ending each solution. Each philosopher completes
five thinking/eating cycles. Every activity draws a new delay from 1 to 500 ms
using that thread's own random generator. Output contains only the required
BEGIN/END markers and four-field activity records, each emitted by one `printf`.

## Lock design and demonstration notes

- **Peterson:** Each participant sets its interest flag, gives the other side
  priority through `turn`, and waits while the other side is interested and has
  priority. Release clears its interest flag. All shared Peterson variables are
  atomic and use the default sequentially consistent ordering, which preserves
  the ordering required by the algorithm.
- **Tournament tree:** Round `n` up to a power of two, `L`. Internal nodes are
  array positions `1` through `L - 1`; position `0` is unused. Thread `i` owns
  conceptual leaf `L + i`. Remaining leaves have no thread. An even child uses
  Peterson participant 0 at its parent; an odd child uses participant 1.
  Acquire repeatedly locks the parent and moves upward to the root. Release
  follows the bits of `i` from the root downward, reversing that same path.
  Lower locks stay held while competing at higher nodes, so only one thread
  per subtree can represent a side of a higher Peterson lock.
- **Coarse:** A philosopher holds the entire Tournament-Tree lock while eating.
  Consequently, only one philosopher can eat at any time.
- **Fine:** Philosopher `i` needs chopsticks `(i + n - 1) % n` and `i`, since
  chopstick `c` lies between philosophers `c` and `(c + 1) % n`. At chopstick
  `c`, philosopher `c` always uses local participant 0 and its neighbor always
  uses 1. Both locks remain held through EAT_END and are released in reverse
  acquisition order. Neighbors therefore cannot eat together, while
  philosophers with disjoint chopsticks can eat concurrently.
- **Deadlock prevention:** Always acquire the lower-numbered chopstick first.
  A philosopher waiting for its second chopstick holds a smaller-numbered one.
  A circular chain would require strictly increasing numbers to return to its
  starting number, which is impossible. For `n = 2`, both philosophers acquire
  chopstick 0 before chopstick 1, using distinct participant IDs at each lock.
- **Failure handling:** Arguments are parsed as complete decimal integers.
  Allocation and thread-creation errors produce stderr messages and a nonzero
  status. Newly created threads wait on an atomic startup flag. If creation
  fails, the main thread changes the flag to abort, joins every started thread,
  and reports the error. Otherwise it releases all threads to run.

## Assumptions and limitations

The scheduler must eventually run waiting threads. These are busy-wait locks;
`yield()` gives other runnable threads a chance to execute, but supplies no
fixed time limit or scheduling guarantee. Runtime grows with `n`, especially
when coarse locking serializes eating. The number of philosophers is limited
by integer representation, available memory, and operating-system thread limits.
Random seeds come from the clock and thread ID; these delays are simulations,
not cryptographic randomness. A resource failure may leave a partial trace
without an END marker; stderr and the nonzero exit status indicate failure.
