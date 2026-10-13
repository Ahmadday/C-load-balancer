# Load Balancer Simulation (C++)

A multithreaded simulation of a load balancer distributing traffic across a
fleet of backend servers, comparing five real-world load balancing
algorithms under identical traffic conditions.

Each backend server runs on its own real thread with its own request queue
(mutex + condition variable), so this isn't a discrete-event simulation —
it's an actual concurrent system, just with simulated network/processing
costs (`std::this_thread::sleep_for`) instead of real sockets.

## Algorithms implemented

| Strategy | How it picks a server |
|---|---|
| **Round Robin** | Cycles through servers in fixed order. Ignores load and capacity. |
| **Weighted Round Robin** | Smooth weighted round robin (the algorithm nginx uses) — higher-capacity servers get picked proportionally more, spread evenly over time rather than in bursts. |
| **Least Connections** | Picks the server with the fewest requests currently in flight. |
| **Least Response Time** | Picks the server with the lowest recent average response time (tracked via exponential moving average). |
| **IP Hash** | Hashes a simulated client/session id so the same client always lands on the same server (session affinity / sticky sessions). |

## Architecture

```
RequestGenerator --> LoadBalancer --> Strategy.selectServer() --> Server (own thread, own queue)
                                                                        |
                                                                        v
                                                                    Metrics (thread-safe)
```

- **`Server`** — a worker thread pulling from its own queue. Tracks active
  connection count and an EMA of response time, both read (lock-free,
  atomics) by strategies without touching the queue's mutex.
- **`IStrategy`** — pluggable interface; each algorithm above is a small
  class implementing `selectServer`.
- **`LoadBalancer`** — glues servers + a strategy together; `dispatch()`
  just asks the strategy who should get the request.
- **`RequestGenerator`** — produces traffic as a **Poisson process**
  (exponentially distributed inter-arrival times), which is the standard
  model for independent random arrivals like real user traffic. Work cost
  per request is drawn from a normal distribution.
- **`Metrics`** — thread-safe collector. Servers report into it from their
  own threads on every completion. Computes average/p95/p99 latency,
  queueing delay, throughput, and **Jain's fairness index** (how evenly load
  actually spread across servers; 1.0 = perfectly even).

To make the comparison meaningful, the fleet is **heterogeneous** — servers
have different speed multipliers (`1.0, 1.0, 2.0, 0.5, 1.5`) simulating
different hardware capacity. Every strategy runs against the *exact same*
generated traffic (fixed random seed) so results are directly comparable.

## Build & run

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
./lbsim [totalRequestsPerStrategy] [arrivalRatePerSec] [--live]

# e.g. 3000 requests per strategy at 150 req/s
./lbsim 3000 150

# with a live terminal dashboard (recommended - watch the herd effect happen)
./lbsim 2000 150 --live
```

Prints a comparison table to stdout and writes `results.csv` (in whatever
directory you ran it from) for further analysis or plotting.

### Live dashboard

`--live` runs each strategy with a self-updating terminal view (redraws in
place via ANSI cursor codes, no external deps) instead of running silently:

```
Live: Least Response Time
Progress [##############................]  980/2000 (49.0%)   4.2s   233.1 req/s

ID   Speed  Active  Queue  Done    Load (relative to busiest server)
S0   1.0x   0       0      41      [####......................]
S1   1.0x   1       0      38      [###.......................]
S2   2.0x   3       12     812     [#########################]
S3   0.5x   0       0      44      [####......................]
S4   1.5x   0       0      45      [####......................]
```

Watching this in real time makes the Least-Response-Time herd effect (see
below) much more visceral than reading it off a final table — you can see
server 2's bar pull away from the others within the first second or two of
traffic.

## A finding worth noting

In test runs, **Least Response Time collapses** under this traffic pattern —
the fast server (2x speed) ends up handling ~80-90% of all requests while
others sit nearly idle. This isn't a bug; it's a well-known real failure
mode of naive least-response-time balancing: the EMA lags reality, so while
many concurrent requests are in flight, they all still see "server 2 looked
fastest a moment ago" and pile on before the feedback catches up — a herd
effect. **Weighted Round Robin**, which doesn't depend on live feedback at
all, was consistently the most stable performer in this setup.

A natural extension: implement **"Power of Two Choices"** (sample two random
servers, pick the less-loaded of the two) — a well-known fix for exactly
this herding problem, and cheaper than checking every server.

## Possible extensions

- Power of Two Choices strategy (fixes the herding issue above)
- Simulated server failures / health checks and failover
- Circuit breaker / retry logic
- Real TCP sockets instead of in-process queues
- Live terminal dashboard instead of end-of-run summary
- `plot_results.py` (included) for bar-chart visualizations of `results.csv`

## Files

```
include/        Header files (one class per file)
src/            Implementations + main.cpp (runs all 5 strategies, prints comparison)
CMakeLists.txt  Build config (C++17, requires only pthreads)
plot_results.py Optional: matplotlib bar charts from results.csv
```
