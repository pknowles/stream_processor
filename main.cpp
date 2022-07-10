
#include <assert.h>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <stdio.h>
#include <thread>
#include <type_traits>

template <class T> class stream_queue {
public:
  class iterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::size_t;
    using value_type = T;
    using pointer = value_type *;
    using reference = value_type &;
    using const_reference = const value_type &;

    iterator(stream_queue<value_type> &queue, std::size_t position)
        : m_queue(queue), m_position(position) {}
    const_reference operator*() const { read(); return *m_value; }
    reference operator*() { read(); return *m_value; }
    const pointer operator->() const { read(); return &m_value; }
    pointer operator->() { read(); return &m_value; }
    difference_type operator-(iterator &other) {
      return m_position - other.m_position;
    }
    iterator &operator++() {
      // Make sure to consume the value, e.g. when iterating without reading.
      read();
      m_value.reset();
      ++m_position;
      return *this;
    }
    iterator operator++(int) {
      iterator tmp(*this);
      ++(*this);
      return tmp;
    }
    bool operator==(const iterator &other) const {
      return m_position == other.m_position;
    };
    bool operator!=(const iterator &other) const {
      return m_position != other.m_position;
    };

  private:
    void read()
    {
        if (!m_value)
            m_value = m_queue.iterator_pop();
    }
    stream_queue<value_type> &m_queue;
    std::size_t m_position;
    std::optional<value_type> m_value;
  };

  void promise(size_t count) {
    std::lock_guard<std::mutex> lk(m_mutex);
    // Must promise everything up-front, before reading
    assert(!m_consuming);
    m_promised += count;
  }

  template <class V> void push(V &&value) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_queue.push(std::forward<V>(value));
    m_cond.notify_all();
  }

  template <typename Container> void push_from(Container &container) {
    // TODO: push in batches if the container is huge? maybe pass the iterator
    // to the consumer?
    std::lock_guard<std::mutex> lk(m_mutex);
    for (T &value : container)
      m_queue.push(value);
    m_cond.notify_all();
  }

  // Wait for, pop and return an element
  T pop() {
    std::unique_lock<std::mutex> lk(m_mutex);
    consume_locked(1);
    return pop_locked(lk);
  }

  // Wait for, pop and return an element if there are still promised items
  std::optional<T> try_pop() {
    std::optional<T> result;
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_consumed < m_promised) {
      consume_locked(1);
      result = pop_locked(lk);
    }
    return result;
  }

  std::size_t size() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_promised - m_consumed;
  }
  iterator begin() {
    std::unique_lock<std::mutex> lk(m_mutex);
    std::size_t tmp = m_consumed;
    consume_locked(m_promised - m_consumed);
    return iterator(*this, tmp);
  }
  iterator end() { return iterator(*this, m_promised); }

private:
  // Pop an element without incrementing m_consumed as it has been already
  // allocated to an iterator
  T iterator_pop() {
    std::unique_lock<std::mutex> lk(m_mutex);
    return pop_locked(lk);
  }

  // Not exception safe and involves a copy
  T pop_locked(std::unique_lock<std::mutex> &lk) {
    m_consuming = true;
    m_cond.wait(lk, [&] { return !m_queue.empty(); });
    T value = std::move(m_queue.front());
    m_queue.pop();
    return value;
  }

  void consume_locked(size_t items) {
    m_consumed += items;

    // Allow reuse after consuming all items, i.e. more promises
    if (m_consumed == m_promised)
      m_consuming = false;
  }

  std::condition_variable m_cond;
  mutable std::mutex m_mutex;
  std::queue<T> m_queue;
  std::size_t m_consumed{0};
  std::size_t m_promised{0};
  bool m_consuming{false};
};

template <class F> struct function_traits;

// function pointer
template <class R, class... Args>
struct function_traits<R (*)(Args...)> : public function_traits<R(Args...)> {};

// member function pointer
template <class C, class R, class... Args>
struct function_traits<R (C::*)(Args...)>
    : public function_traits<R(C &, Args...)> {};

// member object pointer
template <class C, class R>
struct function_traits<R(C::*)> : public function_traits<R(C &)> {};

