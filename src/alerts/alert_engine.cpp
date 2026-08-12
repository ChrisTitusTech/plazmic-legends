#include "alerts/alert_engine.h"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <ctime>
#include <fcntl.h>
#include <iomanip>
#include <limits>
#include <locale>
#include <ranges>
#include <sstream>
#include <unordered_set>
#include <unistd.h>
#include <sys/stat.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace plazmic {
namespace {

constexpr std::size_t kMaximumPackBytes = 256U * 1024U;
constexpr std::size_t kMaximumLineBytes = 4096U;
constexpr auto kMaximumFutureSkew = std::chrono::hours(24);

bool valid_text(std::string_view value, std::size_t maximum) {
    return !value.empty() && value.size() <= maximum &&
           std::ranges::all_of(value, [](unsigned char byte) {
               return byte >= 0x20U && byte != 0x7fU;
           });
}

bool valid_json_text(const QString& value, std::size_t maximum) {
    const QByteArray bytes = value.toUtf8();
    return !value.isEmpty() && bytes.size() <= static_cast<qsizetype>(maximum) &&
           QString::fromUtf8(bytes) == value &&
           std::ranges::none_of(value, [](QChar character) {
               return character.isNull() ||
                      character.category() == QChar::Other_Control;
           });
}

std::string casefold(std::string_view value) {
    const QByteArray bytes(value.data(), static_cast<qsizetype>(value.size()));
    return QString::fromUtf8(bytes).toCaseFolded().toUtf8().toStdString();
}

std::optional<std::int64_t> line_timestamp(std::string_view line,
                                           std::string_view& payload) {
    if (line.size() < 27U || line.size() > kMaximumLineBytes ||
        line.front() != '[' || line[25U] != ']' || line[26U] != ' ') {
        return std::nullopt;
    }
    std::tm value{};
    std::istringstream input(std::string(line.substr(1U, 24U)));
    input.imbue(std::locale::classic());
    input >> std::get_time(&value, "%a %b %d %H:%M:%S %Y");
    if (input.fail()) {
        return std::nullopt;
    }
    value.tm_isdst = -1;
    const std::time_t converted = std::mktime(&value);
    if (converted == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }
    payload = line.substr(27U);
    return static_cast<std::int64_t>(converted);
}

std::optional<AlertTimerKind> parse_kind(std::string_view value) {
    if (value == "buff") {
        return AlertTimerKind::buff;
    }
    if (value == "crowd-control") {
        return AlertTimerKind::crowd_control;
    }
    if (value == "respawn") {
        return AlertTimerKind::respawn;
    }
    if (value == "custom") {
        return AlertTimerKind::custom;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> json_uint32(const QJsonValue& value,
                                         std::uint32_t maximum) {
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble(-1.0);
    if (number < 0.0 || number > static_cast<double>(maximum) ||
        number != static_cast<double>(static_cast<std::uint32_t>(number))) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(number);
}

std::optional<QByteArray> read_pack(const std::filesystem::path& path) {
    const int descriptor =
        ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (descriptor < 0) {
        return std::nullopt;
    }
    struct stat status {};
    if (::fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_uid != ::geteuid() || status.st_size < 0 ||
        static_cast<std::uintmax_t>(status.st_size) > kMaximumPackBytes) {
        (void)::close(descriptor);
        return std::nullopt;
    }
    QByteArray bytes(static_cast<qsizetype>(status.st_size), '\0');
    qsizetype received = 0;
    while (received < bytes.size()) {
        const ssize_t count = ::read(
            descriptor, bytes.data() + received,
            static_cast<std::size_t>(bytes.size() - received));
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            (void)::close(descriptor);
            return std::nullopt;
        }
        received += static_cast<qsizetype>(count);
    }
    const bool closed = ::close(descriptor) == 0;
    return closed ? std::optional<QByteArray>(std::move(bytes)) : std::nullopt;
}

}  // namespace

std::optional<AlertRulePack> load_alert_rule_pack(
    const std::filesystem::path& path,
    std::string& detail) {
    detail.clear();
    const auto bytes = read_pack(path);
    if (!bytes) {
        detail = "Alert rule pack must be an owned regular non-symlink file no larger than 256 KiB";
        return std::nullopt;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(*bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        detail = "Alert rule pack is not valid JSON";
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    if (root.value("schema").toInt(-1) != 1 || !root.value("rules").isArray()) {
        detail = "Alert rule pack uses an unsupported schema";
        return std::nullopt;
    }
    const QJsonArray entries = root.value("rules").toArray();
    if (entries.isEmpty() || entries.size() > static_cast<qsizetype>(AlertEngine::maximum_rules)) {
        detail = "Alert rule pack must contain between 1 and 128 rules";
        return std::nullopt;
    }
    AlertRulePack pack;
    pack.source_name = path.filename().string();
    std::unordered_set<std::string> ids;
    for (const QJsonValue& value : entries) {
        if (!value.isObject()) {
            detail = "Alert rule pack contains a malformed rule";
            return std::nullopt;
        }
        const QJsonObject object = value.toObject();
        const QString id_value = object.value("id").toString();
        const QString name_value = object.value("name").toString();
        const QString match_value = object.value("match").toString();
        const std::string id = id_value.toStdString();
        const std::string name = name_value.toStdString();
        const std::string match = match_value.toStdString();
        const std::string kind_text = object.value("kind").toString().toStdString();
        const auto kind = parse_kind(kind_text);
        const auto duration = json_uint32(
            object.value("durationSeconds"), AlertEngine::maximum_duration_seconds);
        const auto cooldown = json_uint32(
            object.value("cooldownSeconds"), AlertEngine::maximum_cooldown_seconds);
        if (!valid_json_text(id_value, 64U) ||
            !valid_json_text(name_value, 128U) ||
            !valid_json_text(match_value, 256U) || !kind || !duration || !cooldown ||
            !ids.insert(id).second ||
            (object.contains("sound") && !object.value("sound").isBool())) {
            detail = "Alert rule pack contains an invalid or duplicate rule";
            return std::nullopt;
        }
        pack.rules.push_back({
            .id = id,
            .name = name,
            .match = match,
            .kind = *kind,
            .duration_seconds = *duration,
            .cooldown_seconds = *cooldown,
            .sound = object.value("sound").toBool(false),
        });
    }
    detail = "Loaded bounded local alert rules";
    return pack;
}

void AlertEngine::set_rules(
    AlertRulePack pack,
    std::chrono::system_clock::time_point now) {
    rules_ = std::move(pack.rules);
    normalized_matches_.clear();
    normalized_matches_.reserve(rules_.size());
    for (const AlertRule& rule : rules_) {
        normalized_matches_.push_back(casefold(rule.match));
    }
    source_name_ = std::move(pack.source_name);
    rules_generation_ = pack.generation;
    clear_observations();
    if (enabled_) {
        activation_unix_seconds_ =
            std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch())
                .count();
    }
}

void AlertEngine::clear_rules() {
    rules_.clear();
    normalized_matches_.clear();
    source_name_.clear();
    rules_generation_ = 0U;
    clear_observations();
}

void AlertEngine::set_enabled(
    bool enabled,
    std::chrono::system_clock::time_point now) {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    clear_observations();
    activation_unix_seconds_ = enabled
                                   ? std::chrono::duration_cast<
                                         std::chrono::seconds>(
                                         now.time_since_epoch())
                                         .count()
                                   : 0;
}

bool AlertEngine::consume(std::string_view line,
                          std::string_view zone,
                          std::string_view source_id,
                          std::chrono::system_clock::time_point now) {
    if (!enabled_ || rules_.empty() || !valid_text(zone, 128U)) {
        return false;
    }
    std::string_view payload;
    const auto timestamp = line_timestamp(line, payload);
    const std::int64_t now_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch())
            .count();
    if (now_seconds < activation_unix_seconds_) {
        activation_unix_seconds_ = now_seconds;
    }
    const std::int64_t maximum_timestamp =
        now_seconds + std::chrono::duration_cast<std::chrono::seconds>(
                          kMaximumFutureSkew)
                          .count();
    if (!timestamp || *timestamp < activation_unix_seconds_ ||
        *timestamp > maximum_timestamp) {
        return false;
    }
    const std::string comparable = casefold(payload);
    std::vector<std::size_t> matching_rules;
    matching_rules.reserve(rules_.size());
    for (std::size_t index = 0U; index < rules_.size(); ++index) {
        if (comparable.find(normalized_matches_[index]) != std::string::npos) {
            matching_rules.push_back(index);
        }
    }
    if (matching_rules.empty() || !valid_text(source_id, 64U)) {
        return false;
    }
    const std::string owned_source(source_id);
    if (observed_sources_.contains(owned_source)) {
        return true;
    }
    if (observed_sources_.size() == maximum_observed_sources) {
        observed_sources_.erase(observed_source_order_.front());
        observed_source_order_.pop_front();
    }
    observed_source_order_.push_back(owned_source);
    observed_sources_.insert(owned_source);
    for (const std::size_t index : matching_rules) {
        const AlertRule& rule = rules_[index];
        const auto previous = last_fired_.find(rule.id);
        if (previous != last_fired_.end()) {
            const std::int64_t elapsed = *timestamp - previous->second;
            if (rule.cooldown_seconds > 0U &&
                elapsed < static_cast<std::int64_t>(rule.cooldown_seconds)) {
                continue;
            }
            previous->second = std::max(previous->second, *timestamp);
        } else {
            last_fired_.emplace(rule.id, *timestamp);
        }
        const std::uint64_t sequence = next_sequence_++;
        recent_alerts_.push_back({
            .rule_id = rule.id,
            .label = rule.name,
            .zone = std::string(zone),
            .timestamp_unix_seconds = *timestamp,
            .sequence = sequence,
            .sound = rule.sound,
        });
        if (rule.sound) {
            latest_sound_sequence_ = sequence;
        }
        if (recent_alerts_.size() > maximum_recent_alerts) {
            recent_alerts_.erase(recent_alerts_.begin());
        }
        if (rule.duration_seconds == 0U) {
            continue;
        }
        const AlertTimerSnapshot timer{
            .id = rule.id,
            .name = rule.name,
            .kind = rule.kind,
            .zone = std::string(zone),
            .started_unix_seconds = *timestamp,
            .ends_unix_seconds =
                *timestamp + static_cast<std::int64_t>(rule.duration_seconds),
            .observed = true,
        };
        const auto existing = std::ranges::find(
            timers_, rule.id, &AlertTimerSnapshot::id);
        if (existing == timers_.end()) {
            if (timers_.size() >= maximum_timers) {
                timers_.erase(timers_.begin());
            }
            timers_.push_back(timer);
        } else if (timer.started_unix_seconds >=
                   existing->started_unix_seconds) {
            *existing = timer;
        }
    }
    return true;
}

void AlertEngine::reset_transient() {
    timers_.erase(
        std::remove_if(timers_.begin(), timers_.end(), [](const auto& timer) {
            return timer.kind != AlertTimerKind::respawn;
        }),
        timers_.end());
}

void AlertEngine::clear_observations() {
    timers_.clear();
    recent_alerts_.clear();
    last_fired_.clear();
    observed_source_order_.clear();
    observed_sources_.clear();
    next_sequence_ = 1U;
    latest_sound_sequence_ = 0U;
}

AlertAnalyticsSnapshot AlertEngine::snapshot(
    std::chrono::system_clock::time_point now) {
    const std::int64_t now_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    timers_.erase(
        std::remove_if(timers_.begin(), timers_.end(), [now_seconds](const auto& timer) {
            return timer.ends_unix_seconds <= now_seconds;
        }),
        timers_.end());
    std::ranges::sort(timers_, {}, &AlertTimerSnapshot::ends_unix_seconds);
    return {
        .timers = timers_,
        .recent_alerts = recent_alerts_,
        .rules_source = source_name_,
        .rules_generation = rules_generation_,
        .latest_sound_sequence = latest_sound_sequence_,
        .available = enabled_ && !rules_.empty(),
        .detail = rules_.empty()
                      ? "Import a local alert rule pack"
                      : enabled_
                            ? "Local rules active; observed log matches only"
                            : "Local rules loaded; alerts disabled",
    };
}

std::string_view alert_timer_kind_label(AlertTimerKind kind) {
    switch (kind) {
        case AlertTimerKind::buff:
            return "Buff";
        case AlertTimerKind::crowd_control:
            return "Crowd control";
        case AlertTimerKind::respawn:
            return "Respawn";
        case AlertTimerKind::custom:
            return "Custom";
    }
    return "Custom";
}

}  // namespace plazmic
