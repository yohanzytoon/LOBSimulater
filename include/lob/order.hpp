#pragma once

// Thin wrapper that includes the order book header so that users can
// include `lob/order.hpp` to obtain the Order and Execution types.
#include "lob/order_book.hpp"

namespace lob {
// No additional declarations.  This header simply re‑exports the
// definitions of Order and Execution from order_book.hpp for
// convenience in user code.
} // namespace lob