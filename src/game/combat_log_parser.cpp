#include "game/combat_log_parser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/stat.h>

namespace plazmic {
namespace {

constexpr std::uint64_t kMaximumDamage = 1'000'000'000'000ULL;
constexpr std::size_t kMaximumNameBytes = 128U;

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

std::string_view trim(std::string_view value) {
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1U);
    }
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1U);
    }
    return value;
}

std::string clean_name(std::string_view value,
                       std::string_view active_character) {
    value = trim(value);
    while (!value.empty() &&
           (value.back() == '.' || value.back() == '!' ||
            value.back() == ',')) {
        value.remove_suffix(1U);
    }
    if (ci_equal(value, "you") || ci_equal(value, "your")) {
        return std::string(active_character);
    }
    return std::string(value);
}

bool valid_name(std::string_view value) {
    if (value.empty() || value.size() > kMaximumNameBytes) {
        return false;
    }
    return std::ranges::all_of(value, [](unsigned char byte) {
        return byte >= 0x20U && byte <= 0x7eU;
    });
}

bool is_chat_payload(std::string_view payload) {
    if (payload.ends_with('\'') &&
        payload.find(", '") != std::string_view::npos) {
        return true;
    }
    constexpr std::array<std::string_view, 7> kChatMarkers{
        " says, '",
        " tells you, '",
        " told you, '",
        " shouts, '",
        " auctions, '",
        " says out of character, '",
        " tells the guild, '",
    };
    const std::string lower_payload = lowercase(payload);
    return std::ranges::any_of(
        kChatMarkers, [&lower_payload](const auto marker) {
            return lower_payload.find(marker) != std::string::npos;
        });
}

std::optional<std::uint64_t> parse_damage(std::string_view value) {
    value = trim(value);
    std::uint64_t damage = 0U;
    const auto result = std::from_chars(
        value.data(), value.data() + value.size(), damage);
    if (result.ec != std::errc{} ||
        result.ptr != value.data() + value.size() || damage == 0U ||
        damage > kMaximumDamage) {
        return std::nullopt;
    }
    return damage;
}

std::optional<std::chrono::system_clock::time_point> parse_timestamp(
    std::string_view line,
    std::string_view& payload) {
    if (line.size() < 28U || line.front() != '[') {
        return std::nullopt;
    }
    const std::size_t close = line.find("] ");
    if (close == std::string_view::npos || close < 20U || close > 32U) {
        return std::nullopt;
    }
    std::tm value{};
    value.tm_isdst = -1;
    std::istringstream input(std::string(line.substr(1U, close - 1U)));
    input.imbue(std::locale::classic());
    input >> std::get_time(&value, "%a %b %d %H:%M:%S %Y");
    if (input.fail()) {
        return std::nullopt;
    }
    const std::time_t converted = std::mktime(&value);
    if (converted == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }
    payload = line.substr(close + 2U);
    return std::chrono::system_clock::from_time_t(converted);
}

std::optional<DamageEvent> parse_damage_over_time(
    std::chrono::system_clock::time_point timestamp,
    std::string_view payload,
    std::string_view active_character) {
    constexpr std::string_view kTaken = " has taken ";
    constexpr std::string_view kDamageFrom = " damage from ";
    const std::size_t taken = payload.find(kTaken);
    if (taken == std::string_view::npos) {
        return std::nullopt;
    }
    const std::size_t from = payload.find(
        kDamageFrom, taken + kTaken.size());
    if (from == std::string_view::npos) {
        return std::nullopt;
    }
    const auto damage = parse_damage(payload.substr(
        taken + kTaken.size(),
        from - taken - kTaken.size()));
    if (!damage) {
        return std::nullopt;
    }

    std::string_view source = payload.substr(from + kDamageFrom.size());
    const std::size_t modifier = source.find(". (");
    if (modifier != std::string_view::npos) {
        source = source.substr(0U, modifier);
    }
    while (!source.empty() &&
           (source.back() == '.' || source.back() == '!')) {
        source.remove_suffix(1U);
    }
    std::string attacker;
    const std::size_t by = source.rfind(" by ");
    if (by != std::string_view::npos) {
        attacker = clean_name(source.substr(by + 4U), active_character);
    } else if (lowercase(source).starts_with("your ")) {
        attacker = std::string(active_character);
    } else {
        return std::nullopt;
    }
    std::string defender =
        clean_name(payload.substr(0U, taken), active_character);
    if (!valid_name(attacker) || !valid_name(defender)) {
        return std::nullopt;
    }
    return DamageEvent{
        .timestamp = timestamp,
        .attacker = std::move(attacker),
        .defender = std::move(defender),
        .damage = *damage,
        .kind = DamageKind::damage_over_time,
    };
}

