# plutus
advanced high-performance matching engine for financial trading systems

plutus, named after the the [greek god of wealth](https://en.wikipedia.org/wiki/Plutus), is a robust high-performance matching engine, similar
to those found in stock exchanges today. it's loosely inspired by the [Jane Street](https://www.youtube.com/watch?v=b1e4t2k2KJY)
talk on building exchanges, and extends it with a more modern and advanced architecture for real-time order
matching, risk management, and trade execution.

plutus supports a wide range of trading scenarios and incorporates advanced order types (limit, market, stop-loss,
iceberg), time-in-force (GTC, IOC, FOK) options, and symbol-specific configurations. i tried to emphasize
low-latency and high-throughput processing through efficient data structures and a multithreaded API to mimic
how production systems work from what I could gather from various blog posts, articles, papers, and discussions.

at its core, plutus is a matching engine that processes, validates, and matches incoming orders
against the order book, with support for partial fills, self-trade prevention, and other advanced handling.
the order book maintains structures for bids and asks and the event loop manages the API with non-blocking I/O
for connections and sockets. it uses a volume-weighted average price algorithm to calculate the average price of
a symbol over a period of time.

the client communicates with the server with a rudimentary text-based protocol. eg:
```
ADD|1|1640995200000|1001|AAPL|150.25|10|BUY|GTC|LIMIT|123|0|0
```

i tried to comment parts i found more difficult and write self-documenting code to make it easier to navigate
and learn, especially to simplify the design choices i made. `make` builds the entire project with c++20 with
only a `kqueue` dependency for macos. the build outputs a binary which launches a server. 
