# Microstructure Signals

This project implements a variety of microstructure‑driven signals used for research and trading:

- **Order Imbalance**: The difference between total bid and ask volume over the top N price levels, normalised by total volume.  Positive values indicate buying pressure.  Weighted and count‑based variants are provided.
- **Microprice**: A size‑weighted mid price around the touch, implemented by `OrderBook::getMicroPrice`.  When size weighting is off it falls back to the simple mid【690010940282616†screenshot】.
- **Spread z‑score**: Compares the current bid–ask spread to its moving average and standard deviation to identify regimes where spreads are unusually wide or tight.
- **Trade Flow**: A decayed difference between aggressive buy and sell volume, with an accompanying VWAP metric.
- **Queue Metrics**: Estimates queue position ahead, expected fill time and fill probability based on the order book depth and a simple fill rate model.