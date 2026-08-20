//
// MOTChallenge format: writing, parsing, and the ways a file can be wrong.
//
#include "MotFile.hpp"
#include "test_util.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace {

MotReadResult readString(const std::string &text) {
    std::istringstream in(text);
    return readMotStream(in, "test");
}

std::string writeAll(const std::vector<MotRecord> &records) {
    std::ostringstream out;
    MotWriter writer(out);
    for (const MotRecord &record : records) {
        writer.write(record);
    }
    return out.str();
}

// Values chosen to include ones a fixed-precision writer would round away:
// 0.3f is not representable exactly, and 1e-05f would vanish at two decimals.
void testRoundTripIsLossless() {
    const std::vector<MotRecord> written{
        {1, 7, 100.5f, 50.25f, 30.0f, 60.0f, 0.9f},
        {2, 7, 100.75f, 50.3f, 30.0f, 60.0f, 0.3f},
        {3, 12, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f}, // zero confidence: what SORT and ByteTrack produce
        {4, 12, 1919.0f, 1079.0f, 0.5f, 0.5f, 1e-05f},
    };

    const MotReadResult result = readString(writeAll(written));
    CHECK(result.ok());
    CHECK_EQ(result.records.size(), written.size());
    if (result.records.size() != written.size()) {
        return;
    }

    for (std::size_t i = 0; i < written.size(); ++i) {
        const std::string label = "record " + std::to_string(i);
        const MotRecord &a = written[i];
        const MotRecord &b = result.records[i];
        CHECK_LABELED(a.frame == b.frame, label);
        CHECK_LABELED(a.id == b.id, label);
        CHECK_LABELED(a.x == b.x, label);
        CHECK_LABELED(a.y == b.y, label);
        CHECK_LABELED(a.width == b.width, label);
        CHECK_LABELED(a.height == b.height, label);
        CHECK_LABELED(a.confidence == b.confidence, label);
    }
}

// Whole pixels must not print as 63.00: the file is read by people too.
void testWholeNumbersStayShort() {
    const std::string line = writeAll({{1, 3, 63.0f, 20.0f, 10.0f, 40.0f, 1.0f}});
    CHECK(line == "1,3,63,20,10,40,1,-1,-1,-1\n");
}

void testFrameNumbersAreOneBased() {
    std::ostringstream out;
    MotWriter writer(out);
    writer.writeTracks(1, {TrackedObject{5, 10.0f, 20.0f, 30.0f, 40.0f, 0.8f}});

    const MotReadResult result = readString(out.str());
    CHECK(result.ok());
    CHECK_EQ(result.records.size(), std::size_t{1});
    if (!result.records.empty()) {
        CHECK_EQ(result.records[0].frame, 1);
        CHECK_EQ(result.records[0].id, 5);
    }
}

// A frame the tracker had nothing to say about contributes no line, and must
// not shift the frames after it.
void testEmptyFrameWritesNothing() {
    std::ostringstream out;
    MotWriter writer(out);
    writer.writeTracks(1, {TrackedObject{5, 10.0f, 20.0f, 30.0f, 40.0f, 0.8f}});
    writer.writeTracks(2, {});
    writer.writeTracks(3, {TrackedObject{5, 11.0f, 21.0f, 30.0f, 40.0f, 0.8f}});

    const MotReadResult result = readString(out.str());
    CHECK(result.ok());
    CHECK_EQ(result.records.size(), std::size_t{2});
    if (result.records.size() == 2) {
        CHECK_EQ(result.records[0].frame, 1);
        CHECK_EQ(result.records[1].frame, 3);
    }
}

void testDetectionConvention() {
    const std::string text = writeAll({{1, kMotDetectionId, 10.0f, 20.0f, 30.0f, 40.0f, 0.75f}});
    CHECK(text.find("1,-1,") == 0);

    const MotReadResult result = readString(text);
    CHECK(result.ok());
    if (!result.records.empty()) {
        CHECK_EQ(result.records[0].id, kMotDetectionId);
    }
}

// A MOTChallenge det.txt has 10 columns and a gt.txt has 9. Both must read, or
// external data cannot be used as input.
void testExtraColumnsAreIgnored() {
    const MotReadResult det = readString("1,-1,10,20,30,40,0.9,-1,-1,-1\n");
    CHECK(det.ok());
    CHECK_EQ(det.records.size(), std::size_t{1});

    const MotReadResult gt = readString("1,2,10,20,30,40,1,1,0.85\n");
    CHECK(gt.ok());
    CHECK_EQ(gt.records.size(), std::size_t{1});
    if (!gt.records.empty()) {
        CHECK_EQ(gt.records[0].id, 2);
        CHECK_EQ(gt.records[0].confidence, 1.0f);
    }
}

void testBlankLinesAndSpacingAreTolerated() {
    const MotReadResult result = readString("\n1, 2, 10, 20, 30, 40, 0.5,-1,-1,-1\n\n  \n");
    CHECK(result.ok());
    CHECK_EQ(result.records.size(), std::size_t{1});
    if (!result.records.empty()) {
        CHECK_EQ(result.records[0].id, 2);
        CHECK_EQ(result.records[0].x, 10.0f);
    }
}

// Every rejection names the line, and leaves nothing behind: a caller that
// ignores `error` must not find a half-read file in `records`.
void testMalformedInputIsRejected() {
    struct Case {
        const char *text;
        const char *label;
    };
    const Case cases[] = {
        {"1,2,10,20,30,40,0.5\n1,2,10,20\n", "truncated line"},
        {"1,2,10,20,30,40,0.5\n2,2,ten,20,30,40,0.5\n", "non-numeric column"},
        {"1,2,10,20,30,40,0.5\n0,2,10,20,30,40,0.5\n", "frame number below 1"},
        {"1,2,10,20,30,40,0.5\n-3,2,10,20,30,40,0.5\n", "negative frame number"},
        {"1,2,12abc,20,30,40,0.5\n", "trailing garbage in a column"},
    };

    for (const Case &c : cases) {
        const MotReadResult result = readString(c.text);
        CHECK_LABELED(!result.ok(), c.label);
        CHECK_LABELED(result.records.empty(), c.label);
        CHECK_LABELED(result.error.find("test:") != std::string::npos, c.label);
    }

    // The reported line number must be the offending one, not the first.
    const MotReadResult second_line = readString("1,2,10,20,30,40,0.5\n1,2,10,20\n");
    CHECK(second_line.error.find("test:2") != std::string::npos);
}

void testMissingFileIsAnError() {
    const MotReadResult result = readMotFile("no/such/file.txt");
    CHECK(!result.ok());
    CHECK(result.records.empty());
}

} // namespace

int main() {
    testRoundTripIsLossless();
    testWholeNumbersStayShort();
    testFrameNumbersAreOneBased();
    testEmptyFrameWritesNothing();
    testDetectionConvention();
    testExtraColumnsAreIgnored();
    testBlankLinesAndSpacingAreTolerated();
    testMalformedInputIsRejected();
    testMissingFileIsAnError();
    return test_util::summary("mot_file");
}
