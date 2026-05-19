// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FIDELIS_ENGINE_DECODER_INTERNAL_HPP
#define FIDELIS_ENGINE_DECODER_INTERNAL_HPP

#include <fidelis/engine/decoder.hpp>
#include <fidelis/engine/error.hpp>

#include <expected>
#include <filesystem>
#include <memory>

// Internal header: per-format factories. detect.cpp picks one of these.
namespace fidelis::engine {

std::expected<std::unique_ptr<IDecoder>, Error>
open_wav_decoder(const std::filesystem::path& path);

std::expected<std::unique_ptr<IDecoder>, Error>
open_aiff_decoder(const std::filesystem::path& path);

std::expected<std::unique_ptr<IDecoder>, Error>
open_flac_decoder(const std::filesystem::path& path);

std::expected<std::unique_ptr<IDecoder>, Error>
open_alac_decoder(const std::filesystem::path& path);

std::expected<std::unique_ptr<IDecoder>, Error>
open_mp3_decoder(const std::filesystem::path& path);

std::expected<std::unique_ptr<IDecoder>, Error>
open_vorbis_decoder(const std::filesystem::path& path);

std::expected<std::unique_ptr<IDecoder>, Error>
open_opus_decoder(const std::filesystem::path& path);

} // namespace fidelis::engine

#endif
