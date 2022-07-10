/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include <stdio.h>
#include <assert.h>
#include <psp/stream_processor.hpp>

using namespace psp;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  std::vector<int> thingsToDo((int*)data, (int*)data + size / sizeof(int));

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
    int verify = 0;
    for (auto& item : thingsToDo) verify += item;
  printf("Check %i == %i\n", sum, verify);
    assert(sum == verify);
  return 0;  // Values other than 0 and -1 are reserved for future use.
}
