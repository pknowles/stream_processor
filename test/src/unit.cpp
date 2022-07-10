/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include <gtest/gtest.h>
#include <stdio.h>

#include <psp/stream_processor.hpp>

using namespace psp;

// Demonstrate some basic assertions.
TEST(HelloTest, BasicAssertions) {
  // Expect two strings not to be equal.
  EXPECT_STRNE("hello", "world");
  // Expect equality.
  EXPECT_EQ(7 * 6, 42);
}

int main() {
  std::vector<int> thingsToDo;
  for (int i = 1; i <= 9; ++i) thingsToDo.push_back(i);

  auto increment = [](int item) -> int {
    printf("Inc to %i\n", item);
    return item + 1;
  };
  parallel_streams incrementRunner(increment);

  incrementRunner.push_from(thingsToDo);

  auto decrement = [](int item) -> int {
    printf("Dec %i\n", item);
    return item - 1;
  };
  parallel_streams decrementRunner(decrement);

  decrementRunner.take_from(incrementRunner);

  incrementRunner.start(1);
  decrementRunner.start(1);

  int sum = 0;
  for (auto& item : decrementRunner) sum += item;
  printf("Sum: %i <-- should be 45\n", sum);
  return 0;
}
