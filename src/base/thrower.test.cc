/* <base/thrower.test.cc>
 
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
 
   Unit test for <base/thrower.h>.
 */

#include <base/thrower.h>
 
#include <cstddef> 
#include <string>
#include <vector>

#include <base/error_util.h>

#include <gtest/gtest.h>
  
using namespace Base;

namespace {

  void GetParts(const char *msg, std::vector<std::string> &parts) {
    assert(msg);
    parts.clear();
    const char
        *start  = nullptr,
        *end    = nullptr,
        *cursor = msg;
    for (;;) {
      char c = *cursor;
      if (start) {
        if (!c || c == ';') {
          parts.push_back(std::string(start, end + 1));
          if (!c) {
            break;
          }
          start = nullptr;
          end   = nullptr;
        } else if (!isspace(c)) {
          end = cursor;
        }
      } else {
        if (!c) {
          break;
        }
        if (!isspace(c)) {
          start = cursor;
          end   = cursor;
        }
      }
      ++cursor;
    }
  }

  /* The fixture for testing exception throwing stuff. */
  class TThrowerTest : public ::testing::Test {
    protected:
    TThrowerTest() = default;

    ~TThrowerTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TThrowerTest
  
  TEST_F(TThrowerTest, GetParts) {
    const std::vector<std::string> expected = {
      "hello", "doctor", "name", "continue  yesterday"
    };

    std::vector<std::string> actual;
    GetParts("   hello; doctor;name   ;  continue  yesterday", actual);
    ASSERT_EQ(actual.size(), expected.size());

    for (size_t i = 0; i < expected.size(); ++i) {
      ASSERT_EQ(actual[i], expected[i]);
    }
  }
  
  DEFINE_ERROR(TFoo, std::invalid_argument, "a fooness has occurred");
  
  const char
      *Extra1 = "some stuff",
      *Extra2 = "some more stuff";
  
  template <typename TThrowAs, typename TCatchAs>
  void ThrowIt() {
    std::vector<std::string> parts;
    try {
      THROW_ERROR(TThrowAs) << Extra1 << EndOfPart << Extra2;
    } catch (const TCatchAs &ex) {
      GetParts(ex.what(), parts);
    }
    ASSERT_EQ(parts.size(), 4u);
    ASSERT_EQ(parts[1], TFoo::GetDesc());
    ASSERT_EQ(parts[2], Extra1);
    ASSERT_EQ(parts[3], Extra2);
  }
  
  TEST_F(TThrowerTest, Typical) {
    ThrowIt<TFoo, TFoo>();
    ThrowIt<TFoo, std::invalid_argument>();
    ThrowIt<TFoo, std::logic_error>();
    ThrowIt<TFoo, std::exception>();
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  DieOnTerminate();
  return RUN_ALL_TESTS();
}