std::optional<DamageEvent> parse_direct_damage(
    std::chrono::system_clock::time_point timestamp,
    std::string_view payload,
    std::string_view active_character) {
    constexpr std::array<std::string_view, 36> kVerbs{
        " hit ", " hits ", " slash ", " slashes ",
        " pierce ", " pierces ", " crush ", " crushes ",
        " kick ", " kicks ", " bash ", " bashes ",
        " bite ", " bites ", " claw ", " claws ",
        " maul ", " mauls ", " punch ", " punches ",
        " backstab ", " backstabs ", " strike ", " strikes ",
        " cleave ", " cleaves ", " smite ", " smites ",
        " reave ", " reaves ", " rend ", " rends ",
        " frenzy on ", " frenzies on ", " gore ", " gores ",
    };
    constexpr std::string_view kFor = " for ";
    constexpr std::array<std::string_view, 2> kDamageUnits{
        " points of ",
        " point of ",
    };
    std::size_t points = std::string_view::npos;
    for (const std::string_view unit : kDamageUnits) {
        const std::size_t found = payload.find(unit);
        if (found != std::string_view::npos &&
            (points == std::string_view::npos || found < points)) {
            points = found;
        }
    }
    if (points == std::string_view::npos) {
        return std::nullopt;
    }
    const std::size_t for_position = payload.rfind(kFor, points);
    if (for_position == std::string_view::npos) {
        return std::nullopt;
    }
    const auto damage = parse_damage(payload.substr(
        for_position + kFor.size(),
        points - for_position - kFor.size()));
    if (!damage) {
        return std::nullopt;
    }

    std::size_t verb_position = std::string_view::npos;
    std::string_view verb;
    for (const std::string_view candidate : kVerbs) {
        const std::size_t found = payload.find(candidate);
        if (found != std::string_view::npos && found < for_position &&
            (verb_position == std::string_view::npos ||
             found < verb_position)) {
            verb_position = found;
            verb = candidate;
        }
    }
    if (verb_position == std::string_view::npos) {
        return std::nullopt;
    }

    std::string attacker = clean_name(
        payload.substr(0U, verb_position), active_character);
    std::string defender = clean_name(
        payload.substr(
            verb_position + verb.size(),
            for_position - verb_position - verb.size()),
        active_character);
    if (!valid_name(attacker) || !valid_name(defender)) {
        return std::nullopt;
    }

    const std::string lower_suffix = lowercase(payload.substr(points));
    DamageKind kind = lower_suffix.find(" damage by ") != std::string::npos
                          ? DamageKind::spell
                          : DamageKind::melee;
    const auto possessive = attacker.find("'s ");
    const auto backtick_possessive = attacker.find("`s ");
    const std::size_t owner_end =
        possessive != std::string::npos ? possessive : backtick_possessive;
    if (owner_end != std::string::npos && owner_end > 0U) {
        attacker.resize(owner_end);
        if (kind == DamageKind::melee) {
            kind = DamageKind::pet;
        }
    } else if (ci_equal(attacker, "your pet")) {
        attacker = std::string(active_character);
        kind = DamageKind::pet;
    }
    const std::string lower_attacker = lowercase(attacker);
    const std::size_t owner = lower_attacker.find(" (owner: ");
    if (owner != std::string::npos && attacker.ends_with(')')) {
        const std::size_t owner_begin = owner + 9U;
        attacker = clean_name(
            std::string_view(attacker).substr(
                owner_begin, attacker.size() - owner_begin - 1U),
            active_character);
        kind = DamageKind::pet;
    }
    return DamageEvent{
        .timestamp = timestamp,
        .attacker = std::move(attacker),
        .defender = std::move(defender),
        .damage = *damage,
        .kind = kind,
    };
}

