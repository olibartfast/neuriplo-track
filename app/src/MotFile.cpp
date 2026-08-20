#include "MotFile.hpp"

#include <array>
#include <charconv>
#include <fstream>
#include <istream>
#include <ostream>
#include <string_view>
#include <system_error>

namespace {

// Number of leading columns this format defines. Extra columns are ignored, so
// a 9-column gt.txt and a 10-column det.txt both read.
constexpr std::size_t kRequiredColumns = 7;

// Shortest representation that reads back as the same float. `to_chars` without
// a precision argument is defined to produce exactly that, which is what keeps
// a write/read round trip lossless without printing 63.000000 everywhere.
std::string shortest(float value) {
    std::array<char, 32> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string(buffer.data(), result.ptr);
}

std::string_view trim(std::string_view text) {
    const auto is_space = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
    while (!text.empty() && is_space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_space(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

template <typename T> bool parseNumber(std::string_view text, T &out) {
    if (text.empty()) {
        return false;
    }
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), out);
    // The whole field must be consumed: "12abc" is a malformed column, not 12.
    return error == std::errc() && end == text.data() + text.size();
}

std::string location(const std::string &origin, int line_number) { return origin + ":" + std::to_string(line_number); }

} // namespace

void MotWriter::write(const MotRecord &record) {
    out_ << record.frame << ',' << record.id << ',' << shortest(record.x) << ',' << shortest(record.y) << ','
         << shortest(record.width) << ',' << shortest(record.height) << ',' << shortest(record.confidence)
         << ",-1,-1,-1\n";
}

void MotWriter::writeTracks(int frame, const std::vector<TrackedObject> &tracks) {
    for (const TrackedObject &track : tracks) {
        write(MotRecord{frame, track.track_id, track.x, track.y, track.width, track.height, track.confidence});
    }
}

MotReadResult readMotStream(std::istream &in, const std::string &origin) {
    MotReadResult result;
    std::string line;
    int line_number = 0;

    while (std::getline(in, line)) {
        ++line_number;
        const std::string_view content = trim(line);
        if (content.empty()) {
            continue;
        }

        std::array<std::string_view, kRequiredColumns> columns{};
        std::size_t found = 0;
        std::size_t start = 0;
        while (found < kRequiredColumns && start <= content.size()) {
            const std::size_t comma = content.find(',', start);
            const std::size_t end = (comma == std::string_view::npos) ? content.size() : comma;
            columns[found++] = trim(content.substr(start, end - start));
            if (comma == std::string_view::npos) {
                break;
            }
            start = comma + 1;
        }

        if (found < kRequiredColumns) {
            result.records.clear();
            result.error = location(origin, line_number) + ": expected at least " + std::to_string(kRequiredColumns) +
                           " comma-separated columns, found " + std::to_string(found);
            return result;
        }

        MotRecord record;
        const bool parsed = parseNumber(columns[0], record.frame) && parseNumber(columns[1], record.id) &&
                            parseNumber(columns[2], record.x) && parseNumber(columns[3], record.y) &&
                            parseNumber(columns[4], record.width) && parseNumber(columns[5], record.height) &&
                            parseNumber(columns[6], record.confidence);
        if (!parsed) {
            result.records.clear();
            result.error = location(origin, line_number) + ": column is not a number";
            return result;
        }

        if (record.frame < 1) {
            result.records.clear();
            result.error =
                location(origin, line_number) + ": frame numbers are 1-based, got " + std::to_string(record.frame);
            return result;
        }

        result.records.push_back(record);
    }

    return result;
}

MotReadResult readMotFile(const std::string &path) {
    std::ifstream in(path);
    if (!in) {
        MotReadResult result;
        result.error = "failed to open " + path;
        return result;
    }
    return readMotStream(in, path);
}
