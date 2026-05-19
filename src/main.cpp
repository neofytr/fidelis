// SPDX-License-Identifier: GPL-3.0-or-later

#include <fidelis/config/config.hpp>
#include <fidelis/dbus/service.hpp>
#include <fidelis/engine/device.hpp>
#include <fidelis/engine/engine.hpp>
#include <fidelis/library/library.hpp>
#include <fidelis/queue/queue.hpp>
#include <fidelis/version.hpp>
#include <fidelis/web/server.hpp>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include <unistd.h>

namespace {

void print_version() {
    std::fputs("fidelis ", stdout);
    std::fwrite(fidelis::version.data(), 1, fidelis::version.size(),
                stdout);
    std::fputc('\n', stdout);
}

void print_help() {
    std::fputs(
        "usage: fidelis [OPTIONS] [FILE]\n"
        "\n"
        "Linux-native, bit-perfect player for external USB DACs.\n"
        "Serves a web UI on http://localhost:7800 and an MPRIS interface.\n"
        "\n"
        "Options:\n"
        "  --config PATH       Override config file path\n"
        "  --library-db PATH   Override library SQLite path\n"
        "  --version, -V       Print version and exit\n"
        "  --help, -h          Print this help and exit\n"
        "\n"
        "Positional FILE is loaded and played at launch.\n",
        stdout);
}

struct ParsedArgs {
    bool show_version   = false;
    bool show_help      = false;
    std::filesystem::path config_path;
    std::filesystem::path library_db_path;
    std::filesystem::path file;
    bool error = false;
    std::string error_msg;
};

bool needs_value(std::string_view a) {
    return a == "--config" || a == "--library-db";
}

ParsedArgs parse_args(int argc, char** argv) {
    ParsedArgs out;
    const std::span<char*> args{argv, static_cast<std::size_t>(argc)};
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view a{args[i]};
        if (a == "--version" || a == "-V") {
            out.show_version = true;
        } else if (a == "--help" || a == "-h") {
            out.show_help = true;
        } else if (needs_value(a)) {
            if (i + 1 >= args.size()) {
                out.error = true;
                out.error_msg = std::string(a) + " requires an argument";
                break;
            }
            const std::string_view val = args[++i];
            if (a == "--config") {
                out.config_path = val;
            } else {
                out.library_db_path = val;
            }
        } else if (a.starts_with("--config=")) {
            out.config_path = a.substr(9);
        } else if (a.starts_with("--library-db=")) {
            out.library_db_path = a.substr(13);
        } else if (!a.empty() && a[0] != '-' && out.file.empty()) {
            out.file = std::string(a);
        } else {
            out.error = true;
            out.error_msg = std::string{"unknown option: "} + std::string(a);
            break;
        }
    }
    return out;
}

namespace eng = fidelis::engine;
namespace cfg = fidelis::config;
namespace lib = fidelis::library;
namespace dbs = fidelis::dbus_svc;
namespace que = fidelis::queue;

std::string pick_device(const cfg::Config& c,
                        const std::vector<eng::DeviceInfo>& devices) {
    const std::string& pref = c.device.preferred;
    const std::string chosen = eng::select_preferred_device(pref, devices);
    // Disclose when a non-empty preference could not be honoured exactly.
    if (!pref.empty() && !chosen.empty() && chosen != pref) {
        const bool id_match = std::any_of(
            devices.begin(), devices.end(),
            [&](const eng::DeviceInfo& d) { return d.id == pref; });
        if (!id_match) {
            std::fprintf(stderr,
                         "fidelis: preferred device '%s' not present; "
                         "falling back to '%s'\n",
                         pref.c_str(), chosen.c_str());
        }
    }
    return chosen;
}

// One-time migration from the pre-rename config location. If the current
// config does not exist but a legacy ".../transporter/config.toml" sibling
// does, copy it across so existing users keep their device/library settings.
void migrate_legacy_config(const std::filesystem::path& path) {
    std::error_code ec;
    if (path.empty() || std::filesystem::exists(path, ec)) {
        return;
    }
    const auto dir = path.parent_path();
    if (dir.filename() != "fidelis") {
        return;
    }
    const std::filesystem::path legacy =
        dir.parent_path() / "transporter" / path.filename();
    if (!std::filesystem::exists(legacy, ec)) {
        return;
    }
    std::filesystem::create_directories(dir, ec);
    std::filesystem::copy_file(
        legacy, path, std::filesystem::copy_options::overwrite_existing, ec);
}

cfg::Config load_config_or_defaults(const std::filesystem::path& path) {
    if (path.empty()) {
        return cfg::Config{};
    }
    auto r = cfg::load_file(path);
    if (r) {
        return std::move(*r);
    }
    return cfg::Config{};
}

std::atomic<int> g_signal{0};

void install_signal_handlers() {
    struct sigaction sa{};
    sa.sa_flags = 0;
    sa.sa_handler = [](int sig) {
        g_signal.store(sig, std::memory_order_relaxed);
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP,  &sa, nullptr);

    // Block these in the main thread now, before any other threads are
    // created. All child threads inherit the mask. sigsuspend() in
    // wait_for_termination_signal() temporarily unblocks them only in the
    // main thread, guaranteeing kill(getpid(), sig) wakes it and not a
    // random httplib worker thread.
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGTERM);
    sigaddset(&ss, SIGINT);
    sigaddset(&ss, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &ss, nullptr);
}

