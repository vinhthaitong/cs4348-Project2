#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <new>
#include <random>
#include <system_error>
#include <thread>
#include <vector>

using namespace std;

class PetersonLock {
    atomic<bool> interested[2] = {false, false};
    atomic<int> turn{0};

public:
    // The two participants must consistently use different IDs: 0 and 1.
    void acquire(int id) {
        int other = 1 - id;
        interested[id].store(true);
        turn.store(other);
        while (interested[other].load() && turn.load() == other) {
            this_thread::yield();
        }
    }

    void release(int id) {
        interested[id].store(false);
    }
};

class TournamentLock {
    static size_t next_power_of_two(int n) {
        size_t count = 2;
        while (count < static_cast<size_t>(n)) {
            count *= 2;
        }
        return count;
    }

    size_t leaves;
    vector<PetersonLock> nodes;

public:
    explicit TournamentLock(int n)
        : leaves(next_power_of_two(n)), nodes(leaves) {}

    // Global thread id maps to leaf leaves + id; node 1 is the root.
    void acquire(int id) {
        for (size_t child = leaves + id; child > 1; child /= 2) {
            nodes[child / 2].acquire(static_cast<int>(child % 2));
        }
    }

    // Read id's path from its highest bit to release root toward leaf.
    void release(int id) {
        size_t node = 1;
        for (size_t bit = leaves / 2; bit > 0; bit /= 2) {
            int side = (static_cast<size_t>(id) & bit) ? 1 : 0;
            nodes[node].release(side);
            node = 2 * node + side;
        }
    }
};

void event(const char* solution, int id, int round, const char* name) {
    // One output operation emits the entire record.
    printf("%s %d %d %s\n", solution, id, round, name);
}

void random_delay(mt19937& generator) {
    uniform_int_distribution<int> milliseconds(1, 500);
    this_thread::sleep_for(
        chrono::milliseconds(milliseconds(generator)));
}

void philosopher(int id, int n, bool coarse, unsigned int seed,
                 TournamentLock& table, vector<PetersonLock>& chopsticks) {
    const char* solution = coarse ? "COARSE" : "FINE";
    mt19937 generator(seed); // Each thread owns its random-number state.
    int first = (id == 0) ? n - 1 : id - 1;
    int second = id;
    if (first > second) {
        int temporary = first;
        first = second;
        second = temporary;
    }

    for (int round = 0; round < 5; ++round) {
        event(solution, id, round, "THINK_START");
        random_delay(generator);
        event(solution, id, round, "THINK_END");

        if (coarse) {
            table.acquire(id);
        } else {
            // Chopstick c: philosopher c uses 0; (c + 1) % n uses 1.
            chopsticks[first].acquire(id == first ? 0 : 1);
            chopsticks[second].acquire(id == second ? 0 : 1);
        }

        event(solution, id, round, "EAT_START");
        random_delay(generator);
        event(solution, id, round, "EAT_END");

        if (coarse) {
            table.release(id);
        } else {
            chopsticks[second].release(id == second ? 0 : 1);
            chopsticks[first].release(id == first ? 0 : 1);
        }
    }
}

void run_solution(int n, bool coarse, TournamentLock& table,
                  vector<PetersonLock>& chopsticks) {
    const char* solution = coarse ? "COARSE" : "FINE";
    vector<thread> threads;
    threads.reserve(n);
    // 0 = waiting, 1 = run, 2 = abort after a thread-creation failure.
    atomic<int> start{0};
    unsigned int seed = static_cast<unsigned int>(
        chrono::steady_clock::now().time_since_epoch().count());

    printf("BEGIN %s\n", solution);
    try {
        for (int id = 0; id < n; ++id) {
            threads.emplace_back([&, id] {
                int state;
                while ((state = start.load()) == 0) {
                    this_thread::yield();
                }
                if (state == 1) {
                    philosopher(id, n, coarse, seed + static_cast<unsigned int>(id),
                                table, chopsticks);
                }
            });
        }
    } catch (...) {
        start.store(2);
        for (thread& thread : threads) {
            thread.join();
        }
        throw;
    }

    start.store(1);
    for (thread& thread : threads) {
        thread.join();
    }
    printf("END %s\n", solution);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s n (integer n >= 2)\n", argv[0]);
        return 1;
    }

    int n = 0;
    const char* end = argv[1] + strlen(argv[1]);
    auto parsed = from_chars(argv[1], end, n);
    if (parsed.ec != errc{} || parsed.ptr != end || n < 2) {
        fprintf(stderr, "Error: n must be a representable integer >= 2.\n");
        return 1;
    }

    try {
        TournamentLock table(n);
        vector<PetersonLock> chopsticks(n);
        run_solution(n, true, table, chopsticks);
        run_solution(n, false, table, chopsticks);
    } catch (const bad_alloc&) {
        fprintf(stderr, "Error: not enough memory for the requested philosophers.\n");
        return 1;
    } catch (const system_error& error) {
        fprintf(stderr, "Error: thread operation failed: %s\n", error.what());
        return 1;
    } catch (const exception& error) {
        fprintf(stderr, "Error: %s\n", error.what());
        return 1;
    }
    return 0;
}
