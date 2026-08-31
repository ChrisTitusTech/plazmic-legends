#include "alerts/mote_loot_audio_alert.h"

#include <utility>

#include <QFileInfo>
#include <QProcess>
#include <QStringList>

namespace plazmic {
namespace {

bool ascii_word_character(char character) {
    const auto byte = static_cast<unsigned char>(character);
    return (byte >= 'A' && byte <= 'Z') ||
           (byte >= 'a' && byte <= 'z') ||
           (byte >= '0' && byte <= '9') || byte == '_';
}

bool ascii_equal_case_insensitive(char left, char right) {
    const auto fold = [](unsigned char byte) {
        return byte >= 'A' && byte <= 'Z'
                   ? static_cast<unsigned char>(byte + ('a' - 'A'))
                   : byte;
    };
    return fold(static_cast<unsigned char>(left)) ==
           fold(static_cast<unsigned char>(right));
}

}  // namespace

bool contains_mote_word(std::string_view item_name) {
    constexpr std::string_view kWord = "mote";
    if (item_name.size() < kWord.size()) {
        return false;
    }
    for (std::size_t offset = 0U;
         offset + kWord.size() <= item_name.size(); ++offset) {
        bool matches = true;
        for (std::size_t index = 0U; index < kWord.size(); ++index) {
            if (!ascii_equal_case_insensitive(
                    kWord[index], item_name[offset + index])) {
                matches = false;
                break;
            }
        }
        if (!matches) {
            continue;
        }
        const bool bounded_before =
            offset == 0U || !ascii_word_character(item_name[offset - 1U]);
        const std::size_t after = offset + kWord.size();
        const bool bounded_after =
            after == item_name.size() ||
            !ascii_word_character(item_name[after]);
        if (bounded_before && bounded_after) {
            return true;
        }
    }
    return false;
}

bool play_mote_loot_desktop_sound() {
    constexpr auto kPulsePlayer = "/usr/bin/paplay";
    constexpr auto kBellSound =
        "/usr/share/sounds/freedesktop/stereo/bell.oga";
    if (!QFileInfo(kPulsePlayer).isExecutable() ||
        !QFileInfo(kBellSound).isReadable()) {
        return false;
    }
    return QProcess::startDetached(
        kPulsePlayer,
        {"--client-name=Plazmic Legends",
         "--stream-name=Mote loot alert", kBellSound});
}

MoteLootAudioAlert::MoteLootAudioAlert(Sink sink, Clock clock)
    : sink_(sink ? std::move(sink) : Sink([] {})),
      clock_(clock ? std::move(clock)
                   : Clock([] { return std::chrono::steady_clock::now(); })) {}

void MoteLootAudioAlert::set_enabled(bool enabled) {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    pending_ = false;
    if (enabled_) {
        baseline_ready_ = false;
    }
}

void MoteLootAudioAlert::reset() {
    storage_key_.clear();
    seen_source_ids_.clear();
    baseline_ready_ = false;
    retention_enabled_ = false;
    pending_ = false;
}

void MoteLootAudioAlert::establish_baseline(
    const ActivityAnalyticsSnapshot& snapshot) {
    storage_key_ = snapshot.storage_key;
    seen_source_ids_.clear();
    for (const ActivityEventSnapshot& event : snapshot.events) {
        if (!event.source_id.empty()) {
            seen_source_ids_.insert(event.source_id);
        }
    }
    baseline_ready_ = true;
    retention_enabled_ = snapshot.retention_enabled;
    pending_ = false;
}

void MoteLootAudioAlert::observe(
    const ActivityAnalyticsSnapshot& snapshot) {
    if (!snapshot.available || snapshot.storage_key.empty() ||
        snapshot.events.size() > maximum_snapshot_events) {
        reset();
        return;
    }
    if (!baseline_ready_ || storage_key_ != snapshot.storage_key ||
        retention_enabled_ != snapshot.retention_enabled) {
        establish_baseline(snapshot);
        return;
    }

    bool new_match = false;
    bool rebaseline = false;
    for (const ActivityEventSnapshot& event : snapshot.events) {
        if (event.source_id.empty() ||
            seen_source_ids_.contains(event.source_id)) {
            continue;
        }
        if (seen_source_ids_.size() >= maximum_tracked_sources) {
            rebaseline = true;
            break;
        }
        const bool inserted = seen_source_ids_.insert(event.source_id).second;
        if (inserted && event.kind == ActivityEventKind::loot &&
            contains_mote_word(event.label)) {
            new_match = true;
        }
    }
    if (rebaseline) {
        for (const ActivityEventSnapshot& event : snapshot.events) {
            if (!event.source_id.empty() &&
                !seen_source_ids_.contains(event.source_id) &&
                event.kind == ActivityEventKind::loot &&
                contains_mote_word(event.label)) {
                new_match = true;
            }
        }
        const bool was_pending = pending_;
        establish_baseline(snapshot);
        pending_ = was_pending;
    }
    if (!enabled_) {
        pending_ = false;
        return;
    }
    pending_ = pending_ || new_match;
    const auto now = clock_();
    if (!pending_ || now < next_dispatch_) {
        return;
    }
    sink_();
    pending_ = false;
    next_dispatch_ = now + rate_limit;
}

}  // namespace plazmic