double duration_seconds(
    std::chrono::system_clock::time_point first,
    std::chrono::system_clock::time_point last) {
    const double elapsed =
        std::chrono::duration<double>(last - first).count();
    return std::max(1.0, elapsed + 1.0);
}

bool valid_character(std::string_view value) {
    return valid_name(value) &&
           value.find('/') == std::string_view::npos &&
           value.find('\\') == std::string_view::npos;
}

std::optional<CombatLogTailer::FileIdentity> stat_identity(
    const std::filesystem::path& path) {
    struct stat status {};
    if (::stat(path.c_str(), &status) != 0 || !S_ISREG(status.st_mode)) {
        return std::nullopt;
    }
    return CombatLogTailer::FileIdentity{
        .device = static_cast<std::uint64_t>(status.st_dev),
        .inode = static_cast<std::uint64_t>(status.st_ino),
    };
}

std::optional<std::string> read_boundary(
    const std::filesystem::path& path,
    std::uintmax_t offset) {
    if (offset == 0U) {
        return std::string{};
    }
    const std::uintmax_t count = std::min<std::uintmax_t>(
        offset, CombatLogTailer::boundary_bytes);
    if (offset > static_cast<std::uintmax_t>(
                     std::numeric_limits<std::streamoff>::max())) {
        return std::nullopt;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    input.seekg(static_cast<std::streamoff>(offset - count));
    std::string result(static_cast<std::size_t>(count), '\0');
    input.read(result.data(), static_cast<std::streamsize>(count));
    if (static_cast<std::uintmax_t>(input.gcount()) != count) {
        return std::nullopt;
    }
    return result;
}

std::optional<std::string> read_prefix(
    const std::filesystem::path& path,
    std::uintmax_t size) {
    const std::uintmax_t count = std::min<std::uintmax_t>(
        size, CombatLogTailer::boundary_bytes);
    if (count == 0U) {
        return std::string{};
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    std::string result(static_cast<std::size_t>(count), '\0');
    input.read(result.data(), static_cast<std::streamsize>(count));
    if (static_cast<std::uintmax_t>(input.gcount()) != count) {
        return std::nullopt;
    }
    return result;
}

}  // namespace

std::optional<DamageEvent> parse_damage_line(
    std::string_view line,
    std::string_view active_character) {
    if (line.empty() || line.size() > CombatLogTailer::maximum_line_bytes ||
        !valid_character(active_character)) {
        return std::nullopt;
    }
    std::string_view payload;
    const auto timestamp = parse_timestamp(line, payload);
    if (!timestamp || is_chat_payload(payload)) {
        return std::nullopt;
    }
    if (auto event = parse_damage_over_time(
            *timestamp, payload, active_character)) {
        return event;
    }
    return parse_direct_damage(*timestamp, payload, active_character);
}

void CombatAccumulator::clear() {
    participants_.clear();
    target_.clear();
    total_damage_ = 0U;
    first_ = {};
    last_ = {};
}

bool CombatAccumulator::add(const DamageEvent& event,
                            std::string_view active_character) {
    if (!valid_name(event.attacker) || !valid_name(event.defender) ||
        event.damage == 0U || event.damage > kMaximumDamage) {
        return false;
    }
    const bool outside_encounter =
        !participants_.empty() &&
        (event.timestamp < first_ || event.timestamp - last_ > inactivity);
    if (outside_encounter) {
        if (!active_character.empty() &&
            !ci_equal(event.attacker, active_character)) {
            return false;
        }
        clear();
    }
    if (participants_.empty() && !active_character.empty() &&
        !ci_equal(event.attacker, active_character)) {
        return false;
    }
    if (!participants_.empty() && !ci_equal(event.defender, target_)) {
        return false;
    }
    auto found = participants_.find(event.attacker);
    if (found == participants_.end() &&
        participants_.size() >= maximum_participants) {
        return false;
    }
    if (total_damage_ >
        std::numeric_limits<std::uint64_t>::max() - event.damage) {
        return false;
    }
    if (participants_.empty()) {
        first_ = event.timestamp;
        last_ = event.timestamp;
        target_ = event.defender;
    }
    auto [iterator, inserted] = participants_.try_emplace(event.attacker);
    Participant& participant = iterator->second;
    if (inserted) {
        participant.first = event.timestamp;
        participant.last = event.timestamp;
    }
    participant.last = std::max(participant.last, event.timestamp);
    participant.first = std::min(participant.first, event.timestamp);
    participant.damage += event.damage;
    ++participant.hits;
    const auto kind_index = static_cast<std::size_t>(event.kind);
    participant.kind_damage.at(kind_index) += event.damage;
    total_damage_ += event.damage;
    last_ = std::max(last_, event.timestamp);
    return true;
}

CombatEncounterSnapshot CombatAccumulator::snapshot(
    std::chrono::system_clock::time_point now,
    std::string_view active_character) const {
    if (participants_.empty()) {
        return {
            .state = CombatEncounterState::idle,
            .target = {},
            .participants = {},
            .total_damage = 0U,
            .duration_seconds = 0.0,
            .active_character_dps = 0.0,
            .detail = "Waiting for combat",
        };
    }
    CombatEncounterSnapshot result{
        .state = now - last_ > inactivity
                     ? CombatEncounterState::complete
                     : CombatEncounterState::active,
        .target = target_,
        .participants = {},
        .total_damage = total_damage_,
        .duration_seconds = duration_seconds(first_, last_),
        .active_character_dps = 0.0,
        .detail = now - last_ > inactivity
                      ? "Most recent encounter"
                      : "Current encounter",
    };
    result.participants.reserve(participants_.size());
    for (const auto& [name, participant] : participants_) {
        const double active =
            duration_seconds(participant.first, participant.last);
        CombatParticipantSnapshot value{
            .name = name,
            .damage = participant.damage,
            .hits = participant.hits,
            .dps = static_cast<double>(participant.damage) / active,
            .percentage = total_damage_ == 0U
                              ? 0.0
                              : static_cast<double>(participant.damage) *
                                    100.0 /
                                    static_cast<double>(total_damage_),
            .active_seconds = active,
        };
        if (ci_equal(name, active_character)) {
            result.active_character_dps = value.dps;
        }
        result.participants.push_back(std::move(value));
    }
    std::ranges::sort(
        result.participants,
        [](const CombatParticipantSnapshot& left,
           const CombatParticipantSnapshot& right) {
            if (left.damage != right.damage) {
                return left.damage > right.damage;
            }
            return lowercase(left.name) < lowercase(right.name);
        });
    return result;
}

CombatLogRefresh CombatLogTailer::failure(
    CombatLogError error,
    std::string detail) const {
    return {
        .snapshot = {
            .state = CombatEncounterState::unavailable,
            .target = {},
            .participants = {},
            .total_damage = 0U,
            .duration_seconds = 0.0,
            .active_character_dps = 0.0,
            .detail = std::move(detail),
        },
        .error = error,
    };
}

std::optional<std::filesystem::path> CombatLogTailer::select_log(
    const std::filesystem::path& game_directory,
    std::string_view active_character,
    CombatLogError& error) const {
    std::filesystem::path directory = game_directory / "Logs";
    std::error_code filesystem_error;
    if (!std::filesystem::is_directory(directory, filesystem_error)) {
        filesystem_error.clear();
        directory = game_directory / "logs";
    }
    if (!std::filesystem::is_directory(directory, filesystem_error)) {
        error = CombatLogError::missing;
        return std::nullopt;
    }

    const std::string prefix =
        "eqlog_" + lowercase(active_character) + "_";
    std::vector<std::filesystem::path> matches;
    std::size_t inspected = 0U;
    for (std::filesystem::directory_iterator iterator(
             directory,
             std::filesystem::directory_options::skip_permission_denied,
             filesystem_error),
         end;
         iterator != end && !filesystem_error;
         iterator.increment(filesystem_error)) {
        ++inspected;
        if (inspected > maximum_log_entries) {
            error = CombatLogError::unavailable;
            return std::nullopt;
        }
        const std::filesystem::file_status status =
            iterator->symlink_status(filesystem_error);
        if (filesystem_error) {
            error = CombatLogError::unavailable;
            return std::nullopt;
        }
        if (!std::filesystem::is_regular_file(status)) {
            continue;
        }
        const std::string filename =
            lowercase(iterator->path().filename().string());
        if (filename.starts_with(prefix) && filename.ends_with(".txt")) {
            matches.push_back(iterator->path());
            if (matches.size() > 1U) {
                error = CombatLogError::ambiguous;
                return std::nullopt;
            }
        }
    }
    if (filesystem_error) {
        error = CombatLogError::unavailable;
        return std::nullopt;
    }
    if (matches.empty()) {
        error = CombatLogError::missing;
        return std::nullopt;
    }
    error = CombatLogError::none;
    return matches.front();
}

bool CombatLogTailer::consume(
    char byte,
    std::string_view active_character) {
    if (byte == '\r') {
        return true;
    }
    if (byte == '\n') {
        if (dropping_line_) {
            oversized_line_seen_ = true;
        }
        if (!dropping_line_ && !partial_line_.empty() &&
            lines_this_refresh_ < maximum_lines_per_refresh) {
            if (const auto event =
                    parse_damage_line(partial_line_, active_character);
                event && !ci_equal(event->defender, active_character)) {
                (void)accumulator_.add(*event, active_character);
            }
            ++lines_this_refresh_;
        }
        partial_line_.clear();
        dropping_line_ = false;
        return lines_this_refresh_ < maximum_lines_per_refresh;
    }
    if (dropping_line_) {
        return true;
    }
    if (partial_line_.size() >= maximum_line_bytes) {
        partial_line_.clear();
        dropping_line_ = true;
        oversized_line_seen_ = true;
        accumulator_.clear();
        return true;
    }
    partial_line_.push_back(byte);
    return true;
}

CombatLogRefresh CombatLogTailer::refresh(
    const std::filesystem::path& game_directory,
    std::string_view active_character,
    std::chrono::system_clock::time_point now) {
    if (!valid_character(active_character)) {
        clear();
        return failure(
            CombatLogError::invalid_character,
            "Combat log unavailable for the active character");
    }
    if (!ci_equal(character_, active_character)) {
        clear();
        character_ = std::string(active_character);
    }
    CombatLogError selection_error = CombatLogError::none;
    const auto selected = select_log(
        game_directory, active_character, selection_error);
    if (!selected) {
        return failure(
            selection_error,
            selection_error == CombatLogError::ambiguous
                ? "Multiple combat logs match the active character"
                : "Combat logging is unavailable");
    }
    if (path_ != *selected) {
        const bool replacing_stream =
            identity_.has_value() || !path_.empty();
        path_ = *selected;
        identity_.reset();
        offset_ = 0U;
        partial_line_.clear();
        dropping_line_ = false;
        oversized_line_seen_ = false;
        prefix_boundary_.clear();
        boundary_.clear();
        reopen_from_start_ = replacing_stream;
        accumulator_.clear();
    }

    std::error_code filesystem_error;
    const std::uintmax_t size =
        std::filesystem::file_size(path_, filesystem_error);
    const auto current_identity = stat_identity(path_);
    if (filesystem_error || !current_identity) {
        return failure(
            CombatLogError::unavailable,
            "Combat log is unavailable");
    }
    const auto observed_prefix = read_prefix(path_, size);
    if (!observed_prefix) {
        return failure(
            CombatLogError::read_failed,
            "Combat log prefix cannot be read");
    }
    const bool had_identity = identity_.has_value();
    bool stream_reset = false;
    if (!identity_) {
        identity_ = *current_identity;
        offset_ = start_at_end_ && !reopen_from_start_ ? size : 0U;
        reopen_from_start_ = false;
    } else if (*identity_ != *current_identity || size < offset_) {
        identity_ = *current_identity;
        offset_ = 0U;
        partial_line_.clear();
        dropping_line_ = false;
        oversized_line_seen_ = false;
        accumulator_.clear();
        prefix_boundary_.clear();
        boundary_.clear();
        stream_reset = true;
    }
    if (had_identity && !stream_reset &&
        !observed_prefix->starts_with(prefix_boundary_)) {
        offset_ = 0U;
        partial_line_.clear();
        dropping_line_ = false;
        oversized_line_seen_ = false;
        accumulator_.clear();
        boundary_.clear();
        stream_reset = true;
    }
    prefix_boundary_ = *observed_prefix;
    const auto observed_boundary = read_boundary(path_, offset_);
    if (!observed_boundary) {
        return failure(
            CombatLogError::read_failed,
            "Combat log boundary cannot be read");
    }
    if (had_identity && !stream_reset &&
        *observed_boundary != boundary_) {
        offset_ = 0U;
        partial_line_.clear();
        dropping_line_ = false;
        oversized_line_seen_ = false;
        accumulator_.clear();
        boundary_.clear();
    } else {
        boundary_ = *observed_boundary;
    }

    const std::uintmax_t available = size - offset_;
    const std::size_t requested = static_cast<std::size_t>(
        std::min<std::uintmax_t>(available, maximum_read_bytes));
    if (requested > 0U) {
        std::ifstream input(path_, std::ios::binary);
        if (!input) {
            return failure(
                CombatLogError::read_failed,
                "Combat log cannot be read");
        }
        input.seekg(static_cast<std::streamoff>(offset_));
        std::vector<char> bytes(requested);
        input.read(bytes.data(), static_cast<std::streamsize>(requested));
        const std::size_t received =
            static_cast<std::size_t>(input.gcount());
        if (received == 0U && requested != 0U) {
            return failure(
                CombatLogError::read_failed,
                "Combat log read made no progress");
        }
        lines_this_refresh_ = 0U;
        std::size_t consumed = 0U;
        for (; consumed < received; ++consumed) {
            if (!consume(bytes[consumed], active_character)) {
                break;
            }
        }
        offset_ += consumed;
    }

    const auto final_boundary = read_boundary(path_, offset_);
    if (!final_boundary) {
        return failure(
            CombatLogError::read_failed,
            "Combat log boundary cannot be updated");
    }
    boundary_ = *final_boundary;

    if (dropping_line_ || oversized_line_seen_) {
        oversized_line_seen_ = false;
        return failure(
            CombatLogError::unavailable,
            "Combat log contains an oversized line");
    }

    return {
        .snapshot = accumulator_.snapshot(now, active_character),
        .error = CombatLogError::none,
    };
}

void CombatLogTailer::clear() {
    character_.clear();
    path_.clear();
    identity_.reset();
    offset_ = 0U;
    partial_line_.clear();
    dropping_line_ = false;
    oversized_line_seen_ = false;
    lines_this_refresh_ = 0U;
    prefix_boundary_.clear();
    boundary_.clear();
    reopen_from_start_ = false;
    accumulator_.clear();
}

}  // namespace plazmic
