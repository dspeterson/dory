/* <base/opt.test.cc>
 
   ----------------------------------------------------------------------------
   Copyright 2010-2013 if(we)
   Copyright 2018 Dave Peterson <dave@dspeterson.com>

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
 
   Unit test for <base/opt.h>.
 */
  
#include <base/opt.h>

#include <cassert>
#include <string>
#include <utility>

#include <base/error_util.h>

#include <gtest/gtest.h>

using namespace Base;

namespace {

  /* The fixture for testing class TOpt. */
  class TOptTest : public ::testing::Test {
    protected:
    TOptTest() = default;

    ~TOptTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TOptTest

  TEST_F(TOptTest, BasicTests) {
    TOpt<int> opt;
    ASSERT_FALSE(opt);
    ASSERT_FALSE(opt.IsKnown());
    ASSERT_TRUE(opt.IsUnknown());
    ASSERT_EQ(opt.TryGet(), nullptr);
    opt = 0;
    ASSERT_TRUE(opt);
    ASSERT_TRUE(opt.IsKnown());
    ASSERT_FALSE(opt.IsUnknown());
    ASSERT_EQ(*opt, 0);
    *opt = 5;
    ASSERT_TRUE(opt.IsKnown());
    ASSERT_EQ(*opt, 5);
    ASSERT_EQ(*opt.Get(), 5);
    ASSERT_EQ(*opt.TryGet(), 5);
    opt.MakeKnown(10);  // no-op since 'opt' was already known
    ASSERT_EQ(*opt, 5);
    opt.Reset();
    ASSERT_FALSE(opt);
    ASSERT_FALSE(opt.IsKnown());
    ASSERT_TRUE(opt.IsUnknown());
    ASSERT_FALSE(*opt.Unknown);
    opt.MakeKnown(20);
    ASSERT_TRUE(opt.IsKnown());
    ASSERT_EQ(*opt, 20);
  }

  class THolder {
    public:
    THolder() = default;

    explicit THolder(const std::string &s)
        : Value(s) {
    }

    explicit THolder(std::string &&s) noexcept
        : Value(std::move(s)) {
      /* Explicitly clear s to make it obvious that s was moved from. */
      s.clear();
    }

    THolder(const THolder &) = default;

    THolder(THolder &&other) noexcept
        : Value(std::move(other.Value)) {
      /* Explicitly clear other.Value to make it obvious that other.Value was
         moved from. */
      other.Value.clear();
    }

    ~THolder() {
      if (DtorFlag) {
        *DtorFlag = true;
      }
    }

    THolder &operator=(const THolder &) = default;

    THolder &operator=(THolder &&other) noexcept {
      Value = std::move(other.Value);

      /* Explicitly clear other.Value to make it obvious that other.Value was
         moved from. */
      other.Value.clear();

      return *this;
    }

    void SetDtorFlag(bool &flag) noexcept {
      DtorFlag = &flag;
      *DtorFlag = false;
    }

    const std::string &GetValue() const noexcept {
      return Value;
    }

    std::string &GetValue() noexcept {
      return Value;
    }

    bool IsEmpty() const noexcept {
      return Value.empty();
    }

    private:
    bool *DtorFlag = nullptr;

    std::string Value;
  };

  TEST_F(TOptTest, Reset) {
    TOpt<THolder> opt(THolder("blah"));
    ASSERT_TRUE(opt.IsKnown());
    ASSERT_EQ(opt->GetValue(), "blah");
    bool dtor_called = false;
    opt->SetDtorFlag(dtor_called);
    opt.Reset();
    ASSERT_TRUE(dtor_called);
    ASSERT_FALSE(opt.IsKnown());
  }

  TEST_F(TOptTest, MoveConstruction) {
    // test move construction from TVal
    THolder h("blah");
    ASSERT_FALSE(h.IsEmpty());
    TOpt<THolder> opt1(std::move(h));
    ASSERT_TRUE(h.IsEmpty());
    ASSERT_TRUE(opt1.IsKnown());
    ASSERT_EQ(opt1->GetValue(), "blah");

    // test move construction from nonempty TOpt
    TOpt<THolder> opt2(std::move(opt1));
    ASSERT_TRUE(opt2.IsKnown());
    ASSERT_TRUE(opt1.IsKnown());
    ASSERT_EQ(opt2->GetValue(), "blah");
    ASSERT_TRUE(opt1->IsEmpty());

    // test move construction from empty TOpt
    TOpt<THolder> opt3;
    ASSERT_FALSE(opt3.IsKnown());
    TOpt<THolder> opt4(std::move(opt3));
    ASSERT_FALSE(opt3.IsKnown());
    ASSERT_FALSE(opt4.IsKnown());
  }

