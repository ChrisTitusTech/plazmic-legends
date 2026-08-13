#include "activity/activity_tracker.h"

#include "game/combat_log_parser.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cctype>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <ranges>
#include <sstream>
#include <system_error>
#include <unordered_set>

#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace plazmic {
namespace {

constexpr std::size_t kMaximumLineBytes = 4096U;
constexpr std::uintmax_t kMaximumInventoryBytes = 2U * 1024U * 1024U;
constexpr std::size_t kMaximumInventoryRows = 4096U;
constexpr double kMaximumExperiencePercent = 100.0;
constexpr std::uintmax_t kMaximumActivityBytes = 2U * 1024U * 1024U;
constexpr qsizetype kMaximumActivityFiles = 1024;
constexpr std::size_t kMaximumStreamFingerprints = 4096U;
constexpr std::size_t kMaximumReplaySources = 4096U;
constexpr std::uint32_t kMaximumAaPointsPerEvent = 1000U;

std::string lowercase(std::string_view value) {
    std::string result(value);
    std::ranges::transform(result, result.begin(), [](unsigned char byte) {
        return static_cast<char>(std::tolower(byte));
    });
    return result;
}

bool ci_equal(std::string_view left, std::string_view right) {
    return lowercase(left) == lowercase(right);
}

bool valid_text(std::string_view value, std::size_t maximum = 256U) {
    if (value.empty() || value.size() > maximum) {
        return false;
    }
    return std::ranges::all_of(value, [](unsigned char byte) {
        return byte >= 0x20U && byte != 0x7fU;
    });
}

bool canonical_decimal(std::string_view value) {
    const std::size_t decimal = value.find('.');
    const auto digits = [](std::string_view part) {
        return !part.empty() &&
               std::ranges::all_of(part, [](unsigned char byte) {
                   return byte >= '0' && byte <= '9';
               });
    };
    if (decimal == std::string_view::npos) {
        return digits(value);
    }
    return value.find('.', decimal + 1U) == std::string_view::npos &&
           digits(value.substr(0U, decimal)) &&
           digits(value.substr(decimal + 1U));
}

std::string bounded_equipment_label(std::string_view slot,
                                    std::string_view before,
                                    std::string_view after) {
    constexpr std::size_t kMaximumLabelBytes = 256U;
    constexpr std::size_t kSeparatorBytes = 6U;
    const std::string_view bounded_slot =
        slot.substr(0U, kMaximumLabelBytes - kSeparatorBytes);
    const std::size_t fixed = bounded_slot.size() + kSeparatorBytes;
    const std::size_t item_budget = kMaximumLabelBytes - fixed;
    if (bounded_slot.size() == slot.size() &&
        before.size() <= item_budget &&
        after.size() <= item_budget - before.size()) {
        return std::string(slot) + ": " + std::string(before) + " -> " +
               std::string(after);
    }
    const std::size_t before_budget = item_budget / 2U;
    const std::size_t after_budget = item_budget - before_budget;
    return std::string(bounded_slot) + ": " +
           std::string(before.substr(0U, before_budget)) + " -> " +
           std::string(after.substr(0U, after_budget));
}

bool lowercase_hex(std::string_view value) {
    return !value.empty() && std::ranges::all_of(value, [](unsigned char byte) {
        return (byte >= '0' && byte <= '9') ||
               (byte >= 'a' && byte <= 'f');
    });
}

std::optional<std::string> source_fingerprint(std::string_view source_id) {
    constexpr std::size_t kGenerationBytes = 12U;
    constexpr std::size_t kFingerprintBytes = 32U;
    constexpr std::size_t kFingerprintBegin = kGenerationBytes + 1U;
    constexpr std::size_t kSequenceBegin =
        kFingerprintBegin + kFingerprintBytes + 1U;
    if (source_id.size() <= kSequenceBegin || source_id.size() > 64U ||
        source_id[kGenerationBytes] != ':' ||
        source_id[kSequenceBegin - 1U] != ':' ||
        !lowercase_hex(source_id.substr(0U, kGenerationBytes))) {
        return std::nullopt;
    }
    const std::string_view fingerprint =
        source_id.substr(kFingerprintBegin, kFingerprintBytes);
    const std::string_view sequence = source_id.substr(kSequenceBegin);
    std::uint32_t ordinal{};
    const auto [ordinal_end, ordinal_error] = std::from_chars(
        sequence.data(), sequence.data() + sequence.size(), ordinal);
    if (!lowercase_hex(fingerprint) || sequence.empty() ||
        (sequence.size() > 1U && sequence.front() == '0') ||
        ordinal_error != std::errc{} ||
        ordinal_end != sequence.data() + sequence.size() ||
        ordinal >= ActivityTracker::maximum_occurrences_per_fingerprint ||
        !std::ranges::all_of(sequence, [](unsigned char byte) {
            return byte >= '0' && byte <= '9';
        })) {
        return std::nullopt;
    }
    return std::string(fingerprint);
}

std::string generation_token(std::string_view partition_key,
                             std::uint64_t counter) {
    static std::atomic<std::uint64_t> process_sequence{0U};
    const auto ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    const std::string token = CombatHistoryStore::privacy_key(
        std::string(partition_key) + ":" + std::to_string(counter) + ":" +
        std::to_string(ticks) + ":" +
        std::to_string(
            process_sequence.fetch_add(1U, std::memory_order_relaxed)));
    return token.substr(0U, 12U);
}

std::optional<std::chrono::system_clock::time_point> parse_timestamp(
    std::string_view line,
    std::string_view& payload) {
    if (line.size() < 27U || line.front() != '[' || line[25U] != ']' ||
        line[26U] != ' ') {
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
    const std::time_t timestamp = std::mktime(&value);
    if (timestamp == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }
    payload = line.substr(27U);
    return std::chrono::system_clock::from_time_t(timestamp);
}

template <typename Value>
std::optional<Value> parse_integer(std::string_view value) {
    Value parsed{};
    const auto [end, error] = std::from_chars(
        value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size()) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<double> parse_decimal(std::string_view value) {
    double parsed{};
    const auto [end, error] = std::from_chars(
        value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size() ||
        !std::isfinite(parsed)) {
        return std::nullopt;
    }
    return parsed;
}

std::vector<std::string_view> split_tabs(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t begin = 0U;
    while (begin <= line.size()) {
        const std::size_t tab = line.find('\t', begin);
        fields.push_back(line.substr(
            begin, tab == std::string_view::npos ? line.size() - begin
                                                  : tab - begin));
        if (tab == std::string_view::npos) {
            break;
        }
        begin = tab + 1U;
    }
    return fields;
}

std::string category(DamageKind kind) {
    switch (kind) {
        case DamageKind::spell:
            return "Spell";
        case DamageKind::damage_over_time:
            return "Damage over time";
        case DamageKind::pet:
            return "Pet";
        case DamageKind::melee:
            return "Melee";
    }
    return "Observed ability";
}

bool valid_ability_category(std::string_view value) {
    return value == "Spell" || value == "Damage over time" || value == "Pet" ||
           value == "Melee";
}

std::optional<std::filesystem::path> activity_path(
    const std::filesystem::path& state_root,
    std::string_view key) {
    if (state_root.empty() || key.size() != 32U ||
        !std::ranges::all_of(key, [](unsigned char byte) {
            return (byte >= '0' && byte <= '9') ||
                   (byte >= 'a' && byte <= 'f');
        })) {
        return std::nullopt;
    }
    return state_root / "activity" / (std::string(key) + ".json");
}

class OwnedDescriptor {
  public:
    explicit OwnedDescriptor(int value = -1) : value_(value) {}
    ~OwnedDescriptor() {
        if (value_ >= 0) {
            (void)::close(value_);
        }
    }
    OwnedDescriptor(const OwnedDescriptor&) = delete;
    OwnedDescriptor& operator=(const OwnedDescriptor&) = delete;
    OwnedDescriptor(OwnedDescriptor&& other) noexcept
        : value_(std::exchange(other.value_, -1)) {}
    OwnedDescriptor& operator=(OwnedDescriptor&& other) noexcept {
        if (this != &other) {
            if (value_ >= 0) {
                (void)::close(value_);
            }
            value_ = std::exchange(other.value_, -1);
        }
        return *this;
    }
    [[nodiscard]] int get() const { return value_; }
    [[nodiscard]] explicit operator bool() const { return value_ >= 0; }

  private:
    int value_;
};

std::optional<OwnedDescriptor> open_directory_chain(
    const std::filesystem::path& directory,
    bool create_missing,
    bool* missing = nullptr,
    bool* created = nullptr) {
    if (missing != nullptr) {
        *missing = false;
    }
    if (created != nullptr) {
        *created = false;
    }
    if (directory.empty()) {
        return std::nullopt;
    }
    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(directory, error).lexically_normal();
    if (error || absolute.empty()) {
        return std::nullopt;
    }
    OwnedDescriptor current(::open(
        absolute.root_path().c_str(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (!current) {
        return std::nullopt;
    }
    for (const auto& component : absolute.relative_path()) {
        const std::string name = component.string();
        if (name.empty() || name == "." || name == "..") {
            return std::nullopt;
        }
        int next = ::openat(
            current.get(), name.c_str(),
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (next < 0 && errno == ENOENT && create_missing) {
            const int mkdir_result =
                ::mkdirat(current.get(), name.c_str(), 0700);
            if (mkdir_result != 0 && errno != EEXIST) {
                return std::nullopt;
            }
            if (created != nullptr && mkdir_result == 0) {
                *created = true;
            }
            next = ::openat(
                current.get(), name.c_str(),
                O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        }
        if (next < 0) {
            if (missing != nullptr && errno == ENOENT) {
                *missing = true;
            }
            return std::nullopt;
        }
        current = OwnedDescriptor(next);
    }
    return current;
}

bool owner_only_directory(int descriptor) {
    struct stat status {};
    return ::fstat(descriptor, &status) == 0 &&
           S_ISDIR(status.st_mode) && status.st_uid == ::geteuid() &&
           (status.st_mode & 0777) == 0700;
}

bool owner_only_file(const struct stat& status) {
    return S_ISREG(status.st_mode) && status.st_uid == ::geteuid() &&
           (status.st_mode & 0777) == 0600;
}

bool owned_regular_file(const struct stat& status) {
    return S_ISREG(status.st_mode) && status.st_uid == ::geteuid();
}

std::optional<QByteArray> read_owner_only_file(
    const std::filesystem::path& path,
    std::uintmax_t maximum_bytes) {
    auto directory = open_directory_chain(path.parent_path(), false);
    if (!directory || !owner_only_directory(directory->get())) {
        return std::nullopt;
    }
    OwnedDescriptor descriptor(::openat(
        directory->get(), path.filename().c_str(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
    if (!descriptor) {
        return std::nullopt;
    }
    struct stat status {};
    if (::fstat(descriptor.get(), &status) != 0 ||
        !owner_only_file(status) ||
        status.st_size < 0 ||
        static_cast<std::uintmax_t>(status.st_size) > maximum_bytes) {
        return std::nullopt;
    }
    QByteArray bytes(static_cast<qsizetype>(status.st_size), '\0');
    qsizetype received = 0;
    while (received < bytes.size()) {
        const ssize_t chunk = ::read(
            descriptor.get(), bytes.data() + received,
            static_cast<std::size_t>(bytes.size() - received));
        if (chunk < 0 && errno == EINTR) {
            continue;
        }
        if (chunk <= 0) {
            return std::nullopt;
        }
        received += static_cast<qsizetype>(chunk);
    }
    char extra{};
    ssize_t trailing{};
    do {
        trailing = ::read(descriptor.get(), &extra, 1U);
    } while (trailing < 0 && errno == EINTR);
    if (trailing != 0) {
        return std::nullopt;
    }
    return bytes;
}

std::optional<std::int64_t> json_int64(const QJsonValue& value) {
    if (!value.isString()) {
        return std::nullopt;
    }
    return parse_integer<std::int64_t>(value.toString().toStdString());
}

std::optional<std::uint64_t> json_uint64(const QJsonValue& value) {
    if (!value.isString()) {
        return std::nullopt;
    }
    return parse_integer<std::uint64_t>(value.toString().toStdString());
}

QJsonObject event_json(const ActivityEventSnapshot& event) {
    QJsonObject object{
        {"kind", static_cast<int>(event.kind)},
        {"timestamp", QString::number(event.timestamp_unix_seconds)},
        {"zone", QString::fromStdString(event.zone)},
        {"label", QString::fromStdString(event.label)},
        {"amount", event.amount},
        {"evidence", QString::fromStdString(event.evidence)},
    };
    if (!event.source_id.empty()) {
        object.insert("sourceId", QString::fromStdString(event.source_id));
    }
    if (event.total) {
        object.insert("total", static_cast<qint64>(*event.total));
    }
    return object;
}

std::optional<ActivityEventSnapshot> event_from_json(
    const QJsonValue& value) {
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    const int kind = object.value("kind").toInt(-1);
    const auto timestamp = json_int64(object.value("timestamp"));
    const std::string zone = object.value("zone").toString().toStdString();
    const std::string label = object.value("label").toString().toStdString();
    const std::string evidence =
        object.value("evidence").toString().toStdString();
    const std::string source_id =
        object.value("sourceId").toString().toStdString();
    const double amount = object.value("amount").toDouble(
        std::numeric_limits<double>::quiet_NaN());
    if (kind < static_cast<int>(ActivityEventKind::experience) ||
        kind > static_cast<int>(ActivityEventKind::celebration) ||
        !timestamp || !valid_text(zone, 128U) || !valid_text(label) ||
        !valid_text(evidence) ||
        (!source_id.empty() && !valid_text(source_id, 64U)) ||
        !std::isfinite(amount) || amount < 0.0 ||
        amount > 10'000'000.0) {
        return std::nullopt;
    }
    std::optional<std::uint32_t> total;
    if (object.contains("total")) {
        const qint64 parsed = object.value("total").toInteger(-1);
        if (parsed < 0 || parsed > 10'000'000) {
            return std::nullopt;
        }
        total = static_cast<std::uint32_t>(parsed);
    }
    const auto event_kind = static_cast<ActivityEventKind>(kind);
    const bool valid_kind_fields = [&]() {
        switch (event_kind) {
            case ActivityEventKind::experience:
                return amount > 0.0 &&
                       amount <= kMaximumExperiencePercent && !total;
            case ActivityEventKind::alternate_advancement:
                return amount >= 1.0 &&
                       amount <=
                           static_cast<double>(kMaximumAaPointsPerEvent) &&
                       std::floor(amount) == amount && total.has_value();
            case ActivityEventKind::loot:
            case ActivityEventKind::equipment_change:
            case ActivityEventKind::celebration:
                return amount == 1.0 && !total;
        }
        return false;
    }();
    if (!valid_kind_fields) {
        return std::nullopt;
    }
    return ActivityEventSnapshot{
        .kind = event_kind,
        .timestamp_unix_seconds = *timestamp,
        .zone = zone,
        .label = label,
        .amount = amount,
        .total = total,
        .evidence = evidence,
        .source_id = source_id,
    };
}

QJsonObject ability_json(std::string_view name,
                         std::string_view category_name,
                         std::uint64_t damage,
                         std::uint32_t observations,
                         std::string_view confidence,
                         std::optional<std::int64_t> last_seen_unix_seconds =
                             std::nullopt) {
    QJsonObject object{
        {"name", QString::fromUtf8(name.data(),
                                   static_cast<qsizetype>(name.size()))},
        {"category", QString::fromUtf8(
                         category_name.data(),
                         static_cast<qsizetype>(category_name.size()))},
        {"damage", QString::number(damage)},
        {"observations", static_cast<qint64>(observations)},
        {"confidence", QString::fromUtf8(
                           confidence.data(),
                           static_cast<qsizetype>(confidence.size()))},
    };
    if (last_seen_unix_seconds) {
        object.insert("lastSeen", QString::number(*last_seen_unix_seconds));
    }
    return object;
}

bool write_owner_only(const std::filesystem::path& path,
                      const QByteArray& bytes,
                      bool require_owner_directory = false) {
    auto directory = open_directory_chain(path.parent_path(), true);
    if (!directory ||
        (require_owner_directory &&
         !owner_only_directory(directory->get()))) {
        return false;
    }
    const std::string destination = path.filename().string();
    if (destination.empty() || destination == "." || destination == "..") {
        return false;
    }
    struct stat existing {};
    const bool destination_exists =
        ::fstatat(directory->get(), destination.c_str(), &existing,
                  AT_SYMLINK_NOFOLLOW) == 0;
    const auto valid_destination = [require_owner_directory](
                                       const struct stat& status) {
        return require_owner_directory ? owner_only_file(status)
                                       : owned_regular_file(status);
    };
    if ((destination_exists && !valid_destination(existing)) ||
        (!destination_exists && errno != ENOENT)) {
        return false;
    }
    static std::atomic<std::uint64_t> temporary_sequence{0U};
    const std::string temporary =
        ".plazmic-activity-" + std::to_string(::getpid()) + "-" +
        std::to_string(temporary_sequence.fetch_add(
            1U, std::memory_order_relaxed));
    OwnedDescriptor output(::openat(
        directory->get(), temporary.c_str(),
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
    if (!output) {
        return false;
    }
    std::size_t written = 0U;
    while (written < static_cast<std::size_t>(bytes.size())) {
        const ssize_t count = ::write(
            output.get(), bytes.constData() + written,
            static_cast<std::size_t>(bytes.size()) - written);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            (void)::unlinkat(directory->get(), temporary.c_str(), 0);
            return false;
        }
        written += static_cast<std::size_t>(count);
    }
    if (::fchmod(output.get(), 0600) != 0 || ::fsync(output.get()) != 0) {
        (void)::unlinkat(directory->get(), temporary.c_str(), 0);
        return false;
    }
    struct stat current {};
    const bool current_exists =
        ::fstatat(directory->get(), destination.c_str(), &current,
                  AT_SYMLINK_NOFOLLOW) == 0;
    if (current_exists != destination_exists ||
        (current_exists &&
         (current.st_dev != existing.st_dev ||
          current.st_ino != existing.st_ino ||
          !valid_destination(current))) ||
        (!current_exists && errno != ENOENT) ||
        ::renameat(directory->get(), temporary.c_str(), directory->get(),
                   destination.c_str()) != 0) {
        (void)::unlinkat(directory->get(), temporary.c_str(), 0);
        return false;
    }
    return ::fsync(directory->get()) == 0;
}

}  // namespace

std::optional<ActivityEventSnapshot> parse_activity_line(
    std::string_view line,
    std::string_view active_character,
    std::string_view zone) {
    if (line.empty() || line.size() > kMaximumLineBytes ||
        !valid_text(active_character, 128U) ||
        !valid_text(zone, 128U)) {
        return std::nullopt;
    }
    std::string_view payload;
    const auto timestamp = parse_timestamp(line, payload);
    if (!timestamp) {
        return std::nullopt;
    }
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                             timestamp->time_since_epoch())
                             .count();

    constexpr std::string_view kExperiencePrefix =
        "You gain experience! (";
    if (payload.starts_with(kExperiencePrefix) && payload.ends_with("%)")) {
        const std::string_view amount_text = payload.substr(
            kExperiencePrefix.size(),
            payload.size() - kExperiencePrefix.size() - 2U);
        if (!canonical_decimal(amount_text)) {
            return std::nullopt;
        }
        const auto amount = parse_decimal(amount_text);
        if (!amount || *amount <= 0.0 ||
            *amount > kMaximumExperiencePercent) {
            return std::nullopt;
        }
        return ActivityEventSnapshot{
            .kind = ActivityEventKind::experience,
            .timestamp_unix_seconds = seconds,
            .zone = std::string(zone),
            .label = "Experience gained",
            .amount = *amount,
            .total = std::nullopt,
            .evidence = "Exact local log percentage",
            .source_id = {},
        };
    }

    constexpr std::string_view kAaPrefix = "You have gained ";
    constexpr std::string_view kAaPoint = " ability point";
    constexpr std::string_view kAaTotalPrefix = "You now have ";
    if (payload.starts_with(kAaPrefix)) {
        std::string_view remainder = payload.substr(kAaPrefix.size());
        const std::size_t point = remainder.find(kAaPoint);
        if (point == std::string_view::npos) {
            return std::nullopt;
        }
        const std::string_view amount_text = remainder.substr(0U, point);
        std::uint32_t amount = 1U;
        if (amount_text != "an") {
            const auto parsed_amount = parse_integer<std::uint32_t>(
                amount_text);
            if (!parsed_amount || *parsed_amount == 0U ||
                *parsed_amount > kMaximumAaPointsPerEvent) {
                return std::nullopt;
            }
            amount = *parsed_amount;
        }
        remainder.remove_prefix(point + kAaPoint.size());
        const std::string_view gain_suffix =
            amount_text == "an" ? "!" : "(s)!";
        if (!remainder.starts_with(gain_suffix)) {
            return std::nullopt;
        }
        remainder.remove_prefix(gain_suffix.size());
        std::size_t spaces = 0U;
        while (remainder.starts_with(' ') && spaces < 3U) {
            remainder.remove_prefix(1U);
            ++spaces;
        }
        if (spaces == 0U || spaces > 2U ||
            !remainder.starts_with(kAaTotalPrefix)) {
            return std::nullopt;
        }
        remainder.remove_prefix(kAaTotalPrefix.size());
        const std::size_t space = remainder.find(' ');
        if (space == std::string_view::npos) {
            return std::nullopt;
        }
        const auto total = parse_integer<std::uint32_t>(
            remainder.substr(0U, space));
        const std::string_view suffix = remainder.substr(space);
        if (!total || *total > 10'000'000U ||
            (suffix != " ability point." &&
             suffix != " ability points." &&
             suffix != " ability point(s).")) {
            return std::nullopt;
        }
        return ActivityEventSnapshot{
            .kind = ActivityEventKind::alternate_advancement,
            .timestamp_unix_seconds = seconds,
            .zone = std::string(zone),
            .label = amount == 1U
                         ? "Alternate Advancement point gained"
                         : "Alternate Advancement points gained",
            .amount = static_cast<double>(amount),
            .total = *total,
            .evidence = "Exact local log total",
            .source_id = {},
        };
    }

    constexpr std::string_view kLootPrefix = "--You have looted ";
    constexpr std::string_view kLootSuffix = ".--";
    if (payload.starts_with(kLootPrefix) && payload.ends_with(kLootSuffix)) {
        const std::string_view item = payload.substr(
            kLootPrefix.size(),
            payload.size() - kLootPrefix.size() - kLootSuffix.size());
        if (!valid_text(item)) {
            return std::nullopt;
        }
        return ActivityEventSnapshot{
            .kind = ActivityEventKind::loot,
            .timestamp_unix_seconds = seconds,
            .zone = std::string(zone),
            .label = std::string(item),
            .amount = 1.0,
            .total = std::nullopt,
            .evidence = "Exact local loot line",
            .source_id = {},
        };
    }
    return std::nullopt;
}

void ActivityTracker::clear() {
    events_.clear();
    abilities_.clear();
    equipment_.clear();
    current_alternate_advancement_percent_.reset();
    current_alternate_advancement_points_.reset();
    character_.clear();
    dirty_ = false;
    persisted_ = true;
    compatible_ = true;
    partition_loaded_ = false;
    persistence_retry_after_ = {};
    replay_sources_.clear();
    boundary_source_ids_.clear();
    replay_history_.clear();
    begin_log_stream();
}

bool ActivityTracker::select(std::string key) {
    if (!activity_path(state_root_, key)) {
        compatible_ = false;
        persisted_ = false;
        return false;
    }
    if (key == key_) {
        return compatible_;
    }
    if (retention_enabled_ && dirty_ &&
        (persistence_deferred_ || !persist())) {
        return false;
    }
    events_.clear();
    abilities_.clear();
    equipment_.clear();
    current_alternate_advancement_percent_.reset();
    current_alternate_advancement_points_.reset();
    character_.clear();
    key_ = std::move(key);
    dirty_ = false;
    persisted_ = true;
    compatible_ = true;
    partition_loaded_ = false;
    persistence_retry_after_ = {};
    replay_sources_.clear();
    boundary_source_ids_.clear();
    replay_history_.clear();
    generation_counter_ = 0U;
    const bool selected = !retention_enabled_ || load();
    begin_log_stream();
    return selected;
}

void ActivityTracker::set_retention_enabled(bool enabled) {
    if (enabled == retention_enabled_) {
        return;
    }
    retention_enabled_ = enabled;
    if (!enabled || key_.empty()) {
        return;
    }
    if (partition_loaded_) {
        dirty_ = true;
        (void)persist();
        return;
    }
    const auto session_events = events_;
    const auto session_abilities = abilities_;
    const auto session_replay_history = replay_history_;
    const std::uint64_t session_generation_counter = generation_counter_;
    if (!load()) {
        events_ = session_events;
        abilities_ = session_abilities;
        replay_history_ = session_replay_history;
        generation_counter_ = session_generation_counter;
        begin_log_stream();
        return;
    }
    for (const auto& event : session_events) {
        const auto duplicate = std::ranges::find_if(
            events_, [&event](const auto& existing) {
                if (!event.source_id.empty() || !existing.source_id.empty()) {
                    return !event.source_id.empty() &&
                           event.source_id == existing.source_id;
                }
                return existing.timestamp_unix_seconds ==
                           event.timestamp_unix_seconds &&
                       existing.kind == event.kind &&
                       existing.label == event.label;
            });
        if (duplicate == events_.end()) {
            append(event);
        }
    }
    for (const auto& [key, session] : session_abilities) {
        auto [found, inserted] = abilities_.try_emplace(key, session);
        if (!inserted) {
            for (const auto& observation : session.observations) {
                const auto duplicate = std::ranges::find(
                    found->second.observations, observation.source_id,
                    &AbilityAggregate::Observation::source_id);
                if (duplicate == found->second.observations.end()) {
                    found->second.observations.push_back(observation);
                }
            }
        }
    }
    bound_ability_aggregates();
    bound_ability_observations();
    for (const auto& source : session_replay_history) {
        remember_replay_source(
            source.source_id, source.timestamp_unix_seconds);
    }
    generation_counter_ =
        std::max(generation_counter_, session_generation_counter);
    begin_log_stream();
    dirty_ = true;
    (void)persist();
}

void ActivityTracker::commit_deferred_persistence(bool enabled) {
    persistence_deferred_ = false;
    if (enabled != retention_enabled_) {
        set_retention_enabled(enabled);
    } else if (enabled && dirty_) {
        (void)persist();
    }
}

bool ActivityTracker::delete_history(std::string_view key) {
    const auto path = activity_path(state_root_, key);
    if (!path) {
        return false;
    }
    const auto clear_selected = [this, key]() {
        if (key != key_) {
            return;
        }
        events_.clear();
        abilities_.clear();
        equipment_.clear();
        dirty_ = false;
        persisted_ = true;
        compatible_ = true;
        partition_loaded_ = true;
        persistence_retry_after_ = {};
        replay_sources_.clear();
        boundary_source_ids_.clear();
        replay_history_.clear();
        generation_counter_ = 0U;
        begin_log_stream();
    };
    bool directory_missing = false;
    auto directory = open_directory_chain(
        path->parent_path(), false, &directory_missing);
    if (!directory) {
        if (directory_missing) {
            clear_selected();
        }
        return directory_missing;
    }
    if (!owner_only_directory(directory->get())) {
        return false;
    }
    const std::string name = path->filename().string();
    OwnedDescriptor file(::openat(
        directory->get(), name.c_str(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
    if (!file) {
        if (errno != ENOENT) {
            return false;
        }
    } else {
        struct stat opened {};
        struct stat current {};
        if (::fstat(file.get(), &opened) != 0 ||
            !owner_only_file(opened) ||
            ::fstatat(directory->get(), name.c_str(), &current,
                      AT_SYMLINK_NOFOLLOW) != 0 ||
            current.st_dev != opened.st_dev ||
            current.st_ino != opened.st_ino ||
            !owner_only_file(current) ||
            ::unlinkat(directory->get(), name.c_str(), 0) != 0 ||
            ::fsync(directory->get()) != 0) {
            return false;
        }
    }
    clear_selected();
    return true;
}

bool ActivityTracker::flush() {
    return persistence_deferred_ || !retention_enabled_ || !dirty_ ||
           persist();
}

void ActivityTracker::append(ActivityEventSnapshot event) {
    if (!event.source_id.empty() &&
        (boundary_source_ids_.contains(event.source_id) ||
         std::ranges::find(events_, event.source_id,
                           &ActivityEventSnapshot::source_id) !=
             events_.end())) {
        return;
    }
    if (events_.size() == maximum_events) {
        const auto oldest = std::ranges::min_element(
            events_, {}, &ActivityEventSnapshot::timestamp_unix_seconds);
        if (event.timestamp_unix_seconds <
            oldest->timestamp_unix_seconds) {
            return;
        }
        events_.erase(oldest);
    }
    events_.push_back(std::move(event));
    dirty_ = true;
    persisted_ = false;
}

std::string ActivityTracker::consume(std::string_view line,
                                     std::string_view active_character,
                                     std::string_view zone,
                                     bool source_required) {
    if (!compatible_ || !valid_text(active_character, 128U)) {
        return {};
    }
    if (!character_.empty() && !ci_equal(character_, active_character)) {
        return {};
    }
    character_ = std::string(active_character);
    auto event = parse_activity_line(line, active_character, zone);
    if (!event && !source_required) {
        return {};
    }
    const std::string source_id = next_source_id(line);
    if (event) {
        event->source_id = source_id;
        append(std::move(*event));
    }
    return source_id;
}

void ActivityTracker::observe_damage(
    const DamageEvent& event,
    std::string_view active_character,
    std::string source_id) {
    if (!compatible_ || !ci_equal(event.attacker, active_character) ||
        !is_activity_ability(event) || !valid_text(event.ability)) {
        return;
    }
    const auto timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(
            event.timestamp.time_since_epoch())
            .count();
    if (timestamp <= 0) {
        return;
    }
    if (!source_fingerprint(source_id) ||
        boundary_source_ids_.contains(source_id)) {
        return;
    }
    const std::string observed_category = category(event.kind);
    const std::string aggregate_key =
        lowercase(event.ability) + "\n" + observed_category;
    auto found = abilities_.find(aggregate_key);
    if (found == abilities_.end()) {
        if (abilities_.size() >= maximum_abilities) {
            return;
        }
        found = abilities_.emplace(
                              aggregate_key,
                              AbilityAggregate{
                                  .name = event.ability,
                                  .category = observed_category,
                                  .observations = {},
                              })
                    .first;
    }
    AbilityAggregate& aggregate = found->second;
    if (std::ranges::find(aggregate.observations, source_id,
                          &AbilityAggregate::Observation::source_id) !=
        aggregate.observations.end()) {
        return;
    }
    aggregate.observations.push_back({
        .source_id = std::move(source_id),
        .damage = event.damage,
        .timestamp_unix_seconds = timestamp,
    });
    bound_ability_observations();
    dirty_ = true;
    persisted_ = false;
}

void ActivityTracker::begin_log_stream(std::string_view replay_prefix) {
    replay_sources_.clear();
    boundary_source_ids_.clear();
    if (replay_history_.empty()) {
        for (const auto& event : events_) {
            remember_replay_source(
                event.source_id, event.timestamp_unix_seconds);
        }
        for (const auto& [key, aggregate] : abilities_) {
            (void)key;
            for (const auto& observation : aggregate.observations) {
                remember_replay_source(
                    observation.source_id,
                    observation.timestamp_unix_seconds);
            }
        }
    }
    std::unordered_map<std::string, std::vector<std::string>> candidates;
    for (const auto& source : replay_history_) {
        const auto fingerprint = source_fingerprint(source.source_id);
        if (!fingerprint) {
            continue;
        }
        candidates[*fingerprint].push_back(source.source_id);
    }
    std::unordered_map<std::string, std::size_t> matched_occurrences;
    std::size_t begin = 0U;
    std::size_t retained_sources = 0U;
    while (begin < replay_prefix.size() &&
           retained_sources < kMaximumReplaySources) {
        const std::size_t newline = replay_prefix.find('\n', begin);
        if (newline == std::string_view::npos) {
            break;
        }
        std::string_view line = replay_prefix.substr(begin, newline - begin);
        if (line.ends_with('\r')) {
            line.remove_suffix(1U);
        }
        const std::string fingerprint =
            CombatHistoryStore::privacy_key(line);
        const auto found = candidates.find(fingerprint);
        if (found != candidates.end()) {
            std::size_t& occurrence = matched_occurrences[fingerprint];
            if (occurrence < found->second.size()) {
                const std::string& source_id = found->second[occurrence];
                replay_sources_[fingerprint].push_back(source_id);
                boundary_source_ids_.insert(source_id);
                ++occurrence;
                ++retained_sources;
            }
        }
        begin = newline + 1U;
    }
    stream_occurrences_.clear();
    stream_generation_ = allocate_stream_generation();
    stream_overflow_sequence_ = 0U;
}

void ActivityTracker::reset_transient_observations() {
    break_equipment_baseline();
    begin_log_stream();
}

void ActivityTracker::break_equipment_baseline() {
    equipment_.clear();
    current_alternate_advancement_percent_.reset();
    current_alternate_advancement_points_.reset();
    character_.clear();
}

std::string ActivityTracker::next_source_id(std::string_view line) {
    const std::string fingerprint = CombatHistoryStore::privacy_key(line);
    std::string_view payload;
    const auto timestamp = parse_timestamp(line, payload);
    const std::int64_t timestamp_unix_seconds =
        timestamp ? std::chrono::duration_cast<std::chrono::seconds>(
                        timestamp->time_since_epoch())
                        .count()
                  : 0;
    if (stream_generation_.empty()) {
        stream_generation_ = allocate_stream_generation();
    }
    const auto existing_occurrence = stream_occurrences_.find(fingerprint);
    const bool occurrence_exhausted =
        existing_occurrence != stream_occurrences_.end() &&
        existing_occurrence->second >= maximum_occurrences_per_fingerprint;
    const bool overflow_exhausted =
        stream_overflow_sequence_ >= maximum_occurrences_per_fingerprint;
    if (occurrence_exhausted || overflow_exhausted) {
        replay_sources_.clear();
        stream_occurrences_.clear();
        stream_generation_ = allocate_stream_generation();
        stream_overflow_sequence_ = 0U;
    }
    const bool replay_tracked = replay_sources_.contains(fingerprint);
    if (!stream_occurrences_.contains(fingerprint) &&
        stream_occurrences_.size() >= kMaximumStreamFingerprints &&
        !replay_tracked) {
        const std::string result = stream_generation_ + ":" + fingerprint +
                                   ":" +
                                   std::to_string(stream_overflow_sequence_);
        ++stream_overflow_sequence_;
        remember_replay_source(result, timestamp_unix_seconds);
        return result;
    }
    std::uint32_t& occurrence = stream_occurrences_[fingerprint];
    const auto replay = replay_sources_.find(fingerprint);
    const std::string result =
        replay != replay_sources_.end() && occurrence < replay->second.size()
            ? replay->second[occurrence]
            : stream_generation_ + ":" + fingerprint + ":" +
                  std::to_string(occurrence);
    ++occurrence;
    remember_replay_source(result, timestamp_unix_seconds);
    return result;
}

std::string ActivityTracker::allocate_stream_generation() {
    for (;;) {
        if (generation_counter_ == std::numeric_limits<std::uint64_t>::max()) {
            generation_counter_ = 0U;
        }
        ++generation_counter_;
        const std::string candidate =
            generation_token(key_, generation_counter_);
        const bool retained_collision = std::ranges::any_of(
            replay_history_, [&candidate](const ReplaySource& source) {
                return source.source_id.starts_with(candidate + ":");
            });
        if (!retained_collision) {
            return candidate;
        }
    }
}

void ActivityTracker::remember_replay_source(
    const std::string& source_id,
    std::int64_t timestamp_unix_seconds) {
    if (!source_fingerprint(source_id) || timestamp_unix_seconds <= 0) {
        return;
    }
    const auto existing = std::ranges::find(
        replay_history_, source_id, &ReplaySource::source_id);
    if (existing != replay_history_.end()) {
        if (timestamp_unix_seconds > existing->timestamp_unix_seconds) {
            existing->timestamp_unix_seconds = timestamp_unix_seconds;
            dirty_ = true;
            persisted_ = false;
        }
        return;
    }
    if (replay_history_.size() == kMaximumReplaySources) {
        const auto oldest = std::ranges::min_element(
            replay_history_, {}, &ReplaySource::timestamp_unix_seconds);
        if (timestamp_unix_seconds < oldest->timestamp_unix_seconds) {
            return;
        }
        replay_history_.erase(oldest);
    }
    replay_history_.push_back({
        .source_id = source_id,
        .timestamp_unix_seconds = timestamp_unix_seconds,
    });
    dirty_ = true;
    persisted_ = false;
}

void ActivityTracker::bound_ability_observations() {
    std::size_t total = 0U;
    for (const auto& [name, aggregate] : abilities_) {
        (void)name;
        total += aggregate.observations.size();
    }
    while (total > maximum_ability_observations) {
        auto oldest_aggregate = abilities_.end();
        std::size_t oldest_index = 0U;
        std::int64_t oldest_timestamp = std::numeric_limits<std::int64_t>::max();
        for (auto aggregate = abilities_.begin(); aggregate != abilities_.end();
             ++aggregate) {
            for (std::size_t index = 0U;
                 index < aggregate->second.observations.size(); ++index) {
                const auto timestamp = aggregate->second.observations[index]
                                           .timestamp_unix_seconds;
                if (timestamp < oldest_timestamp) {
                    oldest_timestamp = timestamp;
                    oldest_aggregate = aggregate;
                    oldest_index = index;
                }
            }
        }
        if (oldest_aggregate == abilities_.end()) {
            break;
        }
        oldest_aggregate->second.observations.erase(
            oldest_aggregate->second.observations.begin() +
            static_cast<std::ptrdiff_t>(oldest_index));
        if (oldest_aggregate->second.observations.empty()) {
            abilities_.erase(oldest_aggregate);
        }
        --total;
    }
}

void ActivityTracker::bound_ability_aggregates() {
    while (abilities_.size() > maximum_abilities) {
        auto oldest = abilities_.end();
        std::int64_t oldest_latest = std::numeric_limits<std::int64_t>::max();
        for (auto candidate = abilities_.begin(); candidate != abilities_.end();
             ++candidate) {
            if (candidate->second.observations.empty()) {
                oldest = candidate;
                oldest_latest = std::numeric_limits<std::int64_t>::min();
                break;
            }
            const auto latest = std::ranges::max(
                candidate->second.observations,
                {}, &AbilityAggregate::Observation::timestamp_unix_seconds)
                                    .timestamp_unix_seconds;
            if (latest < oldest_latest ||
                (latest == oldest_latest &&
                 (oldest == abilities_.end() || candidate->first < oldest->first))) {
                oldest = candidate;
                oldest_latest = latest;
            }
        }
        if (oldest == abilities_.end()) {
            break;
        }
        abilities_.erase(oldest);
    }
}

void ActivityTracker::observe_character(
    const CharacterSnapshot& character,
    std::string_view zone,
    std::chrono::system_clock::time_point now) {
    if (!compatible_ || !character.available() ||
        !valid_text(character.name, 128U) ||
        !valid_text(zone, 128U)) {
        equipment_.clear();
        current_alternate_advancement_percent_.reset();
        current_alternate_advancement_points_.reset();
        return;
    }
    if (!character_.empty() && !ci_equal(character_, character.name)) {
        current_alternate_advancement_percent_.reset();
        current_alternate_advancement_points_.reset();
        return;
    }
    character_ = character.name;
    current_alternate_advancement_percent_ =
        character.alternate_advancement_percent;
    current_alternate_advancement_points_ =
        character.alternate_advancement_points;
    if (equipment_.empty()) {
        equipment_ = character.equipment;
        return;
    }
    for (const auto& current : character.equipment) {
        const auto previous = std::ranges::find(
            equipment_, current.slot, &EquipmentSlotSnapshot::slot);
        if (previous != equipment_.end() &&
            previous->item == current.item) {
            continue;
        }
        if (previous == equipment_.end() && current.item.empty()) {
            continue;
        }
        const std::string before =
            previous == equipment_.end() || previous->item.empty()
                                       ? "Empty"
                                       : previous->item;
        const std::string after = current.item.empty()
                                      ? "Empty"
                                      : current.item;
        append(ActivityEventSnapshot{
            .kind = ActivityEventKind::equipment_change,
            .timestamp_unix_seconds =
                std::chrono::duration_cast<std::chrono::seconds>(
                    now.time_since_epoch())
                    .count(),
            .zone = std::string(zone),
            .label = bounded_equipment_label(current.slot, before, after),
            .amount = 1.0,
            .total = std::nullopt,
            .evidence = "Two consecutive immutable equipment snapshots",
            .source_id = {},
        });
    }
    for (const auto& previous : equipment_) {
        const auto current = std::ranges::find(
            character.equipment, previous.slot,
            &EquipmentSlotSnapshot::slot);
        if (current != character.equipment.end() || previous.item.empty()) {
            continue;
        }
        append(ActivityEventSnapshot{
            .kind = ActivityEventKind::equipment_change,
            .timestamp_unix_seconds =
                std::chrono::duration_cast<std::chrono::seconds>(
                    now.time_since_epoch())
                    .count(),
            .zone = std::string(zone),
            .label = bounded_equipment_label(
                previous.slot, previous.item, "Empty"),
            .amount = 1.0,
            .total = std::nullopt,
            .evidence = "Two consecutive immutable equipment snapshots",
            .source_id = {},
        });
    }
    equipment_ = character.equipment;
}

void ActivityTracker::prune(std::chrono::system_clock::time_point now) {
    const auto cutoff = std::chrono::duration_cast<std::chrono::seconds>(
                            (now - maximum_age).time_since_epoch())
                            .count();
    const auto future_limit =
        std::chrono::duration_cast<std::chrono::seconds>(
            (now + std::chrono::hours(24)).time_since_epoch())
            .count();
    const std::size_t removed =
        std::erase_if(events_, [cutoff, future_limit](const auto& event) {
            return event.timestamp_unix_seconds < cutoff ||
                   event.timestamp_unix_seconds > future_limit;
        });
    std::size_t removed_abilities = 0U;
    for (auto& [name, aggregate] : abilities_) {
        (void)name;
        removed_abilities += std::erase_if(
            aggregate.observations,
            [cutoff, future_limit](const auto& observation) {
                return observation.timestamp_unix_seconds < cutoff ||
                       observation.timestamp_unix_seconds > future_limit;
            });
    }
    std::erase_if(abilities_, [](const auto& entry) {
        return entry.second.observations.empty();
    });
    const std::size_t removed_replay = std::erase_if(
        replay_history_, [cutoff, future_limit](const auto& source) {
            return source.timestamp_unix_seconds < cutoff ||
                   source.timestamp_unix_seconds > future_limit;
        });
    if (removed > 0U || removed_abilities > 0U || removed_replay > 0U) {
        dirty_ = true;
        persisted_ = false;
    }
}

bool ActivityTracker::load(std::chrono::system_clock::time_point now) {
    const auto path = activity_path(state_root_, key_);
    if (!path) {
        compatible_ = false;
        persisted_ = false;
        return false;
    }
    bool directory_missing = false;
    auto directory = open_directory_chain(
        path->parent_path(), false, &directory_missing);
    if (!directory) {
        compatible_ = directory_missing;
        persisted_ = directory_missing;
        partition_loaded_ = directory_missing;
        return directory_missing;
    }
    if (!owner_only_directory(directory->get())) {
        compatible_ = false;
        persisted_ = false;
        return false;
    }
    struct stat path_status {};
    if (::fstatat(directory->get(), path->filename().c_str(), &path_status,
                  AT_SYMLINK_NOFOLLOW) != 0) {
        const bool file_missing = errno == ENOENT;
        compatible_ = file_missing;
        persisted_ = file_missing;
        partition_loaded_ = file_missing;
        return file_missing;
    }
    const auto bytes = read_owner_only_file(*path, kMaximumActivityBytes);
    if (!bytes) {
        compatible_ = false;
        persisted_ = false;
        return false;
    }
    QJsonParseError parse_error;
    const QJsonDocument document =
        QJsonDocument::fromJson(*bytes, &parse_error);
    if (parse_error.error != QJsonParseError::NoError ||
        !document.isObject()) {
        compatible_ = false;
        persisted_ = false;
        return false;
    }
    const QJsonObject root = document.object();
    if (root.value("schema").toInt(-1) != 1 ||
        !root.value("events").isArray() ||
        !root.value("abilities").isArray()) {
        compatible_ = false;
        persisted_ = false;
        return false;
    }
    const QJsonArray event_values = root.value("events").toArray();
    const QJsonArray ability_values = root.value("abilities").toArray();
    const QJsonValue replay_value = root.value("replaySources");
    const QJsonValue generation_value = root.value("generationCounter");
    const auto loaded_generation_counter = generation_value.isUndefined()
                                               ? std::optional<std::uint64_t>{0U}
                                               : json_uint64(generation_value);
    if (event_values.size() > static_cast<qsizetype>(maximum_events) ||
        ability_values.size() > static_cast<qsizetype>(maximum_abilities) ||
        (!replay_value.isUndefined() && !replay_value.isArray()) ||
        !loaded_generation_counter) {
        compatible_ = false;
        persisted_ = false;
        return false;
    }
    std::vector<ActivityEventSnapshot> loaded_events;
    std::unordered_set<std::string> loaded_source_ids;
    loaded_events.reserve(static_cast<std::size_t>(event_values.size()));
    for (const QJsonValue& value : event_values) {
        auto event = event_from_json(value);
        const auto future_limit =
            std::chrono::duration_cast<std::chrono::seconds>(
                (now + std::chrono::hours(24))
                    .time_since_epoch())
                .count();
        if (!event || event->timestamp_unix_seconds <= 0 ||
            event->timestamp_unix_seconds > future_limit ||
            (event->source_id.empty() &&
             (event->kind == ActivityEventKind::experience ||
              event->kind == ActivityEventKind::alternate_advancement ||
              event->kind == ActivityEventKind::loot)) ||
            (!event->source_id.empty() &&
             !source_fingerprint(event->source_id)) ||
            (!event->source_id.empty() &&
             !loaded_source_ids.insert(event->source_id).second)) {
            compatible_ = false;
            persisted_ = false;
            return false;
        }
        loaded_events.push_back(std::move(*event));
    }
    std::unordered_map<std::string, AbilityAggregate> loaded_abilities;
    for (const QJsonValue& value : ability_values) {
        if (!value.isObject()) {
            compatible_ = false;
            persisted_ = false;
            return false;
        }
        const QJsonObject object = value.toObject();
        const std::string name = object.value("name").toString().toStdString();
        const std::string category_name =
            object.value("category").toString().toStdString();
        const QJsonValue observations_value = object.value("observations");
        const auto future_limit =
            std::chrono::duration_cast<std::chrono::seconds>(
                (now + std::chrono::hours(24)).time_since_epoch())
                .count();
        if (!valid_text(name) || !valid_ability_category(category_name) ||
            !observations_value.isArray()) {
            compatible_ = false;
            persisted_ = false;
            return false;
        }
        const QJsonArray observation_values = observations_value.toArray();
        if (observation_values.empty() ||
            observation_values.size() >
                static_cast<qsizetype>(maximum_ability_observations)) {
            compatible_ = false;
            persisted_ = false;
            return false;
        }
        AbilityAggregate aggregate{
            .name = name,
            .category = category_name,
            .observations = {},
        };
        for (const QJsonValue& observation_value : observation_values) {
            if (!observation_value.isObject()) {
                compatible_ = false;
                persisted_ = false;
                return false;
            }
            const QJsonObject observation = observation_value.toObject();
            const std::string source_id =
                observation.value("sourceId").toString().toStdString();
            const auto damage = json_uint64(observation.value("damage"));
            const auto timestamp =
                json_int64(observation.value("timestamp"));
            if (!valid_text(source_id, 64U) ||
                !source_fingerprint(source_id) || !damage || !timestamp ||
                *timestamp <= 0 || *timestamp > future_limit ||
                !loaded_source_ids.insert(source_id).second ||
                std::ranges::find(
                    aggregate.observations, source_id,
                    &AbilityAggregate::Observation::source_id) !=
                    aggregate.observations.end()) {
                compatible_ = false;
                persisted_ = false;
                return false;
            }
            aggregate.observations.push_back({
                .source_id = source_id,
                .damage = *damage,
                .timestamp_unix_seconds = *timestamp,
            });
        }
        const std::string aggregate_key =
            lowercase(name) + "\n" + category_name;
        if (!loaded_abilities
                 .emplace(aggregate_key, std::move(aggregate))
                 .second) {
            compatible_ = false;
            persisted_ = false;
            return false;
        }
    }
    std::size_t loaded_observations = 0U;
    for (const auto& [name, aggregate] : loaded_abilities) {
        (void)name;
        loaded_observations += aggregate.observations.size();
    }
    if (loaded_observations > maximum_ability_observations) {
        compatible_ = false;
        persisted_ = false;
        return false;
    }
    std::unordered_map<std::string, std::int64_t> loaded_source_timestamps;
    for (const auto& event : loaded_events) {
        if (!event.source_id.empty()) {
            loaded_source_timestamps.emplace(
                event.source_id, event.timestamp_unix_seconds);
        }
    }
    for (const auto& [name, aggregate] : loaded_abilities) {
        (void)name;
        for (const auto& observation : aggregate.observations) {
            loaded_source_timestamps.emplace(
                observation.source_id,
                observation.timestamp_unix_seconds);
        }
    }
    std::vector<ReplaySource> loaded_replay_history;
    bool replay_migration_needed =
        replay_value.isUndefined() || generation_value.isUndefined();
    const auto replay_future_limit =
        std::chrono::duration_cast<std::chrono::seconds>(
            (now + std::chrono::hours(24)).time_since_epoch())
            .count();
    if (replay_value.isArray()) {
        const QJsonArray replay_values = replay_value.toArray();
        if (replay_values.size() >
            static_cast<qsizetype>(kMaximumReplaySources)) {
            compatible_ = false;
            persisted_ = false;
            return false;
        }
        loaded_replay_history.reserve(
            static_cast<std::size_t>(replay_values.size()));
        std::unordered_set<std::string> unique_replay_sources;
        for (const QJsonValue& value : replay_values) {
            std::string source_id;
            std::optional<std::int64_t> timestamp;
            const bool timestamp_required = value.isObject();
            if (value.isObject()) {
                const QJsonObject object = value.toObject();
                source_id = object.value("sourceId").toString().toStdString();
                timestamp = json_int64(object.value("timestamp"));
            } else if (value.isString()) {
                replay_migration_needed = true;
                source_id = value.toString().toStdString();
                const auto found = loaded_source_timestamps.find(source_id);
                if (found != loaded_source_timestamps.end()) {
                    timestamp = found->second;
                }
            }
            if (!valid_text(source_id, 64U) ||
                !source_fingerprint(source_id) ||
                (timestamp_required && !timestamp) ||
                (timestamp &&
                 (*timestamp <= 0 || *timestamp > replay_future_limit)) ||
                !unique_replay_sources.insert(source_id).second) {
                compatible_ = false;
                persisted_ = false;
                return false;
            }
            if (timestamp) {
                loaded_replay_history.push_back({
                    .source_id = std::move(source_id),
                    .timestamp_unix_seconds = *timestamp,
                });
            }
        }
    } else {
        std::vector<std::pair<std::int64_t, std::string>> legacy_sources;
        legacy_sources.reserve(loaded_events.size() + loaded_observations);
        for (const auto& event : loaded_events) {
            if (!event.source_id.empty()) {
                legacy_sources.emplace_back(
                    event.timestamp_unix_seconds, event.source_id);
            }
        }
        for (const auto& [name, aggregate] : loaded_abilities) {
            (void)name;
            for (const auto& observation : aggregate.observations) {
                legacy_sources.emplace_back(
                    observation.timestamp_unix_seconds,
                    observation.source_id);
            }
        }
        std::ranges::sort(legacy_sources);
        if (legacy_sources.size() > kMaximumReplaySources) {
            legacy_sources.erase(
                legacy_sources.begin(),
                legacy_sources.begin() + static_cast<std::ptrdiff_t>(
                    legacy_sources.size() - kMaximumReplaySources));
        }
        loaded_replay_history.reserve(legacy_sources.size());
        for (auto& [timestamp, source_id] : legacy_sources) {
            loaded_replay_history.push_back({
                .source_id = std::move(source_id),
                .timestamp_unix_seconds = timestamp,
            });
        }
    }
    events_ = std::move(loaded_events);
    abilities_ = std::move(loaded_abilities);
    replay_history_ = std::move(loaded_replay_history);
    generation_counter_ = *loaded_generation_counter;
    dirty_ = replay_migration_needed;
    persisted_ = !replay_migration_needed;
    compatible_ = true;
    partition_loaded_ = true;
    prune(now);
    if (dirty_) {
        (void)persist();
    }
    return compatible_;
}

bool ActivityTracker::persist() {
    if (persistence_deferred_) {
        persisted_ = false;
        return false;
    }
    persisted_ = save();
    persistence_retry_after_ =
        persisted_ ? std::chrono::steady_clock::time_point{}
                   : std::chrono::steady_clock::now() +
                         persistence_retry_delay;
    return persisted_;
}

bool ActivityTracker::save() {
    const auto path = activity_path(state_root_, key_);
    if (!path || !compatible_) {
        persisted_ = false;
        return false;
    }
    bool directory_created = false;
    auto directory = open_directory_chain(
        path->parent_path(), true, nullptr, &directory_created);
    if (!directory ||
        (directory_created && ::fchmod(directory->get(), 0700) != 0) ||
        !owner_only_directory(directory->get())) {
        persisted_ = false;
        return false;
    }
    QJsonArray event_values;
    for (const auto& event : events_) {
        event_values.push_back(event_json(event));
    }
    QJsonArray ability_values;
    std::vector<const AbilityAggregate*> ordered_abilities;
    ordered_abilities.reserve(abilities_.size());
    for (const auto& [key, aggregate] : abilities_) {
        (void)key;
        ordered_abilities.push_back(&aggregate);
    }
    std::ranges::sort(ordered_abilities, [](const auto* left,
                                           const auto* right) {
        const std::string left_key =
            lowercase(left->name) + "\n" + left->category;
        const std::string right_key =
            lowercase(right->name) + "\n" + right->category;
        return left_key < right_key;
    });
    for (const AbilityAggregate* aggregate : ordered_abilities) {
        QJsonArray observations;
        for (const auto& observation : aggregate->observations) {
            observations.push_back(QJsonObject{
                {"sourceId", QString::fromStdString(observation.source_id)},
                {"damage", QString::number(observation.damage)},
                {"timestamp",
                 QString::number(observation.timestamp_unix_seconds)},
            });
        }
        ability_values.push_back(QJsonObject{
            {"name", QString::fromStdString(aggregate->name)},
            {"category", QString::fromStdString(aggregate->category)},
            {"observations", observations},
        });
    }
    QJsonArray replay_values;
    for (const auto& source : replay_history_) {
        replay_values.push_back(QJsonObject{
            {"sourceId", QString::fromStdString(source.source_id)},
            {"timestamp", QString::number(source.timestamp_unix_seconds)},
        });
    }
    const QByteArray bytes = QJsonDocument(QJsonObject{
        {"schema", 1},
        {"generationCounter", QString::number(generation_counter_)},
        {"events", event_values},
        {"abilities", ability_values},
        {"replaySources", replay_values},
    }).toJson(QJsonDocument::Compact);
    if (bytes.size() > static_cast<qsizetype>(kMaximumActivityBytes) ||
        !write_owner_only(*path, bytes, true)) {
        persisted_ = false;
        return false;
    }
    dirty_ = false;
    persisted_ = true;
    return true;
}

void ActivityTracker::maintain_store(
    std::chrono::system_clock::time_point now) {
    if (now < sweep_after_) {
        return;
    }
    const auto activity_directory = state_root_ / "activity";
    bool directory_missing = false;
    auto directory = open_directory_chain(
        activity_directory, false, &directory_missing);
    if (!directory) {
        last_swept_key_.clear();
        sweep_after_ = now + (directory_missing ? std::chrono::hours(24)
                                                 : std::chrono::hours(1));
        return;
    }
    if (!owner_only_directory(directory->get())) {
        last_swept_key_.clear();
        sweep_after_ = now + std::chrono::hours(1);
        return;
    }
    const int listing_descriptor = ::dup(directory->get());
    DIR* listing = listing_descriptor >= 0
                       ? ::fdopendir(listing_descriptor)
                       : nullptr;
    if (listing == nullptr) {
        if (listing_descriptor >= 0) {
            (void)::close(listing_descriptor);
        }
        last_swept_key_.clear();
        sweep_after_ = now + std::chrono::hours(1);
        return;
    }
    std::vector<std::string> files;
    errno = 0;
    while (const dirent* entry = ::readdir(listing)) {
        const std::string name = entry->d_name;
        if (name != "." && name != "..") {
            files.push_back(name);
            if (files.size() > static_cast<std::size_t>(kMaximumActivityFiles)) {
                break;
            }
        }
        errno = 0;
    }
    const bool listing_failed = errno != 0 || ::closedir(listing) != 0;
    if (listing_failed ||
        files.size() > static_cast<std::size_t>(kMaximumActivityFiles)) {
        last_swept_key_.clear();
        sweep_after_ = now + std::chrono::hours(1);
        return;
    }
    std::ranges::sort(files);
    std::size_t processed = 0U;
    for (const std::string& name : files) {
        if (!name.ends_with(".json")) {
            continue;
        }
        const std::string candidate =
            name.substr(0U, name.size() - std::string_view(".json").size());
        if (!last_swept_key_.empty() && candidate <= last_swept_key_) {
            continue;
        }
        last_swept_key_ = candidate;
        struct stat status {};
        if (::fstatat(directory->get(), name.c_str(), &status,
                      AT_SYMLINK_NOFOLLOW) != 0 ||
            !owner_only_file(status) || candidate.size() != 32U ||
            !std::ranges::all_of(candidate, [](unsigned char byte) {
                return (byte >= '0' && byte <= '9') ||
                       (byte >= 'a' && byte <= 'f');
            }) || (retention_enabled_ && candidate == key_)) {
            continue;
        }
        ActivityTracker partition(state_root_);
        partition.retention_enabled_ = true;
        partition.key_ = candidate;
        partition.sweep_after_ = std::chrono::system_clock::time_point::max();
        (void)partition.load(now);
        ++processed;
        if (processed == maximum_sweep_partitions) {
            sweep_after_ = now + std::chrono::seconds(1);
            return;
        }
    }
    last_swept_key_.clear();
    sweep_after_ = now + std::chrono::hours(24);
}

void ActivityTracker::maintain(
    std::chrono::system_clock::time_point now) {
    prune(now);
    if (!persistence_deferred_ && retention_enabled_ && dirty_ &&
        std::chrono::steady_clock::now() >= persistence_retry_after_) {
        (void)persist();
    }
    if (!persistence_deferred_) {
        maintain_store(now);
    }
}

ActivityAnalyticsSnapshot ActivityTracker::snapshot(
    std::chrono::system_clock::time_point now) {
    maintain(now);
    if (!compatible_) {
        return unavailable_snapshot(
            "Stored activity uses an unsupported or invalid schema");
    }
    ActivityAnalyticsSnapshot result{
        .storage_key = key_,
        .events = events_,
        .abilities = {},
        .experience_percent = 0.0,
        .experience_percent_per_hour = 0.0,
        .level_pace_hours = std::nullopt,
        .alternate_advancement_percent =
            current_alternate_advancement_percent_,
        .alternate_advancement_points = std::nullopt,
        .alternate_advancement_points_per_hour = 0.0,
        .next_alternate_advancement_hours = std::nullopt,
        .recent_loot_count = 0U,
        .class_activity_summary = {},
        .recent_celebration = std::nullopt,
        .retention_enabled = retention_enabled_,
        .persisted = retention_enabled_ ? persisted_ : true,
        .available = !key_.empty(),
        .detail = "Waiting for progression or activity events",
    };
    const auto now_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch())
            .count();
    const std::int64_t rate_cutoff = now_seconds - 3600;
    const std::int64_t loot_cutoff = now_seconds - 24 * 3600;
    std::int64_t first_experience = 0;
    std::int64_t first_alternate_advancement = 0;
    std::int64_t latest_aa = 0;
    double recent_experience = 0.0;
    double aa_gains = 0.0;
    std::int64_t latest_celebration = 0;
    for (const auto& event : events_) {
        if (event.kind == ActivityEventKind::experience) {
            result.experience_percent += event.amount;
        } else if (event.kind ==
                   ActivityEventKind::alternate_advancement) {
            if (event.total && event.timestamp_unix_seconds >= latest_aa) {
                result.alternate_advancement_points = *event.total;
                latest_aa = event.timestamp_unix_seconds;
            }
        } else if (event.kind == ActivityEventKind::loot &&
                   event.timestamp_unix_seconds >= loot_cutoff &&
                   event.timestamp_unix_seconds <= now_seconds) {
            ++result.recent_loot_count;
        }
        if (event.kind == ActivityEventKind::experience &&
            event.timestamp_unix_seconds >= rate_cutoff &&
            event.timestamp_unix_seconds <= now_seconds) {
            recent_experience += event.amount;
            if (first_experience == 0 ||
                event.timestamp_unix_seconds < first_experience) {
                first_experience = event.timestamp_unix_seconds;
            }
        }
        if (event.kind == ActivityEventKind::alternate_advancement &&
            event.timestamp_unix_seconds >= rate_cutoff &&
            event.timestamp_unix_seconds <= now_seconds) {
            aa_gains += event.amount;
            if (first_alternate_advancement == 0 ||
                event.timestamp_unix_seconds <
                    first_alternate_advancement) {
                first_alternate_advancement =
                    event.timestamp_unix_seconds;
            }
        }
        if ((event.kind == ActivityEventKind::experience ||
             event.kind == ActivityEventKind::alternate_advancement ||
             event.kind == ActivityEventKind::loot ||
             event.kind == ActivityEventKind::equipment_change) &&
            event.timestamp_unix_seconds >= latest_celebration) {
            latest_celebration = event.timestamp_unix_seconds;
            switch (event.kind) {
                case ActivityEventKind::experience:
                    result.recent_celebration = "Experience gained";
                    break;
                case ActivityEventKind::alternate_advancement:
                    result.recent_celebration = event.label;
                    break;
                case ActivityEventKind::loot:
                    result.recent_celebration = "Loot: " + event.label;
                    break;
                case ActivityEventKind::equipment_change:
                    result.recent_celebration =
                        "Equipment change: " + event.label;
                    break;
                case ActivityEventKind::celebration:
                    result.recent_celebration = event.label;
                    break;
            }
        }
    }
    if (first_experience > 0 && now_seconds - first_experience >= 60) {
        const double hours =
            static_cast<double>(now_seconds - first_experience) / 3600.0;
        result.experience_percent_per_hour =
            recent_experience / hours;
        if (result.experience_percent_per_hour > 0.0) {
            result.level_pace_hours =
                100.0 / result.experience_percent_per_hour;
        }
    }
    if (first_alternate_advancement > 0 &&
        now_seconds - first_alternate_advancement >= 60) {
        const double hours =
            static_cast<double>(now_seconds -
                                first_alternate_advancement) /
            3600.0;
        result.alternate_advancement_points_per_hour =
            aa_gains / hours;
        if (result.alternate_advancement_points_per_hour > 0.0) {
            result.next_alternate_advancement_hours =
                1.0 / result.alternate_advancement_points_per_hour;
        }
    }
    result.abilities.reserve(abilities_.size());
    for (const auto& [key, aggregate] : abilities_) {
        (void)key;
        std::uint64_t damage = 0U;
        for (const auto& observation : aggregate.observations) {
            damage += std::min(
                std::numeric_limits<std::uint64_t>::max() - damage,
                observation.damage);
        }
        result.abilities.push_back({
            .name = aggregate.name,
            .category = aggregate.category,
            .damage = damage,
            .observations = static_cast<std::uint32_t>(
                std::min<std::size_t>(
                    aggregate.observations.size(),
                    std::numeric_limits<std::uint32_t>::max())),
            .confidence =
                "Observed log damage; proc or class identity is unconfirmed",
        });
    }
    std::ranges::sort(result.abilities, [](const auto& left,
                                           const auto& right) {
        if (left.damage != right.damage) {
            return left.damage > right.damage;
        }
        return lowercase(left.name) < lowercase(right.name);
    });
    const auto equipped = std::ranges::count_if(
        equipment_, [](const EquipmentSlotSnapshot& slot) {
            return !slot.item.empty();
        });
    result.class_activity_summary =
        std::to_string(equipped) + " equipped slot(s), " +
        std::to_string(result.abilities.size()) +
        " observed ability source(s); class combination unconfirmed";
    if (current_alternate_advancement_points_) {
        result.alternate_advancement_points =
            current_alternate_advancement_points_;
    }
    if (!events_.empty() || !abilities_.empty()) {
        result.detail = compatible_
                            ? "Local observations only; ambiguous class and "
                              "proc labels remain unclassified"
                            : result.detail;
    }
    if (retention_enabled_ && !persisted_ && compatible_) {
        result.detail += "; activity history could not be saved";
    }
    return result;
}

ActivityAnalyticsSnapshot ActivityTracker::unavailable_snapshot(
    std::string detail) const {
    ActivityAnalyticsSnapshot result;
    result.storage_key = key_;
    result.retention_enabled = retention_enabled_;
    result.persisted = retention_enabled_ ? persisted_ : true;
    result.available = false;
    result.detail = "Activity unavailable: " + std::move(detail);
    return result;
}

bool save_activity_export(
    const std::filesystem::path& path,
    const ActivityAnalyticsSnapshot& snapshot) {
    if (path.empty() || snapshot.events.size() > ActivityTracker::maximum_events ||
        snapshot.abilities.size() > ActivityTracker::maximum_abilities) {
        return false;
    }
    QJsonArray events;
    for (const auto& event : snapshot.events) {
        events.push_back(event_json(event));
    }
    QJsonArray abilities;
    for (const auto& ability : snapshot.abilities) {
        if (!valid_text(ability.name) || !valid_text(ability.category, 64U) ||
            !valid_text(ability.confidence)) {
            return false;
        }
        abilities.push_back(ability_json(
            ability.name, ability.category, ability.damage,
            ability.observations, ability.confidence));
    }
    const QByteArray bytes = QJsonDocument(QJsonObject{
        {"schema", 1},
        {"type", "plazmic-activity-export"},
        {"events", events},
        {"abilities", abilities},
    }).toJson(QJsonDocument::Indented);
    return bytes.size() <= static_cast<qsizetype>(kMaximumActivityBytes) &&
           write_owner_only(path, bytes);
}

InventoryReconciliationSnapshot reconcile_inventory_entries(
    std::vector<InventoryEntrySnapshot> entries,
    std::string source_name,
    const CharacterSnapshot& character) {
    InventoryReconciliationSnapshot result{
        .entries = std::move(entries),
        .equipped_not_in_import = {},
        .imported_equipped_items = {},
        .source_name = std::move(source_name),
        .detail = "Select an EverQuest inventory output file",
        .available = false,
    };
    if (!valid_text(result.source_name)) {
        result.entries.clear();
        result.source_name.clear();
        result.detail = "Inventory output has an invalid file name";
        return result;
    }
    if (!character.available()) {
        result.detail =
            "Inventory imported; live equipment is unavailable for comparison";
    } else {
        std::unordered_set<std::string> imported_names;
        for (const auto& entry : result.entries) {
            imported_names.insert(lowercase(entry.item));
        }
        for (const auto& slot : character.equipment) {
            if (slot.item.empty()) {
                continue;
            }
            if (imported_names.contains(lowercase(slot.item))) {
                result.imported_equipped_items.push_back(
                    slot.slot + ": " + slot.item);
            } else {
                result.equipped_not_in_import.push_back(
                    slot.slot + ": " + slot.item);
            }
        }
        result.detail = result.equipped_not_in_import.empty()
                            ? "Imported inventory matches all visible equipment"
                            : "Imported inventory is missing visible equipment";
    }
    result.available = true;
    return result;
}

InventoryReconciliationSnapshot import_inventory_output(
    const std::filesystem::path& path,
    const CharacterSnapshot& character) {
    InventoryReconciliationSnapshot result;
    const int descriptor =
        ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) {
        result.detail = "Inventory output must be a regular local file";
        return result;
    }
    struct stat information {};
    if (::fstat(descriptor, &information) != 0 ||
        !S_ISREG(information.st_mode)) {
        (void)::close(descriptor);
        result.detail = "Inventory output must be a regular local file";
        return result;
    }
    if (information.st_size < 0 ||
        static_cast<std::uintmax_t>(information.st_size) >
            kMaximumInventoryBytes) {
        (void)::close(descriptor);
        result.detail = "Inventory output exceeds the 2 MiB limit";
        return result;
    }
    QFile source_file;
    if (!source_file.open(
            descriptor, QIODevice::ReadOnly,
            QFileDevice::AutoCloseHandle)) {
        (void)::close(descriptor);
        result.detail = "Inventory output could not be opened";
        return result;
    }
    std::string input_bytes;
    input_bytes.reserve(static_cast<std::size_t>(information.st_size));
    std::array<char, 64U * 1024U> buffer{};
    while (true) {
        const std::size_t remaining =
            static_cast<std::size_t>(kMaximumInventoryBytes) + 1U -
            input_bytes.size();
        const qint64 received = source_file.read(
            buffer.data(), static_cast<qint64>(
                               std::min(buffer.size(), remaining)));
        if (received < 0) {
            result.detail = "Inventory output could not be read completely";
            return result;
        }
        if (received == 0) {
            break;
        }
        input_bytes.append(buffer.data(), static_cast<std::size_t>(received));
        if (input_bytes.size() > kMaximumInventoryBytes) {
            result.detail = "Inventory output exceeds the 2 MiB limit";
            return result;
        }
    }
    std::istringstream input(std::move(input_bytes));
    enum class InventorySection {
        none,
        items,
        key_ring,
    };
    InventorySection section = InventorySection::none;
    bool recognized_header = false;
    std::size_t row_count = 0U;
    std::string line;
    while (std::getline(input, line)) {
        ++row_count;
        if (row_count > kMaximumInventoryRows) {
            result.entries.clear();
            result.detail = "Inventory output exceeds its line or row limit";
            return result;
        }
        if (line.ends_with('\r')) {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (line.size() > kMaximumLineBytes) {
            result.entries.clear();
            result.detail = "Inventory output exceeds its line or row limit";
            return result;
        }
        const auto fields = split_tabs(line);
        if (fields.size() < 2U || !valid_text(fields[0])) {
            result.entries.clear();
            result.detail = "Inventory output contains a malformed row";
            return result;
        }
        const bool item_header =
            fields.size() == 5U && fields[0] == "Location" &&
            fields[1] == "Name" && fields[2] == "ID" &&
            fields[3] == "Count" && fields[4] == "Slots";
        const bool key_ring_header =
            fields.size() == 3U && fields[0] == "KeyRing" &&
            fields[1] == "Name" && fields[2] == "ID";
        if (item_header || key_ring_header) {
            if (item_header) {
                section = InventorySection::items;
                recognized_header = true;
            } else {
                section = InventorySection::key_ring;
                recognized_header = true;
            }
            continue;
        }
        if (section == InventorySection::none) {
            result.entries.clear();
            result.detail = "Inventory output is missing a recognized header";
            return result;
        }
        std::uint32_t quantity = 1U;
        if (section == InventorySection::items) {
            if (fields.size() != 5U) {
                result.entries.clear();
                result.detail = "Inventory output contains a malformed row";
                return result;
            }
            const auto parsed = parse_integer<std::uint32_t>(fields[3]);
            if (!parsed || *parsed > 1'000'000U) {
                result.entries.clear();
                result.detail = "Inventory output contains an invalid quantity";
                return result;
            }
            quantity = *parsed;
        } else if (fields.size() != 3U) {
            result.entries.clear();
            result.detail = "Inventory output contains a malformed row";
            return result;
        }
        if (quantity == 0U &&
            (fields[1].empty() || ci_equal(fields[1], "Empty"))) {
            continue;
        }
        if (quantity == 0U) {
            result.entries.clear();
            result.detail = "Inventory output contains an invalid quantity";
            return result;
        }
        if (!valid_text(fields[1])) {
            result.entries.clear();
            result.detail = "Inventory output contains a malformed row";
            return result;
        }
        result.entries.push_back({
            .location = std::string(fields[0]),
            .item = std::string(fields[1]),
            .quantity = quantity,
        });
    }
    if (!input.eof()) {
        result.entries.clear();
        result.detail = "Inventory output could not be read completely";
        return result;
    }
    if (!recognized_header) {
        result.entries.clear();
        result.detail = "Inventory output contains no recognized item section";
        return result;
    }
    return reconcile_inventory_entries(
        std::move(result.entries), path.filename().string(), character);
}

}  // namespace plazmic
