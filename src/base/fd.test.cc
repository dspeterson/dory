/* <base/fd.test.cc>
 
   ----------------------------------------------------------------------------
   Copyright 2010-2013 if(we)

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
 
   Unit test for <base/fd.h>.
 */

#include <base/fd.h>
 
#include <cstddef> 
#include <cstring>
  
#include <base/io_util.h>
#include <base/zero.h>
  
#include <gtest/gtest.h>
  
using namespace Base;
 
namespace {

  /* The fixture for testing class TFd. */
  class TFdTest : public ::testing::Test {
    protected:
    TFdTest() = default;

    ~TFdTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TFdTest

  const char *ExpectedData = "hello";
  const size_t ExpectedSize = strlen(ExpectedData);
  
  const size_t MaxActualSize = 1024;
  char ActualData[MaxActualSize];
  
  void Transact(const TFd &readable, const TFd &writeable) {
    WriteExactly(writeable, ExpectedData, ExpectedSize);
    Zero(ActualData);
    size_t actual_size = ReadAtMost(readable, ActualData, MaxActualSize);
    ASSERT_EQ(actual_size, ExpectedSize);
    ASSERT_FALSE(strcmp(ActualData, ExpectedData));
  }
  
  void CheckClose(const TFd &readable, TFd &writeable) {
    writeable.Reset();
    size_t actual_size = ReadAtMost(readable, ActualData, MaxActualSize);
    ASSERT_FALSE(actual_size);
  }
  
  TEST_F(TFdTest, Pipe) {
    TFd readable, writeable;
    TFd::Pipe(readable, writeable);
    Transact(readable, writeable);
    CheckClose(readable, writeable);
  }
  
  TEST_F(TFdTest, SocketPair) {
    TFd lhs, rhs;
    TFd::SocketPair(lhs, rhs, AF_UNIX, SOCK_STREAM, 0);
    Transact(lhs, rhs);
    Transact(rhs, lhs);
    CheckClose(lhs, rhs);
  }

  TEST_F(TFdTest, Swap) {
    TFd fd_1(1);
    TFd fd_2(2);
    fd_1.Swap(fd_2);
    ASSERT_EQ(int(fd_1), 2);
    ASSERT_EQ(int(fd_2), 1);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