  TEST_F(TOptTest, MoveAssignment) {
    // test move assignment from THolder to empty TOpt
    THolder h("blah");
    ASSERT_FALSE(h.IsEmpty());
    TOpt<THolder> opt1;
    ASSERT_FALSE(opt1.IsKnown());
    opt1 = std::move(h);
    ASSERT_TRUE(h.IsEmpty());
    ASSERT_TRUE(opt1.IsKnown());
    ASSERT_EQ(opt1->GetValue(), "blah");

    // test move assignment from THolder to nonempty TOpt
    h.GetValue() = "duh";
    ASSERT_FALSE(h.IsEmpty());
    opt1 = std::move(h);
    ASSERT_TRUE(h.IsEmpty());
    ASSERT_TRUE(opt1.IsKnown());
    ASSERT_EQ(opt1->GetValue(), "duh");

    // test move assignment from nonempty TOpt to nonempty TOpt
    TOpt<THolder> opt2(THolder("barf"));
    ASSERT_TRUE(opt2.IsKnown());
    ASSERT_EQ(opt2->GetValue(), "barf");
    bool dtor_called = false;
    opt2->SetDtorFlag(dtor_called);
    opt2 = std::move(opt1);
    ASSERT_FALSE(dtor_called);
    ASSERT_TRUE(opt1.IsKnown());
    ASSERT_TRUE(opt2.IsKnown());
    ASSERT_EQ(opt2->GetValue(), "duh");
    ASSERT_TRUE(opt1->IsEmpty());

    // test move assignment from empty TOpt to nonempty TOpt
    opt1.Reset();
    ASSERT_FALSE(opt1.IsKnown());
    opt2->SetDtorFlag(dtor_called);
    opt2 = std::move(opt1);
    ASSERT_TRUE(dtor_called);
    ASSERT_FALSE(opt1.IsKnown());
    ASSERT_FALSE(opt2.IsKnown());

    // test move assignment from nonempty TOpt to empty TOpt
    opt1 = THolder("hiccup");
    ASSERT_TRUE(opt1.IsKnown());
    opt2 = std::move(opt1);
    ASSERT_TRUE(opt1.IsKnown());
    ASSERT_TRUE(opt2.IsKnown());
    ASSERT_TRUE(opt1->IsEmpty());
    ASSERT_EQ(opt2->GetValue(), "hiccup");

    // test move assignment from empty TOpt to empty TOpt
    opt1.Reset();
    opt2.Reset();
    ASSERT_FALSE(opt1.IsKnown());
    ASSERT_FALSE(opt2.IsKnown());
    opt2 = std::move(opt1);
    ASSERT_FALSE(opt1.IsKnown());
    ASSERT_FALSE(opt2.IsKnown());
  }

  TEST_F(TOptTest, CopyConstruction) {
    // test copy construction from TVal
    THolder h("blah");
    ASSERT_FALSE(h.IsEmpty());
    TOpt<THolder> opt1(h);
    ASSERT_EQ(h.GetValue(), "blah");
    ASSERT_TRUE(opt1.IsKnown());
    ASSERT_EQ(opt1->GetValue(), "blah");

    // test copy construction from nonempty TOpt
    TOpt<THolder> opt2(opt1);
    ASSERT_TRUE(opt2.IsKnown());
    ASSERT_TRUE(opt1.IsKnown());
    ASSERT_EQ(opt2->GetValue(), "blah");
    ASSERT_EQ(opt1->GetValue(), "blah");

    // test copy construction from empty TOpt
    TOpt<THolder> opt3;
    ASSERT_FALSE(opt3.IsKnown());
    TOpt<THolder> opt4(opt3);
    ASSERT_FALSE(opt3.IsKnown());
    ASSERT_FALSE(opt4.IsKnown());
  }

  TEST_F(TOptTest, CopyAssignment) {
    // test copy assignment from THolder to empty TOpt
    THolder h("blah");
    ASSERT_FALSE(h.IsEmpty());
    TOpt<THolder> opt1;
    ASSERT_FALSE(opt1.IsKnown());
    opt1 = h;
    ASSERT_EQ(h.GetValue(), "blah");
    ASSERT_TRUE(opt1.IsKnown());
    ASSERT_EQ(opt1->GetValue(), "blah");

    // test copy assignment from THolder to nonempty TOpt
    h.GetValue() = "duh";
    ASSERT_FALSE(h.IsEmpty());
    opt1 = h;
    ASSERT_EQ(h.GetValue(), "duh");
    ASSERT_TRUE(opt1.IsKnown());
    ASSERT_EQ(opt1->GetValue(), "duh");

    // test copy assignment from nonempty TOpt to nonempty TOpt
    TOpt<THolder> opt2(THolder("barf"));
    ASSERT_TRUE(opt2.IsKnown());
    ASSERT_EQ(opt2->GetValue(), "barf");
    bool dtor_called = false;
    opt2->SetDtorFlag(dtor_called);
    opt2 = opt1;
    ASSERT_FALSE(dtor_called);
    ASSERT_TRUE(opt1.IsKnown());
    ASSERT_TRUE(opt2.IsKnown());
    ASSERT_EQ(opt2->GetValue(), "duh");
    ASSERT_EQ(opt1->GetValue(), "duh");

    // test copy assignment from empty TOpt to nonempty TOpt
    opt1.Reset();
    ASSERT_FALSE(opt1.IsKnown());
    opt2->SetDtorFlag(dtor_called);
    opt2 = opt1;
    ASSERT_TRUE(dtor_called);
    ASSERT_FALSE(opt1.IsKnown());
    ASSERT_FALSE(opt2.IsKnown());

    // test copy assignment from nonempty TOpt to empty TOpt
    opt1 = THolder("hiccup");
    ASSERT_TRUE(opt1.IsKnown());
    opt2 = opt1;
    ASSERT_TRUE(opt1.IsKnown());
    ASSERT_TRUE(opt2.IsKnown());
    ASSERT_EQ(opt1->GetValue(), "hiccup");
    ASSERT_EQ(opt2->GetValue(), "hiccup");

    // test copy assignment from empty TOpt to empty TOpt
    opt1.Reset();
    opt2.Reset();
    ASSERT_FALSE(opt1.IsKnown());
    ASSERT_FALSE(opt2.IsKnown());
    opt2 = opt1;
    ASSERT_FALSE(opt1.IsKnown());
    ASSERT_FALSE(opt2.IsKnown());
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  DieOnTerminate();
  return RUN_ALL_TESTS();
}
