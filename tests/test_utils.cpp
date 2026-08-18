//
// Pure-logic helpers from app/src/utils.cpp.
//
#include "test_util.hpp"
#include "utils.hpp"

#include <string>
#include <vector>

namespace {

void testSplitString() {
    const std::vector<std::string> classes = splitString("person,car,bus", ',');
    CHECK_EQ(classes.size(), 3u);
    CHECK(classes[0] == "person");
    CHECK(classes[1] == "car");
    CHECK(classes[2] == "bus");

    // Whitespace around entries is trimmed and empty entries are dropped.
    const std::vector<std::string> padded = splitString(" person , car ,, bus ", ',');
    CHECK_EQ(padded.size(), 3u);
    CHECK(padded[0] == "person");
    CHECK(padded[2] == "bus");

    CHECK(splitString("", ',').empty());
    CHECK_EQ(splitString("person", ',').size(), 1u);
}

void testGenerateOutputPath() {
    CHECK(generateOutputPath("clip.mp4") == "clip_processed.mp4");
    CHECK(generateOutputPath("/videos/street.avi") == "street_processed.avi");

    // A source with no extension (an RTSP URL, a device index) gets a default.
    CHECK(generateOutputPath("rtsp://camera/stream") == "output_processed.mp4");
    CHECK(generateOutputPath("") == "output_processed.mp4");
}

} // namespace

int main() {
    testSplitString();
    testGenerateOutputPath();
    return test_util::summary("utils");
}
