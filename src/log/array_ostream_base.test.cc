/* <log/array_ostream_base.test.cc>
 
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
 
   Unit test for <log/array_ostream_base.h>.
 */

#include <log/array_ostream_base.h>

#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include <iostream>
  
using namespace Log;
 
namespace {

  template <size_t BufSize>
  class TTestStream : public TArrayOstreamBase<BufSize> {
    public:
    explicit TTestStream(size_t reserve_bytes)
        : TArrayOstreamBase<BufSize>(reserve_bytes) {
    }

    char *GetBuffer() noexcept {
      assert(this);
      return TArrayOstreamBase<BufSize>::GetBuf();
    }

    char *GetEnd() noexcept {
      assert(this);
      return TArrayOstreamBase<BufSize>::GetPos();
    }
  };  // TTestStream

  /* The fixture for testing class TIndent. */
  class TArrayOstreamBaseTest : public ::testing::Test {
    protected:
    TArrayOstreamBaseTest() {
    }

    virtual ~TArrayOstreamBaseTest() {
    }

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
  };  // TArrayOstreamBaseTest

  TEST_F(TArrayOstreamBaseTest, Test1) {
    static const size_t BUF_SIZE = 20;
    static const size_t reserve_bytes = 0;
    TTestStream<BUF_SIZE> test_stream(reserve_bytes);
    ASSERT_TRUE(test_stream.IsEmpty());
    std::string msg("hello world");
    test_stream << msg;
    ASSERT_FALSE(test_stream.IsEmpty());
    ASSERT_EQ(test_stream.Size(), msg.size());
    std::string result(test_stream.GetBuffer(), test_stream.GetEnd());
    ASSERT_EQ(result, msg);
    test_stream << 5;
    test_stream << msg;
    ASSERT_EQ(test_stream.Size(), BUF_SIZE - reserve_bytes);
    result.assign(test_stream.GetBuffer(), test_stream.GetEnd());
    ASSERT_EQ(result, "hello world5hello wo");
  }

  TEST_F(TArrayOstreamBaseTest, Test2) {
    static const size_t BUF_SIZE = 20;
    static const size_t reserve_bytes = 1;
    TTestStream<BUF_SIZE> test_stream(reserve_bytes);
    ASSERT_TRUE(test_stream.IsEmpty());
    std::string msg("hello world");
    test_stream << msg;
    ASSERT_FALSE(test_stream.IsEmpty());
    ASSERT_EQ(test_stream.Size(), msg.size());
    std::string result(test_stream.GetBuffer(), test_stream.GetEnd());
    ASSERT_EQ(result, msg);
    test_stream << 5;
    test_stream << msg;
    ASSERT_EQ(test_stream.Size(), BUF_SIZE - reserve_bytes);
    result.assign(test_stream.GetBuffer(), test_stream.GetEnd());
    ASSERT_EQ(result, "hello world5hello w");
  }

  TEST_F(TArrayOstreamBaseTest, Test3) {
    static const size_t BUF_SIZE = 20;
    static const size_t reserve_bytes = 2;
    TTestStream<BUF_SIZE> test_stream(reserve_bytes);
    ASSERT_TRUE(test_stream.IsEmpty());
    std::string msg("hello world");
    test_stream << msg;
    ASSERT_FALSE(test_stream.IsEmpty());
    ASSERT_EQ(test_stream.Size(), msg.size());
    std::string result(test_stream.GetBuffer(), test_stream.GetEnd());
    ASSERT_EQ(result, msg);
    test_stream << 5;
    test_stream << msg;
    ASSERT_EQ(test_stream.Size(), BUF_SIZE - reserve_bytes);
    result.assign(test_stream.GetBuffer(), test_stream.GetEnd());
    ASSERT_EQ(result, "hello world5hello ");
  }
  
}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
