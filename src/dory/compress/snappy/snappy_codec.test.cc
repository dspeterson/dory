/* <dory/compress/snappy/snappy_codec.test.cc>

   ----------------------------------------------------------------------------
   Copyright 2017 Dave Peterson <dave@dspeterson.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
   ----------------------------------------------------------------------------

   Unit test for <dory/compress/snappy/snappy_codec.h>.
 */

#include <dory/compress/snappy/snappy_codec.h>

#include <optional>
#include <string>
#include <vector>

#include <base/tmp_file.h>
#include <test_util/test_logging.h>

#include <gtest/gtest.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::Compress::Snappy;
using namespace ::TestUtil;

namespace {

  /* The fixture for testing class TSnappyCodec. */
  class TSnappyCodecTest : public ::testing::Test {
    protected:
    TSnappyCodecTest() = default;

    ~TSnappyCodecTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TSnappyCodecTest

  TEST_F(TSnappyCodecTest, BasicTest) {
    const TSnappyCodec &codec = TSnappyCodec::The();
    auto level = codec.GetRealCompressionLevel(std::nullopt);
    ASSERT_FALSE(level.has_value());
    level = codec.GetRealCompressionLevel(5);
    ASSERT_FALSE(level.has_value());
    std::string to_compress;

    for (size_t i = 0; i < 1024; ++i) {
      to_compress += "a bunch of junk to compress";
    }

    std::vector<char> compressed_output(
        codec.ComputeCompressedResultBufSpace(to_compress.data(),
            to_compress.size(), std::nullopt));
    size_t result_size = codec.Compress(to_compress.data(), to_compress.size(),
        &compressed_output[0], compressed_output.size(), std::nullopt);
    ASSERT_LE(result_size, compressed_output.size());
    ASSERT_LT(result_size, to_compress.size());
    compressed_output.resize(result_size);

    std::vector<char> uncompressed_output(
        codec.ComputeUncompressedResultBufSpace(&compressed_output[0],
            compressed_output.size()));
    result_size = codec.Uncompress(&compressed_output[0],
        compressed_output.size(), &uncompressed_output[0],
        uncompressed_output.size());
    ASSERT_LE(result_size, uncompressed_output.size());
    uncompressed_output.resize(result_size);

    std::string final_result(uncompressed_output.begin(),
        uncompressed_output.end());
    ASSERT_EQ(final_result, to_compress);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  TTmpFile test_logfile = InitTestLogging(argv[0]);
  return RUN_ALL_TESTS();
}
