#include <iostream>
#include <algorithm>
#include <vector>
#include <random>


// ----------------------------
//           Interface
// ----------------------------


struct Hash {
  size_t summand;
  size_t factor;
  size_t limiter;

  static const size_t modulo_ = 2000000001;
  static const int minimal_number_ = -1000000000;

  size_t operator() (const size_t number) const;
};

class HashGenerator {
public:
  static HashGenerator& getInstance();
  Hash generate_random_hash(size_t limiter) const;

private:
  static std::mt19937 generator_;
  HashGenerator() {};

  HashGenerator(const HashGenerator&) = delete;
  HashGenerator(const HashGenerator&&) = delete;
  void operator=(const HashGenerator&) = delete;
  void operator=(const HashGenerator&&) = delete;
};

class Bucket {
public:
  Bucket();
  explicit Bucket(size_t capacity);

  void Initialize(const std::vector<int>& numbers);
  bool Contains(int number) const;

private:
  struct BucketElement {
    int number;
    bool is_empty;

    BucketElement();
    BucketElement(int number, bool is_empty);
  };

  size_t capacity_;
  std::vector<BucketElement> elements_;


  Hash hash_;

  bool check_if_need_rehashing(const std::vector<int>& numbers) const;
  void set_hash(const std::vector<int>& numbers);
};

class FixedSet {
public:
  FixedSet();

  void Initialize(const std::vector<int>& numbers);
  bool Contains(int number) const;

private:
  size_t number_of_buckets_;
  std::vector<Bucket> buckets_;

  static const size_t number_of_buckets_factor_ = 2;
  static const size_t total_buckets_size_factor_ = 10;

  Hash hash_;

  std::vector<std::vector<int>> set_hash(const std::vector<int>& numbers);
};

std::vector<char> check_existance_queries_in_numbers(const std::vector<int>& numbers,
    const std::vector<int>& queries);

size_t read_number(std::istream& istream);

std::vector<int> read_vector(std::istream& istream,
    size_t amount_of_numbers);

void write_existance_of_queries_in_numbers(std::ostream& ostream,
    const std::vector<char>& queries_existance);


// ----------------------------
//           Realization
// ----------------------------


size_t Hash::operator() (const size_t number) const {
  size_t key = number - minimal_number_;

  return ((static_cast<long long>(factor) * static_cast<long long>(key) +
        static_cast<long long>(summand)) % modulo_) % limiter;
}

std::mt19937 HashGenerator::generator_ = std::mt19937(0);

HashGenerator& HashGenerator::getInstance() {
  static HashGenerator hash_generator;
  return hash_generator;
}

Hash HashGenerator::generate_random_hash(size_t limiter) const {
  Hash hash = {
    std::uniform_int_distribution<size_t>(0, Hash::modulo_ - 1)(generator_),
    std::uniform_int_distribution<size_t>(1, Hash::modulo_ - 1)(generator_),
    limiter
  };

  return hash;
}

Bucket::BucketElement::BucketElement() : number(0), is_empty(true) {};
Bucket::BucketElement::BucketElement(int number, bool is_empty)
    : number(number),
      is_empty(is_empty) {};

Bucket::Bucket() {
  capacity_ = 0;
}

Bucket::Bucket(size_t capacity)
    : capacity_(capacity),
      elements_(capacity, BucketElement {0, true}),
      hash_(HashGenerator::getInstance().generate_random_hash(capacity)) {}

bool Bucket::check_if_need_rehashing(const std::vector<int>& numbers) const {
  std::vector<char> elements_filling(capacity_, 0);

  for (int number : numbers) {
    size_t key = hash_(number);
    if (elements_filling[key] == 1) {
      return true;
    }

    elements_filling[key] = 1;
  }

  return false;
}

void Bucket::set_hash(const std::vector<int>& numbers) {
  bool need_rehashing = check_if_need_rehashing(numbers);

  while (need_rehashing) {
    hash_ = HashGenerator::getInstance().generate_random_hash(
        capacity_);
    need_rehashing = check_if_need_rehashing(numbers);
  }
}

