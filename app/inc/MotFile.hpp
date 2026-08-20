//
// MotFile.hpp: MOTChallenge-format reading and writing.
//
// One row per object per frame:
//
//     frame,id,bb_left,bb_top,bb_width,bb_height,conf,-1,-1,-1
//
// Frame numbers are 1-based, as MOTChallenge expects. `id` is -1 in a detection
// file, which is the `det.txt` convention and lets one format serve both the
// tracking output and the captured detections that are replayed through it.
//
// The trailing -1 columns are class and visibility. They stay -1 here:
// `TrackedObject` carries no class id and the trackers pool every tracked class
// into one association problem, so evaluation is class-agnostic.
//
#pragma once
#include "TrackedObject.hpp"

#include <iosfwd>
#include <string>
#include <vector>

// The value written in the id column of a detection file.
inline constexpr int kMotDetectionId = -1;

// A single row. Reading fills these from the first seven columns; anything
// beyond them is accepted and ignored, so a MOTChallenge `det.txt` (10 columns)
// and a `gt.txt` (9) both parse.
struct MotRecord {
    int frame{1}; // 1-based
    int id{kMotDetectionId};
    float x{};
    float y{};
    float width{};
    float height{};
    float confidence{};
};

// Appends rows to a stream. Holds a reference, so the stream must outlive it.
//
// Floats are written in the shortest form that reads back as the same value,
// so a round trip through a file changes nothing. In practice that means whole
// pixels print as "63" rather than "63.00".
class MotWriter {
  public:
    explicit MotWriter(std::ostream &out) : out_(out) {}

    void write(const MotRecord &record);

    // Writes one row per track. A frame with no tracks writes nothing, which is
    // what MOTChallenge expects — absence is how a frame says "nothing here".
    void writeTracks(int frame, const std::vector<TrackedObject> &tracks);

  private:
    std::ostream &out_;
};

// Parsed rows, or the first problem found. `error` names the line it was on so
// a bad file can be corrected without bisecting it.
struct MotReadResult {
    std::vector<MotRecord> records;
    std::string error;

    bool ok() const { return error.empty(); }
};

// Blank lines are skipped. A line with fewer than seven columns, a column that
// is not a number, or a frame number below 1 is an error, and no partial result
// is returned.
MotReadResult readMotStream(std::istream &in, const std::string &origin);
MotReadResult readMotFile(const std::string &path);
