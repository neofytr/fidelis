// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FIDELIS_ENGINE_DECODER_FACTORY_HPP
#define FIDELIS_ENGINE_DECODER_FACTORY_HPP

#include <fidelis/engine/decoder.hpp>
#include <fidelis/engine/error.hpp>

#include <expected>
#include <filesystem>
#include <memory>

namespace fidelis::engine {

// Sniffs the file by extension + magic bytes and returns the matching
// decoder. Mismatch (extension says X, magic says Y) returns
// FormatNotSupported with a descriptive message.
std::expected<std::unique_ptr<IDecoder>, Error>
open_decoder(const std::filesystem::path& path);

} // namespace fidelis::engine

#endif
