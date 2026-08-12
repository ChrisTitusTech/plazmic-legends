#include "game/combat_log_parser.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <charconv>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace plazmic {
namespace {

constexpr std::uint64_t kMaximumDamage = 1'000'000'000'000ULL;
constexpr std::size_t kMaximumNameBytes = 128U;
constexpr std::uint32_t kMaximumTimelineSeconds = 600U;

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

bool same_encounter_content(const CombatEncounterSnapshot& left,
                            const CombatEncounterSnapshot& right) {
    return left.target == right.target &&
           left.participants == right.participants &&
           left.healers == right.healers && left.timeline == right.timeline &&
           left.zone == right.zone &&
           left.started_unix_seconds == right.started_unix_seconds &&
           left.total_damage == right.total_damage &&
           left.total_healing == right.total_healing &&
           left.duration_seconds == right.duration_seconds &&
           left.active_character_dps == right.active_character_dps;
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

std::string damage_kind_label(DamageKind kind) {
    switch (kind) {
        case DamageKind::melee:
            return "Melee";
        case DamageKind::spell:
            return "Spell";
        case DamageKind::damage_over_time:
            return "DoT";
        case DamageKind::pet:
            return "Pet";
    }
    return "Unknown";
}

std::string melee_ability(std::string_view verb) {
    verb = trim(verb);
    constexpr std::array<std::pair<std::string_view, std::string_view>, 36>
        kNames{{
            {"hit", "Hit"}, {"hits", "Hit"},
            {"slash", "Slash"}, {"slashes", "Slash"},
            {"pierce", "Pierce"}, {"pierces", "Pierce"},
            {"crush", "Crush"}, {"crushes", "Crush"},
            {"kick", "Kick"}, {"kicks", "Kick"},
            {"bash", "Bash"}, {"bashes", "Bash"},
            {"bite", "Bite"}, {"bites", "Bite"},
            {"claw", "Claw"}, {"claws", "Claw"},
            {"maul", "Maul"}, {"mauls", "Maul"},
            {"punch", "Punch"}, {"punches", "Punch"},
            {"backstab", "Backstab"}, {"backstabs", "Backstab"},
            {"strike", "Strike"}, {"strikes", "Strike"},
            {"cleave", "Cleave"}, {"cleaves", "Cleave"},
            {"smite", "Smite"}, {"smites", "Smite"},
            {"reave", "Reave"}, {"reaves", "Reave"},
            {"rend", "Rend"}, {"rends", "Rend"},
            {"frenzy on", "Frenzy"}, {"frenzies on", "Frenzy"},
            {"gore", "Gore"}, {"gores", "Gore"},
        }};
    const auto found = std::ranges::find_if(kNames, [verb](const auto& value) {
        return value.first == verb;
    });
    return found == kNames.end() ? "Melee" : std::string(found->second);
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

std::optional<std::uint64_t> parse_healing_amount(std::string_view value) {
    value = trim(value);
    std::uint64_t amount = 0U;
    const auto result = std::from_chars(
        value.data(), value.data() + value.size(), amount);
    if (result.ec != std::errc{} || amount == 0U ||
        amount > kMaximumDamage) {
        return std::nullopt;
    }
    std::string_view remainder(result.ptr,
                               static_cast<std::size_t>(
                                   value.data() + value.size() - result.ptr));
    remainder = trim(remainder);
    if (!remainder.empty()) {
        if (remainder.size() < 3U || remainder.front() != '(' ||
            remainder.back() != ')') {
            return std::nullopt;
        }
        const std::string_view inner =
            trim(remainder.substr(1U, remainder.size() - 2U));
        std::uint64_t secondary = 0U;
        const auto inner_result = std::from_chars(
            inner.data(), inner.data() + inner.size(), secondary);
        if (inner_result.ec != std::errc{} ||
            inner_result.ptr != inner.data() + inner.size() ||
            secondary > kMaximumDamage) {
            return std::nullopt;
        }
    }
    return amount;
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
    std::string ability;
    const std::size_t by = source.rfind(" by ");
    if (by != std::string_view::npos) {
        ability = clean_name(source.substr(0U, by), active_character);
        attacker = clean_name(source.substr(by + 4U), active_character);
    } else if (lowercase(source).starts_with("your ")) {
        ability = clean_name(source.substr(5U), active_character);
        attacker = std::string(active_character);
    } else {
        return std::nullopt;
    }
    std::string defender =
        clean_name(payload.substr(0U, taken), active_character);
    if (!valid_name(attacker) || !valid_name(defender) ||
        !valid_name(ability)) {
        return std::nullopt;
    }
    return DamageEvent{
        .timestamp = timestamp,
        .attacker = std::move(attacker),
        .defender = std::move(defender),
        .damage = *damage,
        .kind = DamageKind::damage_over_time,
        .ability = std::move(ability),
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
    std::string ability = melee_ability(verb);
    if (kind == DamageKind::spell) {
        const std::string lower_payload = lowercase(payload);
        constexpr std::string_view kDamageBy = " damage by ";
        const std::size_t by = lower_payload.rfind(kDamageBy);
        if (by == std::string::npos) {
            return std::nullopt;
        }
        std::string_view ability_text = payload.substr(by + kDamageBy.size());
        const std::size_t modifier = ability_text.find(". (");
        if (modifier != std::string_view::npos) {
            ability_text = ability_text.substr(0U, modifier);
        }
        ability = clean_name(ability_text, active_character);
        if (!valid_name(ability)) {
            return std::nullopt;
        }
    }
    return DamageEvent{
        .timestamp = timestamp,
        .attacker = std::move(attacker),
        .defender = std::move(defender),
        .damage = *damage,
        .kind = kind,
        .ability = std::move(ability),
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

bool valid_server_identity(std::string_view value) {
    if (value.empty() || value.size() > 64U ||
        std::isalnum(static_cast<unsigned char>(value.front())) == 0 ||
        std::isalnum(static_cast<unsigned char>(value.back())) == 0) {
        return false;
    }
    return std::ranges::all_of(value, [](unsigned char byte) {
        return std::isalnum(byte) != 0 || byte == '-' || byte == '_';
    });
}

struct OpenLogFile {
    CombatLogTailer::FileIdentity identity;
    std::shared_ptr<int> descriptor;
    std::uintmax_t size{};
};

std::optional<OpenLogFile> open_log_file(
    const std::filesystem::path& path) {
    const int descriptor = ::open(
        path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) {
        return std::nullopt;
    }
    struct stat status {};
    if (::fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_size < 0) {
        (void)::close(descriptor);
        return std::nullopt;
    }
    return OpenLogFile{
        .identity = {
            .device = static_cast<std::uint64_t>(status.st_dev),
            .inode = static_cast<std::uint64_t>(status.st_ino),
        },
        .descriptor = std::shared_ptr<int>(
            new int(descriptor), [](const int* value) {
                (void)::close(*value);
                delete value;
            }),
        .size = static_cast<std::uintmax_t>(status.st_size),
    };
}

std::optional<std::string> read_boundary(
    int descriptor,
    std::uintmax_t offset) {
    if (offset == 0U) {
        return std::string{};
    }
    const std::uintmax_t count = std::min<std::uintmax_t>(
        offset, CombatLogTailer::boundary_bytes);
    if (offset > static_cast<std::uintmax_t>(
                     std::numeric_limits<off_t>::max())) {
        return std::nullopt;
    }
    std::string result(static_cast<std::size_t>(count), '\0');
    std::size_t received = 0U;
    while (received < result.size()) {
        const ssize_t chunk = ::pread(
            descriptor, result.data() + received,
            result.size() - received,
            static_cast<off_t>(offset - count + received));
        if (chunk < 0 && errno == EINTR) {
            continue;
        }
        if (chunk <= 0) {
            return std::nullopt;
        }
        received += static_cast<std::size_t>(chunk);
    }
    return result;
}

std::optional<std::string> read_prefix(
    int descriptor,
    std::uintmax_t size) {
    const std::uintmax_t count = std::min<std::uintmax_t>(
        size, CombatLogTailer::replay_prefix_bytes);
    if (count == 0U) {
        return std::string{};
    }
    std::string result(static_cast<std::size_t>(count), '\0');
    std::size_t received = 0U;
    while (received < result.size()) {
        const ssize_t chunk = ::pread(
            descriptor, result.data() + received,
            result.size() - received, static_cast<off_t>(received));
        if (chunk < 0 && errno == EINTR) {
            continue;
        }
        if (chunk <= 0) {
            return std::nullopt;
        }
        received += static_cast<std::size_t>(chunk);
    }
    return result;
}

std::optional<std::string> read_available(
    int descriptor,
    std::uintmax_t offset,
    std::size_t requested) {
    if (offset > static_cast<std::uintmax_t>(
                     std::numeric_limits<off_t>::max())) {
        return std::nullopt;
    }
    std::string result(requested, '\0');
    std::size_t received = 0U;
    while (received < requested) {
        const ssize_t chunk = ::pread(
            descriptor, result.data() + received, requested - received,
            static_cast<off_t>(offset + received));
        if (chunk < 0 && errno == EINTR) {
            continue;
        }
        if (chunk < 0) {
            return std::nullopt;
        }
        if (chunk == 0) {
            break;
        }
        received += static_cast<std::size_t>(chunk);
    }
    result.resize(received);
    return result;
}

std::string_view complete_common_prefix(std::string_view previous,
                                        std::string_view current) {
    const auto mismatch = std::ranges::mismatch(previous, current);
    const std::size_t shared = static_cast<std::size_t>(
        std::distance(previous.begin(), mismatch.in1));
    if (shared == 0U) {
        return {};
    }
    const std::size_t newline = previous.rfind('\n', shared - 1U);
    if (newline == std::string_view::npos) {
        return {};
    }
    return previous.substr(0U, newline + 1U);
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

bool is_activity_ability(const DamageEvent& event) {
    if (event.ability.empty()) {
        return false;
    }
    if (event.kind != DamageKind::melee) {
        return true;
    }
    constexpr std::array<std::string_view, 5> kGenericAutoAttacks{
        "Hit", "Slash", "Pierce", "Crush", "Punch",
    };
    return std::ranges::find(kGenericAutoAttacks, event.ability) ==
           kGenericAutoAttacks.end();
}

std::optional<HealingEvent> parse_healing_line(
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
    constexpr std::string_view kFor = " for ";
    constexpr std::array<std::string_view, 4> kUnits{
        " hit points", " hit point", " points", " point",
    };
    std::size_t unit_position = std::string_view::npos;
    std::size_t unit_end = 0U;
    for (const auto unit : kUnits) {
        const std::size_t found = payload.rfind(unit);
        if (found != std::string_view::npos &&
            (found + unit.size() > unit_end ||
             (found + unit.size() == unit_end &&
              (unit_position == std::string_view::npos ||
               found < unit_position)))) {
            unit_position = found;
            unit_end = found + unit.size();
        }
    }
    if (unit_position == std::string_view::npos) {
        return std::nullopt;
    }
    const std::size_t for_position = payload.rfind(kFor, unit_position);
    if (for_position == std::string_view::npos) {
        return std::nullopt;
    }
    const auto amount = parse_healing_amount(payload.substr(
        for_position + kFor.size(),
        unit_position - for_position - kFor.size()));
    if (!amount) {
        return std::nullopt;
    }

    std::string healer;
    std::string target;
    constexpr std::string_view kHealedBy = " has been healed by ";
    const std::size_t healed_by = payload.find(kHealedBy);
    if (healed_by != std::string_view::npos && healed_by < for_position) {
        target = clean_name(payload.substr(0U, healed_by), active_character);
        healer = clean_name(payload.substr(
            healed_by + kHealedBy.size(),
            for_position - healed_by - kHealedBy.size()), active_character);
    } else {
        constexpr std::array<std::string_view, 2> kPassiveHealed{
            " has been healed ", " have been healed ",
        };
        if (std::ranges::any_of(kPassiveHealed, [&](const auto marker) {
                const std::size_t found = payload.find(marker);
                return found != std::string_view::npos &&
                       found < for_position;
            })) {
            return std::nullopt;
        }
        constexpr std::array<std::string_view, 4> kHealed{
            " have healed ", " has healed ", " healed ", " heals ",
        };
        std::size_t healed = std::string_view::npos;
        std::string_view verb;
        for (const auto candidate : kHealed) {
            healed = payload.rfind(candidate, for_position);
            if (healed != std::string_view::npos && healed < for_position) {
                verb = candidate;
                break;
            }
        }
        if (healed == std::string_view::npos) {
            return std::nullopt;
        }
        std::string_view healer_text = payload.substr(0U, healed);
        const std::size_t sentence = healer_text.rfind(". ");
        if (sentence != std::string_view::npos) {
            healer_text.remove_prefix(sentence + 2U);
        }
        healer = clean_name(healer_text, active_character);
        std::string_view target_text = trim(payload.substr(
            healed + verb.size(),
            for_position - healed - verb.size()));
        constexpr std::string_view kOverTime = " over time";
        if (target_text.ends_with(kOverTime)) {
            target_text.remove_suffix(kOverTime.size());
        }
        target = clean_name(target_text, active_character);
        if (ci_equal(target, "himself") || ci_equal(target, "herself") ||
            ci_equal(target, "itself") || ci_equal(target, "yourself")) {
            target = healer;
        }
    }
    if (!valid_name(healer) || !valid_name(target)) {
        return std::nullopt;
    }
    return HealingEvent{
        .timestamp = *timestamp,
        .healer = std::move(healer),
        .target = std::move(target),
        .healing = *amount,
    };
}

void CombatAccumulator::clear() {
    participants_.clear();
    healers_.clear();
    timeline_.clear();
    target_.clear();
    zone_.clear();
    total_damage_ = 0U;
    total_healing_ = 0U;
    total_abilities_ = 0U;
    first_ = {};
    last_ = {};
    completed_.reset();
    active_ = false;
}

bool CombatAccumulator::add(const DamageEvent& event,
                            std::string_view active_character,
                            std::string_view zone) {
    if (!valid_name(event.attacker) || !valid_name(event.defender) ||
        event.damage == 0U || event.damage > kMaximumDamage) {
        return false;
    }
    const bool outside_encounter =
        active_ &&
        (event.timestamp < first_ || event.timestamp - last_ > inactivity);
    if (outside_encounter) {
        if (!active_character.empty() &&
            !ci_equal(event.attacker, active_character)) {
            return false;
        }
        CombatEncounterSnapshot completed = snapshot(
            last_ + inactivity + std::chrono::seconds(1),
            active_character);
        clear();
        completed_ = std::move(completed);
    }
    if ((!active_ || total_damage_ == 0U) && !active_character.empty() &&
        !ci_equal(event.attacker, active_character)) {
        return false;
    }
    if (active_ && total_damage_ != 0U &&
        !ci_equal(event.defender, target_)) {
        return false;
    }
    auto found = participants_.find(event.attacker);
    if (found == participants_.end() &&
        participants_.size() >= maximum_participants) {
        return false;
    }
    if (total_damage_ >
        CombatHistoryStore::maximum_aggregate - event.damage) {
        return false;
    }
    const std::string ability_name = event.ability.empty()
                                         ? damage_kind_label(event.kind)
                                         : event.ability;
    if (!valid_name(ability_name)) {
        return false;
    }
    const std::string ability_key = damage_kind_label(event.kind) + "\n" +
                                    lowercase(ability_name);
    const bool existing_ability =
        found != participants_.end() &&
        found->second.abilities.contains(ability_key);
    const bool record_new_ability =
        !existing_ability &&
        (found == participants_.end() ||
         found->second.abilities.size() < maximum_abilities_per_participant) &&
        total_abilities_ < maximum_total_abilities;
    if (!active_) {
        first_ = event.timestamp;
        last_ = event.timestamp;
        target_ = event.defender;
        zone_ = zone.empty() ? "Unknown" : std::string(zone);
        active_ = true;
    } else if (total_damage_ == 0U) {
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
    if (existing_ability || record_new_ability) {
        auto [ability_iterator, ability_inserted] =
            participant.abilities.try_emplace(ability_key);
        auto& ability = ability_iterator->second;
        if (ability_inserted) {
            ability.name = ability_name;
            ability.kind = event.kind;
            ++total_abilities_;
        }
        ability.damage += event.damage;
        ++ability.hits;
    }
    total_damage_ += event.damage;
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        event.timestamp - first_).count();
    if (elapsed >= 0 &&
        elapsed < static_cast<std::int64_t>(kMaximumTimelineSeconds)) {
        timeline_[static_cast<std::uint32_t>(elapsed)].damage += event.damage;
    }
    last_ = std::max(last_, event.timestamp);
    return true;
}

bool CombatAccumulator::add(const HealingEvent& event,
                            std::string_view active_character,
                            std::string_view zone) {
    if (!valid_name(event.healer) ||
        !valid_name(event.target) || event.healing == 0U ||
        event.healing > kMaximumDamage ||
        total_healing_ >
            CombatHistoryStore::maximum_aggregate - event.healing) {
        return false;
    }
    const bool outside_encounter = active_ &&
        (event.timestamp < first_ || event.timestamp - last_ > inactivity);
    if (outside_encounter) {
        CombatEncounterSnapshot completed = snapshot(
            last_ + inactivity + std::chrono::seconds(1), active_character);
        clear();
        completed_ = std::move(completed);
    }
    if (!active_character.empty() &&
        !ci_equal(event.healer, active_character) &&
        !ci_equal(event.target, active_character)) {
        return false;
    }
    auto found = healers_.find(event.healer);
    if (found == healers_.end() &&
        healers_.size() >= maximum_participants) {
        return false;
    }
    Healer& healer = healers_[event.healer];
    if (!active_) {
        active_ = true;
        first_ = event.timestamp;
        last_ = event.timestamp;
        target_ = "Healing activity";
        zone_ = zone.empty() ? "Unknown" : std::string(zone);
    }
    healer.healing += event.healing;
    ++healer.casts;
    total_healing_ += event.healing;
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        event.timestamp - first_).count();
    if (elapsed >= 0 &&
        elapsed < static_cast<std::int64_t>(kMaximumTimelineSeconds)) {
        timeline_[static_cast<std::uint32_t>(elapsed)].healing += event.healing;
    }
    last_ = std::max(last_, event.timestamp);
    return true;
}

std::optional<CombatEncounterSnapshot> CombatAccumulator::take_completed() {
    return std::exchange(completed_, std::nullopt);
}

std::optional<CombatEncounterSnapshot> CombatAccumulator::finalize(
    std::string_view active_character) {
    if (!active_) {
        clear();
        return std::nullopt;
    }
    CombatEncounterSnapshot result = snapshot(
        last_ + inactivity + std::chrono::seconds(1), active_character);
    clear();
    return result;
}

CombatEncounterSnapshot CombatAccumulator::snapshot(
    std::chrono::system_clock::time_point now,
    std::string_view active_character) const {
    if (!active_) {
        return {
            .state = CombatEncounterState::idle,
            .target = {},
            .participants = {},
            .healers = {},
            .timeline = {},
            .zone = {},
            .started_unix_seconds = 0,
            .total_damage = 0U,
            .total_healing = 0U,
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
        .healers = {},
        .timeline = {},
        .zone = zone_,
        .started_unix_seconds =
            std::chrono::duration_cast<std::chrono::seconds>(
                first_.time_since_epoch()).count(),
        .total_damage = total_damage_,
        .total_healing = total_healing_,
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
            .melee_damage = participant.kind_damage[0],
            .spell_damage = participant.kind_damage[1],
            .damage_over_time = participant.kind_damage[2],
            .pet_damage = participant.kind_damage[3],
            .abilities = {},
        };
        if (ci_equal(name, active_character)) {
            result.active_character_dps = value.dps;
        }
        result.participants.push_back(std::move(value));
    }
    for (std::size_t index = 0U; index < result.participants.size(); ++index) {
        const auto& participant = participants_.at(
            result.participants[index].name);
        auto& abilities = result.participants[index].abilities;
        abilities.reserve(participant.abilities.size());
        for (const auto& [key, ability] : participant.abilities) {
            (void)key;
            abilities.push_back({
                .name = ability.name,
                .category = damage_kind_label(ability.kind),
                .damage = ability.damage,
                .hits = ability.hits,
            });
        }
        std::ranges::sort(abilities, [](const auto& left, const auto& right) {
            return left.damage != right.damage
                       ? left.damage > right.damage
                       : lowercase(left.name) < lowercase(right.name);
        });
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
    result.healers.reserve(healers_.size());
    for (const auto& [name, healer] : healers_) {
        result.healers.push_back({
            .name = name,
            .healing = healer.healing,
            .casts = healer.casts,
            .hps = static_cast<double>(healer.healing) /
                   result.duration_seconds,
            .percentage = total_healing_ == 0U
                              ? 0.0
                              : static_cast<double>(healer.healing) * 100.0 /
                                    static_cast<double>(total_healing_),
        });
    }
    std::ranges::sort(result.healers, [](const auto& left, const auto& right) {
        return left.healing != right.healing
                   ? left.healing > right.healing
                   : lowercase(left.name) < lowercase(right.name);
    });
    std::vector<std::uint32_t> seconds;
    seconds.reserve(timeline_.size());
    for (const auto& [second, bucket] : timeline_) {
        (void)bucket;
        seconds.push_back(second);
    }
    std::ranges::sort(seconds);
    result.timeline.reserve(seconds.size());
    for (const std::uint32_t second : seconds) {
        const TimelineBucket& bucket = timeline_.at(second);
        result.timeline.push_back({
            .elapsed_seconds = second,
            .damage = bucket.damage,
            .healing = bucket.healing,
        });
    }
    return result;
}

CombatLogRefresh CombatLogTailer::failure(
    CombatLogError error,
    std::string detail,
    std::chrono::system_clock::time_point now) {
    activity_tracker_.break_equipment_baseline();
    ActivityAnalyticsSnapshot activity =
        activity_tracker_.unavailable_snapshot(detail);
    if (!activity_partition_confirmed_) {
        activity.storage_key.clear();
    }
    CombatAnalyticsSnapshot analytics{
            .encounter = {
                .state = CombatEncounterState::unavailable,
                .target = {},
                .participants = {},
                .healers = {},
                .timeline = {},
                .zone = {},
                .started_unix_seconds = 0,
                .total_damage = 0U,
                .total_healing = 0U,
                .duration_seconds = 0.0,
                .active_character_dps = 0.0,
                .detail = std::move(detail),
            },
            .history = history_visible_
                           ? history_
                           : std::vector<CombatEncounterSnapshot>{},
            .zone_damage = 0U,
            .zone_healing = 0U,
            .zone_encounters = 0U,
            .history_retention_enabled = history_enabled_,
            .history_persisted = history_persisted_,
            .history_detail = history_persisted_
                                  ? std::string{}
                                  : "Encounter history could not be saved",
        };
    if (history_visible_) {
        for (const auto& encounter : history_) {
            if (ci_equal(encounter.zone, history_zone_)) {
                analytics.zone_damage += encounter.total_damage;
                analytics.zone_healing += encounter.total_healing;
                ++analytics.zone_encounters;
            }
        }
    }
    return {
        .snapshot = std::move(analytics),
        .activity = std::move(activity),
        .alerts = alert_engine_.snapshot(now),
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
        const bool candidate =
            filename.starts_with(prefix) && filename.ends_with(".txt") &&
            filename.size() > prefix.size() + 4U;
        const std::string_view server = candidate
                                            ? std::string_view(filename).substr(
                                                  prefix.size(),
                                                  filename.size() -
                                                      prefix.size() - 4U)
                                            : std::string_view{};
        if (candidate && valid_server_identity(server)) {
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
    std::string_view active_character,
    std::string_view zone,
    std::chrono::system_clock::time_point now) {
    if (byte == '\r') {
        return true;
    }
    if (byte == '\n') {
        if (dropping_line_) {
            oversized_line_seen_ = true;
        }
        if (!dropping_line_ && !partial_line_.empty() &&
            lines_this_refresh_ < maximum_lines_per_refresh) {
            const auto damage =
                parse_damage_line(partial_line_, active_character);
            const bool outgoing_ability =
                damage && ci_equal(damage->attacker, active_character) &&
                !ci_equal(damage->defender, active_character) &&
                is_activity_ability(*damage);
            const std::string activity_source = activity_tracker_.consume(
                partial_line_, active_character, zone,
                outgoing_ability || alert_engine_.has_rules(),
                outgoing_ability);
            if (!activity_source.empty()) {
                if (alert_engine_.consume(
                        partial_line_, zone, activity_source, now)) {
                    activity_tracker_.retain_transient_source(
                        activity_source, partial_line_);
                }
            }
            if (damage && !ci_equal(damage->defender, active_character)) {
                activity_tracker_.observe_damage(
                    *damage, active_character, activity_source);
                (void)accumulator_.add(*damage, active_character, zone);
            } else if (const auto healing =
                           parse_healing_line(partial_line_, active_character)) {
                (void)accumulator_.add(*healing, active_character, zone);
            }
            retain_completed();
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
        return true;
    }
    partial_line_.push_back(byte);
    return true;
}

CombatLogRefresh CombatLogTailer::refresh(
    const std::filesystem::path& game_directory,
    std::string_view active_character,
    std::string_view zone,
    std::chrono::system_clock::time_point now) {
    maintain_history_store();
    activity_tracker_.maintain(now);
    if (!valid_character(active_character)) {
        clear();
        return failure(
            CombatLogError::invalid_character,
            "Combat log unavailable for the active character", now);
    }
    if (!ci_equal(character_, active_character)) {
        clear();
        character_ = std::string(active_character);
    }
    CombatLogError selection_error = CombatLogError::none;
    const auto selected = select_log(
        game_directory, active_character, selection_error);
    if (!selected) {
        activity_partition_confirmed_ = false;
        return failure(
            selection_error,
            selection_error == CombatLogError::ambiguous
                ? "Multiple combat logs match the active character"
                : "Combat logging is unavailable",
            now);
    }
    std::optional<ReplayContinuity> retained_stream;
    if (path_ != *selected) {
        finalize_current();
        std::string history_selection_failure;
        if (!select_history(
                active_character, *selected, history_selection_failure)) {
            return failure(
                CombatLogError::unavailable,
                std::move(history_selection_failure), now);
        }
        const std::string selected_path = selected->lexically_normal().string();
        const auto retained = replay_continuity_.find(selected_path);
        const bool replacing_stream = retained != replay_continuity_.end();
        const bool returning_stream = seen_log_paths_.contains(selected_path);
        if (!returning_stream &&
            seen_log_paths_.size() < maximum_log_entries) {
            seen_log_paths_.insert(selected_path);
        }
        if (replacing_stream) {
            retained_stream = retained->second;
        }
        path_ = *selected;
        identity_.reset();
        descriptor_.reset();
        offset_ = 0U;
        partial_line_.clear();
        dropping_line_ = false;
        oversized_line_seen_ = false;
        prefix_boundary_ = replacing_stream ? retained->second.prefix
                                             : std::string{};
        boundary_.clear();
        reopen_from_start_ = replacing_stream || returning_stream;
        accumulator_.clear();
    } else {
        std::string history_selection_failure;
        if (!select_history(
                active_character, *selected, history_selection_failure)) {
            return failure(
                CombatLogError::unavailable,
                std::move(history_selection_failure), now);
        }
    }
    history_zone_ = std::string(zone);
    history_visible_ = true;
    maintain_history();

    const auto current_file = open_log_file(path_);
    if (!current_file) {
        return failure(
            CombatLogError::unavailable,
            "Combat log is unavailable", now);
    }
    descriptor_ = current_file->descriptor;
    const std::uintmax_t size = current_file->size;
    const FileIdentity current_identity = current_file->identity;
    const auto observed_prefix = read_prefix(*descriptor_, size);
    if (!observed_prefix) {
        return failure(
            CombatLogError::read_failed,
            "Combat log prefix cannot be read", now);
    }
    if (!identity_ && retained_stream &&
        retained_stream->descriptor &&
        retained_stream->identity == current_identity &&
        size >= retained_stream->offset &&
        observed_prefix->starts_with(retained_stream->prefix)) {
        const auto retained_boundary =
            read_boundary(*descriptor_, retained_stream->offset);
        if (retained_boundary &&
            *retained_boundary == retained_stream->boundary) {
            identity_ = retained_stream->identity;
            offset_ = retained_stream->offset;
            partial_line_ = retained_stream->partial_line;
            dropping_line_ = retained_stream->dropping_line;
            oversized_line_seen_ = retained_stream->oversized_line_seen;
            boundary_ = retained_stream->boundary;
            reopen_from_start_ = false;
        }
    }
    const std::string prior_prefix = prefix_boundary_;
    const std::optional<FileIdentity> prior_identity = identity_;
    const bool continuous_identity =
        (prior_identity && *prior_identity == current_identity) ||
        (retained_stream &&
         retained_stream->descriptor &&
         retained_stream->identity == current_identity);
    const std::string_view replay_overlap =
        continuous_identity
            ? complete_common_prefix(prior_prefix, *observed_prefix)
            : std::string_view{};
    const bool had_identity = identity_.has_value();
    const bool reopening_from_start = reopen_from_start_;
    bool stream_reset = false;
    if (!identity_) {
        identity_ = current_identity;
        offset_ = start_at_end_ && !reopen_from_start_ ? size : 0U;
        reopen_from_start_ = false;
        if (reopening_from_start) {
            activity_tracker_.begin_log_stream(replay_overlap);
            stream_reset = true;
        }
    } else if (*identity_ != current_identity || size < offset_) {
        identity_ = current_identity;
        offset_ = 0U;
        partial_line_.clear();
        dropping_line_ = false;
        oversized_line_seen_ = false;
        finalize_current();
        prefix_boundary_.clear();
        boundary_.clear();
        stream_reset = true;
        activity_tracker_.begin_log_stream(replay_overlap);
    }
    if (had_identity && !stream_reset &&
        !observed_prefix->starts_with(prefix_boundary_)) {
        offset_ = 0U;
        partial_line_.clear();
        dropping_line_ = false;
        oversized_line_seen_ = false;
        finalize_current();
        boundary_.clear();
        stream_reset = true;
        activity_tracker_.begin_log_stream(replay_overlap);
    }
    prefix_boundary_ = *observed_prefix;
    const auto observed_boundary = read_boundary(*descriptor_, offset_);
    if (!observed_boundary) {
        return failure(
            CombatLogError::read_failed,
            "Combat log boundary cannot be read", now);
    }
    if (had_identity && !stream_reset &&
        *observed_boundary != boundary_) {
        offset_ = 0U;
        partial_line_.clear();
        dropping_line_ = false;
        oversized_line_seen_ = false;
        finalize_current();
        boundary_.clear();
        activity_tracker_.begin_log_stream(replay_overlap);
    } else {
        boundary_ = *observed_boundary;
    }

    const std::uintmax_t available = size - offset_;
    const std::size_t requested = static_cast<std::size_t>(
        std::min<std::uintmax_t>(available, maximum_read_bytes));
    if (requested > 0U) {
        const auto bytes = read_available(*descriptor_, offset_, requested);
        if (!bytes) {
            return failure(
                CombatLogError::read_failed,
                "Combat log cannot be read", now);
        }
        const std::size_t received = bytes->size();
        if (received == 0U && requested != 0U) {
            return failure(
                CombatLogError::read_failed,
                "Combat log read made no progress", now);
        }
        lines_this_refresh_ = 0U;
        std::size_t consumed = 0U;
        for (; consumed < received; ++consumed) {
            if (!consume((*bytes)[consumed], active_character, zone, now)) {
                break;
            }
        }
        offset_ += consumed;
    }

    const auto final_boundary = read_boundary(*descriptor_, offset_);
    if (!final_boundary) {
        return failure(
            CombatLogError::read_failed,
            "Combat log boundary cannot be updated", now);
    }
    boundary_ = *final_boundary;
    const std::string active_path = path_.lexically_normal().string();
    if (!replay_continuity_.contains(active_path) &&
        replay_continuity_.size() >= maximum_replay_paths) {
        replay_continuity_.erase(replay_continuity_.begin());
    }
    replay_continuity_[active_path] = {
        .identity = *identity_,
        .descriptor = descriptor_,
        .offset = offset_,
        .partial_line = partial_line_,
        .dropping_line = dropping_line_,
        .oversized_line_seen = oversized_line_seen_,
        .prefix = prefix_boundary_,
        .boundary = boundary_,
    };

    if (dropping_line_ || oversized_line_seen_) {
        oversized_line_seen_ = false;
        return failure(
            CombatLogError::unavailable,
            "Combat log contains an oversized line", now);
    }

    return current_snapshot(active_character, zone, now);
}

CombatLogRefresh CombatLogTailer::current_snapshot(
    std::string_view active_character,
    std::string_view zone,
    std::chrono::system_clock::time_point now) {
    CombatEncounterSnapshot current =
        accumulator_.snapshot(now, active_character);
    if (current.state == CombatEncounterState::complete) {
        retain_encounter(current);
    }
    CombatAnalyticsSnapshot analytics{
        .encounter = std::move(current),
        .history = history_,
        .history_retention_enabled = history_enabled_,
        .history_persisted = history_persisted_,
        .history_detail = history_persisted_
                              ? std::string{}
                              : "Encounter history could not be saved",
    };
    for (const auto& encounter : history_) {
        if (ci_equal(encounter.zone, zone)) {
            analytics.zone_damage += encounter.total_damage;
            analytics.zone_healing += encounter.total_healing;
            ++analytics.zone_encounters;
        }
    }
    if (analytics.encounter.state == CombatEncounterState::active &&
        ci_equal(analytics.encounter.zone, zone)) {
        analytics.zone_damage += analytics.encounter.total_damage;
        analytics.zone_healing += analytics.encounter.total_healing;
    }
    return {
        .snapshot = std::move(analytics),
        .activity = activity_tracker_.snapshot(now),
        .alerts = alert_engine_.snapshot(now),
        .error = CombatLogError::none,
    };
}

void CombatLogTailer::observe_character(
    const CharacterSnapshot& character,
    std::string_view zone,
    std::chrono::system_clock::time_point now) {
    activity_tracker_.observe_character(character, zone, now);
}

void CombatLogTailer::begin_deferred_persistence() {
    persistence_deferred_ = true;
    activity_tracker_.begin_deferred_persistence();
}

void CombatLogTailer::commit_deferred_persistence(bool retain_history,
                                                  bool retain_activity) {
    persistence_deferred_ = false;
    if (retain_history != history_enabled_) {
        set_history_enabled(retain_history);
    } else if (retain_history && !history_persisted_ &&
               !history_key_.empty()) {
        history_persisted_ = history_store_.save(history_key_, history_);
        history_retry_after_ =
            history_persisted_
                ? std::chrono::steady_clock::time_point{}
                : std::chrono::steady_clock::now() + history_retry_delay;
    }
    activity_tracker_.commit_deferred_persistence(retain_activity);
}

bool CombatLogTailer::select_history(
    std::string_view active_character,
    const std::filesystem::path& log_path,
    std::string& failure_detail) {
    failure_detail.clear();
    const std::string identity = lowercase(active_character) + "\n" +
                                 lowercase(log_path.filename().string());
    const std::string key = CombatHistoryStore::privacy_key(identity);
    if (key == history_key_) {
        const bool activity_selected = activity_tracker_.select(key);
        if (!activity_selected && activity_tracker_.selected_key() != key) {
            failure_detail =
                "Retained activity history could not be loaded safely";
            return false;
        }
        activity_partition_confirmed_ = true;
        if (history_enabled_ && !history_load_compatible_) {
            failure_detail =
                "Retained combat history could not be loaded safely";
            return false;
        }
        return true;
    }
    activity_partition_confirmed_ = false;
    if (!history_enabled_) {
        const bool activity_selected = activity_tracker_.select(key);
        if (!activity_selected && activity_tracker_.selected_key() != key) {
            failure_detail =
                "Retained activity history could not be loaded safely";
            return false;
        }
        history_key_ = key;
        history_.clear();
        history_persisted_ = true;
        history_load_compatible_ = true;
        history_retry_after_ = {};
        history_visible_ = true;
        activity_partition_confirmed_ = true;
        return true;
    }
    if (!history_persisted_ && !history_key_.empty() &&
        (persistence_deferred_ ||
         !history_store_.save(history_key_, history_))) {
        failure_detail = "Unsaved local history is awaiting persistence";
        return false;
    }
    const auto loaded = history_store_.load_checked(key);
    if (!loaded) {
        failure_detail =
            "Retained combat history could not be loaded safely";
        return false;
    }
    const bool activity_selected = activity_tracker_.select(key);
    if (!activity_selected && activity_tracker_.selected_key() != key) {
        failure_detail =
            "Retained activity history could not be loaded safely";
        return false;
    }
    history_key_ = key;
    history_ = loaded->history;
    history_persisted_ = loaded->persisted;
    history_load_compatible_ = true;
    history_retry_after_ = {};
    history_visible_ = true;
    activity_partition_confirmed_ = true;
    return true;
}

void CombatLogTailer::retain_encounter(
    const CombatEncounterSnapshot& encounter) {
    if (!encounter.available()) {
        return;
    }
    const auto existing = std::ranges::find_if(
        history_, [&encounter](const auto& value) {
            return value.started_unix_seconds ==
                   encounter.started_unix_seconds;
        });
    bool unchanged = false;
    if (existing == history_.end()) {
        history_.push_back(encounter);
    } else {
        if (same_encounter_content(*existing, encounter)) {
            unchanged = true;
        } else {
            *existing = encounter;
        }
    }
    const auto persistence_now = std::chrono::steady_clock::now();
    if (unchanged &&
        (history_persisted_ || persistence_now < history_retry_after_)) {
        return;
    }
    if (history_.size() > CombatHistoryStore::maximum_encounters) {
        history_.erase(history_.begin(),
                       history_.begin() +
                           static_cast<std::ptrdiff_t>(
                               history_.size() -
                               CombatHistoryStore::maximum_encounters));
    }
    if (!history_enabled_ || persistence_deferred_) {
        history_persisted_ = history_store_.bound(history_);
        if (history_enabled_) {
            history_persisted_ = false;
        }
        return;
    }
    if (!history_key_.empty()) {
        history_persisted_ = history_store_.save(history_key_, history_);
        history_retry_after_ = history_persisted_
                                   ? std::chrono::steady_clock::time_point{}
                                   : persistence_now + history_retry_delay;
    } else {
        history_persisted_ = false;
        history_retry_after_ = persistence_now + history_retry_delay;
    }
}

void CombatLogTailer::set_history_enabled(bool enabled) {
    if (enabled == history_enabled_) {
        return;
    }
    history_enabled_ = enabled;
    history_persisted_ = true;
    history_load_compatible_ = true;
    history_retry_after_ = {};
    if (!enabled || history_key_.empty()) {
        return;
    }
    const auto loaded = history_store_.load_checked(history_key_);
    if (!loaded) {
        history_persisted_ = false;
        history_load_compatible_ = false;
        return;
    }
    for (const auto& encounter : loaded->history) {
        const auto existing = std::ranges::find_if(
            history_, [&encounter](const auto& value) {
                return value.started_unix_seconds ==
                       encounter.started_unix_seconds;
            });
        if (existing == history_.end()) {
            history_.push_back(encounter);
        }
    }
    std::ranges::sort(history_, {},
                      &CombatEncounterSnapshot::started_unix_seconds);
    history_persisted_ = history_store_.save(history_key_, history_);
    history_retry_after_ = history_persisted_
                               ? std::chrono::steady_clock::time_point{}
                               : std::chrono::steady_clock::now() +
                                     history_retry_delay;
}

void CombatLogTailer::maintain_history() {
    if (!history_load_compatible_) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto oldest =
        std::chrono::duration_cast<std::chrono::seconds>(
            (std::chrono::system_clock::now() -
             CombatHistoryStore::maximum_age)
                .time_since_epoch())
            .count();
    const bool age_prune = std::ranges::any_of(
        history_, [oldest](const auto& encounter) {
            return encounter.started_unix_seconds < oldest;
        });
    if (age_prune) {
        if (!history_enabled_ || persistence_deferred_) {
            history_persisted_ = history_store_.bound(history_);
            if (history_enabled_) {
                history_persisted_ = false;
            }
            return;
        }
        history_persisted_ = !history_key_.empty() &&
                             history_store_.save(history_key_, history_);
        history_retry_after_ = history_persisted_
                                   ? std::chrono::steady_clock::time_point{}
                                   : now + history_retry_delay;
    }
    if (persistence_deferred_ || !history_enabled_ || history_persisted_ ||
        history_key_.empty()) {
        return;
    }
    if (now < history_retry_after_) {
        return;
    }
    history_persisted_ = history_store_.save(history_key_, history_);
    history_retry_after_ = history_persisted_
                               ? std::chrono::steady_clock::time_point{}
                               : now + history_retry_delay;
}

void CombatLogTailer::maintain_history_store() {
    if (persistence_deferred_) {
        return;
    }
    const auto now = std::chrono::system_clock::now();
    if (now < history_sweep_after_) {
        return;
    }
    const CombatHistoryPrune result = history_store_.prune_expired();
    history_sweep_after_ = result.next_expiration.value_or(
        now + (result.healthy ? std::chrono::hours(24)
                              : std::chrono::hours(1)));
}

void CombatLogTailer::retain_completed() {
    auto completed = accumulator_.take_completed();
    if (completed) {
        retain_encounter(*completed);
    }
}

void CombatLogTailer::finalize_current() {
    auto completed = accumulator_.finalize(character_);
    if (completed) {
        retain_encounter(*completed);
    }
}

void CombatLogTailer::clear() {
    clear_context(false);
}

void CombatLogTailer::reset_context(std::string_view active_character,
                                    bool preserve_respawn_alerts) {
    clear_context(preserve_respawn_alerts);
    character_ = std::string(active_character);
}

void CombatLogTailer::clear_context(bool preserve_respawn_alerts) {
    if (!path_.empty() && identity_) {
        const std::string active_path = path_.lexically_normal().string();
        if (!replay_continuity_.contains(active_path) &&
            replay_continuity_.size() >= maximum_replay_paths) {
            replay_continuity_.erase(replay_continuity_.begin());
        }
        replay_continuity_[active_path] = {
            .identity = *identity_,
            .descriptor = descriptor_,
            .offset = offset_,
            .partial_line = partial_line_,
            .dropping_line = dropping_line_,
            .oversized_line_seen = oversized_line_seen_,
            .prefix = prefix_boundary_,
            .boundary = boundary_,
        };
    }
    finalize_current();
    (void)activity_tracker_.flush();
    if (!persistence_deferred_ && history_enabled_ && !history_persisted_ &&
        !history_key_.empty()) {
        history_persisted_ = history_store_.save(history_key_, history_);
    }
    character_.clear();
    path_.clear();
    identity_.reset();
    descriptor_.reset();
    offset_ = 0U;
    partial_line_.clear();
    dropping_line_ = false;
    oversized_line_seen_ = false;
    lines_this_refresh_ = 0U;
    boundary_.clear();
    reopen_from_start_ = false;
    accumulator_.clear();
    if (preserve_respawn_alerts) {
        activity_tracker_.reset_transient_observations();
        alert_engine_.reset_transient();
    } else {
        activity_tracker_.clear_transient_replay_sources();
        alert_engine_.clear_observations();
    }
    activity_partition_confirmed_ = false;
    history_visible_ = false;
    history_zone_.clear();
    if (history_enabled_ && history_persisted_) {
        history_key_.clear();
        history_.clear();
        history_retry_after_ = {};
        history_load_compatible_ = true;
    }
}

}  // namespace plazmic