bool Bucket::Contains(int number) const {
  if (capacity_ == 0) {
    return false;
  }

  size_t in_bucket_index = hash_(number);
  if (elements_[in_bucket_index].is_empty ||
      elements_[in_bucket_index].number != number) {
    return false;
  }

  return true;
}

void Bucket::Initialize(const std::vector<int>& numbers) {
  set_hash(numbers);

  for (int number : numbers) {
    elements_[hash_(number)] = {number, false};
  }
}

FixedSet::FixedSet() {
  number_of_buckets_ = 0;
}

void FixedSet::Initialize(const std::vector<int>& numbers) {
  number_of_buckets_ = numbers.size() * number_of_buckets_factor_;

  buckets_.clear();
  buckets_.shrink_to_fit();
  buckets_.reserve(number_of_buckets_);

  std::vector<std::vector<int>> buckets_numbers = set_hash(numbers);

  for (size_t bucket_index = 0; bucket_index < number_of_buckets_; ++bucket_index) {
    size_t bucket_size = buckets_numbers[bucket_index].size();

    Bucket bucket(bucket_size * bucket_size);
    bucket.Initialize(buckets_numbers[bucket_index]);

    buckets_.push_back(bucket);
  }
}

bool FixedSet::Contains(int number) const {
  size_t bucket_index = hash_(number);

  return buckets_[bucket_index].Contains(number);
}

std::vector<std::vector<int>> FixedSet::set_hash(const std::vector<int>& numbers) {
  std::vector<std::vector<int>> buckets_numbers(number_of_buckets_);
  bool need_rehashing = true;

  while (need_rehashing) {
    hash_ = HashGenerator::getInstance().generate_random_hash(
        number_of_buckets_);

    buckets_numbers.assign(number_of_buckets_, {});
    for (int number : numbers) {
      size_t bucket_index = hash_(number);
      buckets_numbers[bucket_index].push_back(number);
    }

    unsigned long long buckets_squared_total_size = 0;
    for (size_t bucket_index = 0; bucket_index < number_of_buckets_; ++bucket_index) {
      size_t bucket_size = buckets_numbers[bucket_index].size();
      buckets_squared_total_size += static_cast<long long>(bucket_size) *
        static_cast<long long>(bucket_size);
    }

    need_rehashing = buckets_squared_total_size > (static_cast<long long>(number_of_buckets_) *
        total_buckets_size_factor_);
  }

  return buckets_numbers;
}

std::vector<char> check_existance_queries_in_numbers(const std::vector<int>& numbers,
    const std::vector<int>& queries) {
  std::vector<char> answer;
  answer.reserve(queries.size());

  FixedSet fixed_set = FixedSet();
  fixed_set.Initialize(numbers);

  for (int query : queries) {
    answer.push_back(fixed_set.Contains(query));
  }

  return answer;
}

size_t read_number(std::istream& istream) {
  size_t number;
  istream >> number;
  return number;
}

std::vector<int> read_vector(std::istream& istream,
    size_t length) {
  std::vector<int> numbers;
  numbers.reserve(length);

  for (size_t i = 0; i < length; ++i) {
    int number;
    istream >> number;
    numbers.push_back(number);
  }

  return numbers;
}

void write_existance_of_queries_in_numbers(std::ostream& ostream,
    const std::vector<char>& queries_existance) {
  for (bool query_exists : queries_existance) {
    if (query_exists) {
      ostream << "Yes" << "\n";
    } else {
      ostream << "No" << "\n";
    }
  }
}

int main() {
  std::ios_base::sync_with_stdio(false);
  std::cin.tie(nullptr);

  size_t amount_of_numbers = read_number(std::cin);
  std::vector<int> numbers = read_vector(std::cin, amount_of_numbers);

  size_t amount_of_queries = read_number(std::cin);
  std::vector<int> queries = read_vector(std::cin, amount_of_queries);

  std::vector<char> queries_existance = check_existance_queries_in_numbers(numbers, queries);

  write_existance_of_queries_in_numbers(std::cout, queries_existance);

  return 0;
}
