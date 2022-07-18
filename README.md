# Parallel Stream Processing Library

*do not use. just messing around for now*

A collection of reusable clases to for processing pipelines of work in parallel

The basic idea...
```
    // Compute stuff in parallel
    std::vector<int> input{1, 2, 3};
    parallel_streams squares(input.begin(), input.end(), [](int i){ return i * i; });

    // Use items as soon as they are done
    for (auto square : squares)
        printf("%i
```

Continuing the idea...
```
    std::vector<int> input{1, 2, 3};
    thread_pool threads;
    parallel_streams squareInts(
        input.begin(), input.end(), [](int i) { return i * i; }, threads);
    parallel_streams squareStrings(
        squareInts.begin(), squareInts.end(),
        [](int i) { return std::to_string(i); }, threads);
    std::set<std::string> result(squareStrings.begin(), squareStrings.end());
    // result == {"1", "4", "9"}
```

## Tests

```
cmake -DBUILD_TESTING=on -S . -B build
cd build
make
./test/unit_tests
```
