#include "oneflow/core/common/embedded_skiplist.h"
#include "oneflow/core/common/util.h"

namespace oneflow {

namespace test {

template<typename ElemKeyField>
class TestEmbeddedSkipListHead final : public EmbeddedSkipListHead<ElemKeyField> {
 public:
  TestEmbeddedSkipListHead() { this->__Init__(); }
};

struct FooSkipListElem {
  FooSkipListElem() { key.__Init__(); }

  EmbeddedSkipListKey<int> key;
  int value;
};

using FooSkipList = TestEmbeddedSkipListHead<STRUCT_FIELD(FooSkipListElem, key)>;

TEST(EmbeddedSkipList, empty) {
  FooSkipList skiplist;
  ASSERT_TRUE(skiplist.empty());
  ASSERT_EQ(skiplist.size(), 0);
}

TEST(EmbeddedSkipList, insert_naive) {
  FooSkipList skiplist;
  FooSkipListElem elem0;
  *elem0.key.mut_key() = 0;
  elem0.value = 1;
  skiplist.Insert(&elem0);
  ASSERT_EQ(skiplist.size(), 1);
  {
    auto* searched = skiplist.Find(int(0));
    ASSERT_EQ(searched, &elem0);
  }
  {
    auto* searched = skiplist.Find(int(-1));
    ASSERT_TRUE(searched == nullptr);
  }
}

TEST(EmbeddedSkipList, erase_by_key) {
  FooSkipList skiplist;
  FooSkipListElem elem0;
  *elem0.key.mut_key() = 0;
  elem0.value = 1;
  skiplist.Insert(&elem0);
  ASSERT_EQ(skiplist.size(), 1);
  ASSERT_TRUE(skiplist.Find(int(0)) != nullptr);
  skiplist.Erase(int(0));
  ASSERT_EQ(skiplist.size(), 0);
  ASSERT_TRUE(skiplist.Find(int(0)) == nullptr);
}

TEST(EmbeddedSkipList, erase_by_elem) {
  FooSkipList skiplist;
  FooSkipListElem elem0;
  *elem0.key.mut_key() = 0;
  elem0.value = 1;
  skiplist.Insert(&elem0);
  ASSERT_EQ(skiplist.size(), 1);
  ASSERT_TRUE(skiplist.Find(int(0)) != nullptr);
  skiplist.Erase(&elem0);
  ASSERT_EQ(skiplist.size(), 0);
  ASSERT_TRUE(skiplist.Find(int(0)) == nullptr);
}

TEST(EmbeddedSkipList, insert_many) {
  FooSkipList skiplist;
  FooSkipListElem exists[100];
  for (int i = 0; i < 100; ++i) {
    int key = i - 50;
    if (key >= 0) { ++key; }
    *exists[i].key.mut_key() = key;
    skiplist.Insert(&exists[i]);
    ASSERT_EQ(skiplist.Find(key), &exists[i]);
  }
  FooSkipListElem elem0;
  *elem0.key.mut_key() = 0;
  elem0.value = 1;
  skiplist.Insert(&elem0);
  ASSERT_EQ(skiplist.size(), 101);
  {
    auto* searched = skiplist.Find(int(0));
    ASSERT_EQ(searched, &elem0);
  }
  {
    auto* searched = skiplist.Find(int(-1001));
    ASSERT_TRUE(searched == nullptr);
  }
}

TEST(EmbeddedSkipList, erase_many_by_key) {
  FooSkipList skiplist;
  FooSkipListElem exists[100];
  for (int i = 0; i < 100; ++i) {
    int key = i - 50;
    if (key >= 0) { ++key; }
    *exists[i].key.mut_key() = key;
    skiplist.Insert(&exists[i]);
    ASSERT_EQ(skiplist.Find(key), &exists[i]);
  }
  FooSkipListElem elem0;
  *elem0.key.mut_key() = 0;
  elem0.value = 1;
  skiplist.Insert(&elem0);
  ASSERT_EQ(skiplist.size(), 101);
  ASSERT_TRUE(skiplist.Find(int(0)) != nullptr);
  skiplist.Erase(int(0));
  ASSERT_EQ(skiplist.size(), 100);
  ASSERT_TRUE(skiplist.Find(int(0)) == nullptr);
}

TEST(EmbeddedSkipList, erase_many_by_elem) {
  FooSkipList skiplist;
  FooSkipListElem exists[100];
  for (int i = 0; i < 100; ++i) {
    int key = i - 50;
    if (key >= 0) { ++key; }
    *exists[i].key.mut_key() = key;
    skiplist.Insert(&exists[i]);
    ASSERT_EQ(skiplist.Find(key), &exists[i]);
  }
  FooSkipListElem elem0;
  *elem0.key.mut_key() = 0;
  elem0.value = 1;
  skiplist.Insert(&elem0);
  ASSERT_EQ(skiplist.size(), 101);
  ASSERT_TRUE(skiplist.Find(int(0)) != nullptr);
  skiplist.Erase(&elem0);
  ASSERT_EQ(skiplist.size(), 100);
  ASSERT_TRUE(skiplist.Find(int(0)) == nullptr);
}

}  // namespace test

}  // namespace oneflow