// const member function pointer
template <class C, class R, class... Args>
struct function_traits<R (C::*)(Args...) const>
    : public function_traits<R(C &, Args...)> {};

// functor
template <class F> struct function_traits {
private:
  using call_type = function_traits<decltype(&F::operator())>;

public:
  using return_type = typename call_type::return_type;

  // O.o
  template <typename R, typename As> static As pro_args(std::function<R(As)>);

  template <typename R, typename... As>
  static std::tuple<As...> pro_args(std::function<R(As...)>);

  using arg_type = decltype(pro_args(std::function{std::declval<F>()}));
};

template <class F> struct function_traits<F &> : public function_traits<F> {};

template <class F> struct function_traits<F &&> : public function_traits<F> {};

template <class R, class... Args> struct function_traits<R(Args...)> {
  using return_type = R;
  using arg_type = std::tuple<Args...>;
};

template <class R, class Arg> struct function_traits<R(Arg)> {
  using return_type = R;
  using arg_type = Arg;
};

template <class Func> class stream_processor {
public:
  using input_value = typename function_traits<Func>::arg_type;
  using output_value = typename function_traits<Func>::return_type;
  using output_iterator = typename stream_queue<output_value>::iterator;

  stream_processor(const Func &func) : m_func(func) {}

  template <class T> void push(T &&value) {
    m_input.promise(1);
    m_output.promise(1);
    m_input.push(std::make_tuple(std::forward<T>(value)));
  }

  // connect multiple streams and avoid a stall where one to use push_from
  template <class OtherFunc>
  void take_from(stream_processor<OtherFunc> &other) {
    printf("Push from queue\n");
    m_input.promise(other.size());
    m_output.promise(other.size());
    other.m_outputNext = &m_input;
  }

  template <class Container> void push_from(Container &&container) {
    printf("Push from container\n");
    m_input.promise(container.size());
    m_output.promise(container.size());
    m_input.push_from(std::forward<Container>(container));
  }

  output_iterator begin() { return m_output.begin(); }
  output_iterator end() { return m_output.end(); }
  output_value pop() { return m_output.pop(); }
  std::size_t size() const { return m_output.size(); }

  void process() {
    stream_queue<output_value> *output =
        m_outputNext ? m_outputNext : &m_output;
    std::optional<input_value> args;
    // TODO: use std::apply(m_func, args) when args are a tuple
    while ((args = m_input.try_pop()))
      output->push(m_func(*args));
  }

private:
  Func m_func;
  stream_queue<input_value> m_input;
  stream_queue<output_value> m_output;

public:
  stream_queue<output_value> *m_outputNext{};
};

class thread_group {
public:
  template <class Func, class... Args>
  void start(size_t n, Func &&entrypoint, Args &&...args) {}
  void join() {}

private:
};

template <class Func> class parallel_streams : public stream_processor<Func> {
public:
  parallel_streams(const Func &func) : stream_processor<Func>(func) {}

  void start(size_t thread_count = std::thread::hardware_concurrency()) {
    m_threads.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i)
      m_threads.emplace_back(&stream_processor<Func>::process,
                             (stream_processor<Func> *)this);
  }
  ~parallel_streams() {
    for (auto &thread : m_threads)
      thread.join();
  }

private:
  std::vector<std::thread> m_threads;
};

int main() {
  std::vector<int> thingsToDo;
  for (int i = 1; i <= 9; ++i)
    thingsToDo.push_back(i);

  auto increment = [](int item) -> int { printf("Inc to %i\n", item); return item + 1;};
  parallel_streams incrementRunner(increment);

  incrementRunner.push_from(thingsToDo);

  auto decrement = [](int item) -> int { printf("Dec %i\n", item); return item - 1; };
  parallel_streams decrementRunner(decrement);

  decrementRunner.take_from(incrementRunner);

  incrementRunner.start(1);
  decrementRunner.start(1);

  int sum = 0;
  for (auto &item : decrementRunner)
    sum += item;
  printf("Sum: %i <-- should be 45\n", sum);
  return sum;
}