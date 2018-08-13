/* <dory/compress/gzip/gzip_codec.test.cc>

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

   Unit test for <dory/compress/gzip_codec.h>.
 */

#include <dory/compress/gzip/gzip_codec.h>

#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::Compress::Gzip;

namespace {

  /* The fixture for testing class TGzipCodec. */
  class TGzipCodecTest : public ::testing::Test {
    protected:
    TGzipCodecTest() = default;

    ~TGzipCodecTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TGzipCodecTest

  TEST_F(TGzipCodecTest, BasicTest) {
    const TGzipCodec &codec = TGzipCodec::The();
    TOpt<int> default_level = codec.GetRealCompressionLevel(TOpt<int>());
    ASSERT_TRUE(default_level.IsKnown());
    TOpt<int> level = codec.GetRealCompressionLevel(TOpt<int>(5));
    ASSERT_TRUE(level.IsKnown());
    ASSERT_EQ(*level, 5);
    level = codec.GetRealCompressionLevel(TOpt<int>(6));
    ASSERT_TRUE(level.IsKnown());
    ASSERT_EQ(*level, 6);
    level = codec.GetRealCompressionLevel(TOpt<int>(1000000));
    ASSERT_TRUE(level.IsKnown());
    ASSERT_EQ(*level, *default_level);

    std::string to_compress;

    for (size_t i = 0; i < 1024; ++i) {
      to_compress += "a bunch of junk to compress";
    }

    level = TOpt<int>();
    std::vector<char> compressed_output(
        codec.ComputeCompressedResultBufSpace(to_compress.data(),
            to_compress.size(), level));
    size_t result_size = codec.Compress(to_compress.data(), to_compress.size(),
        &compressed_output[0], compressed_output.size(), level);
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

    level = TOpt<int>(5);
    compressed_output.resize(
        codec.ComputeCompressedResultBufSpace(to_compress.data(),
            to_compress.size(), level));
    result_size = codec.Compress(to_compress.data(), to_compress.size(),
        &compressed_output[0], compressed_output.size(), level);
    ASSERT_LE(result_size, compressed_output.size());
    ASSERT_LT(result_size, to_compress.size());
    compressed_output.resize(result_size);

    uncompressed_output.resize(
        codec.ComputeUncompressedResultBufSpace(&compressed_output[0],
            compressed_output.size()));
    result_size = codec.Uncompress(&compressed_output[0],
        compressed_output.size(), &uncompressed_output[0],
        uncompressed_output.size());
    ASSERT_LE(result_size, uncompressed_output.size());
    uncompressed_output.resize(result_size);

    final_result.assign(uncompressed_output.begin(),
        uncompressed_output.end());
    ASSERT_EQ(final_result, to_compress);

    level = TOpt<int>(9);
    compressed_output.resize(
        codec.ComputeCompressedResultBufSpace(to_compress.data(),
            to_compress.size(), level));
    result_size = codec.Compress(to_compress.data(), to_compress.size(),
        &compressed_output[0], compressed_output.size(), level);
    ASSERT_LE(result_size, compressed_output.size());
    ASSERT_LT(result_size, to_compress.size());
    compressed_output.resize(result_size);

    uncompressed_output.resize(
        codec.ComputeUncompressedResultBufSpace(&compressed_output[0],
            compressed_output.size()));
    result_size = codec.Uncompress(&compressed_output[0],
        compressed_output.size(), &uncompressed_output[0],
        uncompressed_output.size());
    ASSERT_LE(result_size, uncompressed_output.size());
    uncompressed_output.resize(result_size);

    final_result.assign(uncompressed_output.begin(),
        uncompressed_output.end());
    ASSERT_EQ(final_result, to_compress);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