void wait_for_termination_signal() {
    sigset_t mask;
    sigemptyset(&mask);
    while (g_signal.load(std::memory_order_relaxed) == 0) {
        sigsuspend(&mask);
        if (errno != EINTR) {
            break;
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    const ParsedArgs args = parse_args(argc, argv);

    if (args.error) {
        std::fprintf(stderr, "fidelis: %s\n", args.error_msg.c_str());
        print_help();
        return 64;
    }
    if (args.show_help) {
        print_help();
        return 0;
    }
    if (args.show_version) {
        print_version();
        return 0;
    }

    install_signal_handlers();

    const std::filesystem::path config_path = args.config_path.empty()
        ? cfg::default_config_path() : args.config_path;
    migrate_legacy_config(config_path);
    cfg::Config config = load_config_or_defaults(config_path);

    std::unique_ptr<eng::Engine> engine;
    std::vector<eng::DeviceInfo> devices;
    if (auto r = eng::list_playback_devices(); r) {
        devices = std::move(*r);
    }
    const std::string device_id = pick_device(config, devices);
    if (!device_id.empty()) {
        eng::EngineConfig ec{};
        ec.device_id = device_id;
        if (auto e = eng::Engine::create(std::move(ec)); e) {
            engine = std::move(*e);
        } else {
            std::fprintf(stderr, "fidelis: engine create failed: %s\n",
                         e.error().message.c_str());
        }
    } else {
        std::fprintf(stderr,
                     "fidelis: no playback device available (%zu enumerated)\n",
                     devices.size());
    }

    std::unique_ptr<lib::Library> library;
    {
        lib::Config lc{};
        lc.db_path = args.library_db_path.empty()
            ? cfg::default_library_db_path() : args.library_db_path;
        lc.roots = config.library.roots;
        lc.ignore_patterns = config.library.ignore_patterns;
        std::error_code ec;
        std::filesystem::create_directories(lc.db_path.parent_path(), ec);
        if (auto l = lib::Library::open(std::move(lc)); l) {
            library = std::move(*l);
            if (!config.library.roots.empty()) {
                library->rescan_async();
            }
        } else {
            std::fprintf(stderr, "fidelis: library open failed: %s\n",
                         l.error().message.c_str());
        }
    }

    std::unique_ptr<dbs::DbusService> dbus;
    if (config.dbus.enabled) {
        dbs::Hooks hooks;
        dbs::Config dc;
        dc.enabled = true;
        auto svc = dbs::DbusService::start(engine.get(), library.get(),
                                           std::move(hooks), dc);
        if (svc) {
            dbus = std::move(*svc);
        } else {
            std::fprintf(stderr,
                         "fidelis: MPRIS unavailable: %s\n",
                         svc.error().message.c_str());
        }
    }

    // Queue: owns track list and drives engine preload() for gapless.
    std::unique_ptr<que::Queue> queue;
    if (engine != nullptr) {
        queue = std::make_unique<que::Queue>(*engine);
        engine->set_event_callback([&](const eng::Event& ev) {
            queue->on_event(ev);
            if (dbus != nullptr &&
                ev.kind == eng::Event::Kind::TrackLoaded) {
                dbus->notify_track_loaded();
            }
        });
        if (!args.file.empty()) {
            queue->append(args.file);
        }
    }

    // Web server: REST control surface + 10 Hz telemetry WebSocket. Always
    // started, even with no engine/queue — a device that is absent or busy
    // must not take down the UI; the user needs the device picker reachable
    // to recover. engine/queue are passed as (possibly null) pointers.
    fidelis::web::WebConfig wcfg{};
    wcfg.config_path = config_path.string();
    wcfg.host = config.web.host;
    wcfg.port = config.web.port;
    wcfg.token = config.web.token;

    // Bind security policy:
    //   - Loopback host: token optional (single-machine session is trusted).
    //   - Non-loopback host: token required. If the user has not set one,
    //     auto-generate, persist, and log it. Empty token + non-loopback is
    //     a configuration error; refuse rather than expose an unauthenticated
    //     control surface to the network.
    if (!cfg::is_loopback_host(wcfg.host) && wcfg.token.empty()) {
        const std::string gen = cfg::generate_token();
        if (gen.empty()) {
            std::fprintf(stderr,
                         "fidelis: refusing to bind %s:%d with no auth token "
                         "(could not read /dev/urandom to auto-generate one)\n",
                         wcfg.host.c_str(),
                         static_cast<int>(wcfg.port));
            return 78;
        }
        cfg::save_web_token(config_path, gen);
        wcfg.token = gen;
        std::fprintf(stderr,
                     "fidelis: generated a bearer token (also written to %s)\n"
                     "         token: %s\n"
                     "         use it in any client as: Authorization: Bearer <token>\n",
                     config_path.c_str(), gen.c_str());
    }
    auto web_server = std::make_unique<fidelis::web::WebServer>(
        engine.get(), queue.get(), library.get(), wcfg);
    web_server->start();

    wait_for_termination_signal();

    if (web_server != nullptr) {
        web_server->stop();
        web_server.reset();
    }
    if (dbus != nullptr) {
        dbus->shutdown();
        dbus.reset();
    }
    if (engine != nullptr) {
        engine->set_event_callback({});
        engine->stop();
    }
    queue.reset();
    library.reset();
    engine.reset();
    return 0;
}
