// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FIDELIS_ENGINE_FORMAT_MATCH_HPP
#define FIDELIS_ENGINE_FORMAT_MATCH_HPP

#include <fidelis/engine/error.hpp>
#include <fidelis/engine/format.hpp>

#include <expected>

namespace fidelis::engine {

// Returns the file's format unchanged iff every component is in the device's
// accepted set. Refuses with FormatNotSupported otherwise. No coercion, no
// rounding, no nearest-anything.
std::expected<PcmFormat, Error> match(const PcmFormat& file, const DeviceCaps& dev);

} // namespace fidelis::engine

#endif
