#include <algorithm>
#include <cstring>
#include <deque>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_set>
#include <set>
#include <vector>

template <class Iterator>
class IteratorRange {
 public:
  IteratorRange(Iterator begin, Iterator end) : begin_(begin), end_(end) {}

  Iterator begin() const { return begin_; }
  Iterator end() const { return end_; }

 private:
  Iterator begin_, end_;
};

namespace traverses {

template <class Vertex, class Graph, class Visitor>
void BreadthFirstSearch(Vertex origin_vertex, const Graph& graph,
                        Visitor visitor);

template <class Vertex, class Edge>
class BfsVisitor {
 public:
  virtual void DiscoverVertex(Vertex /* vertex */) {}
  virtual void ExamineEdge(const Edge&  /* edge */) {}
  virtual void ExamineVertex(Vertex /* vertex */) {}
  virtual ~BfsVisitor() = default;
};

}  // namespace traverses

namespace aho_corasick {

struct AutomatonNode {
  AutomatonNode() : suffix_link(nullptr), terminal_link(nullptr) {}

  // Stores ids of strings which are ended at this node.
  std::vector<size_t> terminated_string_ids;
  // Stores tree structure of nodes.
  std::map<char, AutomatonNode> trie_transitions;

  // Stores cached transitions of the automaton, contains
  // only pointers to the elements of trie_transitions.
  std::map<char, AutomatonNode*> automaton_transitions_cache;
  AutomatonNode* suffix_link;
  AutomatonNode* terminal_link;
};

// Returns a corresponding trie transition 'nullptr' otherwise.
AutomatonNode* GetTrieTransition(AutomatonNode* node, char character);

// Returns an automaton transition, updates 'node->automaton_transitions_cache'
// if necessary.
// Provides constant amortized runtime.
AutomatonNode* GetAutomatonTransition(AutomatonNode* node,
                                      const AutomatonNode* root,
                                      char character);

namespace internal {

class AutomatonGraph {
 public:
  struct Edge {
    Edge(AutomatonNode* source, AutomatonNode* target, char character)
        : source(source), target(target), character(character) {}

    AutomatonNode* source;
    AutomatonNode* target;
    char character;
  };
};

std::vector<typename AutomatonGraph::Edge> OutgoingEdges(
    const AutomatonGraph&  /* graph */, AutomatonNode* vertex);

AutomatonNode* GetTarget(const AutomatonGraph&  /* graph */,
                         const AutomatonGraph::Edge& edge);

class SuffixLinkCalculator
    : public traverses::BfsVisitor<AutomatonNode* , AutomatonGraph::Edge> {
 public:
  explicit SuffixLinkCalculator(AutomatonNode* root) : root_(root) {}

  void ExamineVertex(AutomatonNode* node) override;

  void ExamineEdge(const AutomatonGraph::Edge& edge) override;

 private:
  AutomatonNode* root_;
};

class TerminalLinkCalculator
    : public traverses::BfsVisitor<AutomatonNode* , AutomatonGraph::Edge> {
 public:
  explicit TerminalLinkCalculator(AutomatonNode* root) : root_(root) {}

  void DiscoverVertex(AutomatonNode* node) override;

 private:
  AutomatonNode* root_;
};

}  // namespace internal

class NodeReference {
 public:
  NodeReference() : node_(nullptr), root_(nullptr) {}

  NodeReference(AutomatonNode* node, AutomatonNode* root)
      : node_(node), root_(root) {}

  NodeReference Next(char character) const {
    return NodeReference(GetAutomatonTransition(node_, root_, character), root_);
  }

  template <class Callback>
  void GenerateMatches(Callback on_match) const;

  bool IsTerminal() const {
    return node_->terminated_string_ids.size() != 0;
  }

  bool IsRoot() const {
    return node_ == root_;
  }

  explicit operator bool() const { return node_ != nullptr; }

  bool operator==(NodeReference other) const {
    return node_ == other.node_;
  }

 private:
  typedef std::vector<size_t>::const_iterator TerminatedStringIterator;
  typedef IteratorRange<TerminatedStringIterator> TerminatedStringIteratorRange;

  NodeReference TerminalLink() const {
    return NodeReference(node_->terminal_link, root_);
  }

  TerminatedStringIteratorRange TerminatedStringIds() const {
    return {node_->terminated_string_ids.begin(), node_->terminated_string_ids.end()};
  }

  AutomatonNode* node_;
  AutomatonNode* root_;
};

class AutomatonBuilder;

class Automaton {
 public:
  Automaton() = default;

  Automaton(const Automaton& ) = delete;
  Automaton& operator=(const Automaton& ) = delete;

  NodeReference Root() {
    return NodeReference(&root_, &root_);
  }

 private:
  AutomatonNode root_;

  friend class AutomatonBuilder;
};

class AutomatonBuilder {
 public:
  void Add(const std::string& string, size_t id) {
    words_.push_back(string);
    ids_.push_back(id);
  }

  std::unique_ptr<Automaton> Build() {
    auto automaton = std::make_unique<Automaton>();

    BuildTrie(words_, ids_, automaton.get());
    BuildSuffixLinks(automaton.get());
    BuildTerminalLinks(automaton.get());

    return automaton;
  }

 private:
  static void BuildTrie(const std::vector<std::string>& words,
                        const std::vector<size_t>& ids, Automaton* automaton) {
    for (size_t i = 0; i < words.size(); ++i) {
      AddString(&automaton->root_, ids[i], words[i]);
    }
  }

  static void AddString(AutomatonNode* root, size_t string_id,
                        const std::string& string);

  static void BuildSuffixLinks(Automaton* automaton) {
    BreadthFirstSearch(&automaton->root_, aho_corasick::internal::AutomatonGraph(),
                       aho_corasick::internal::SuffixLinkCalculator(&automaton->root_));
  }

  static void BuildTerminalLinks(Automaton* automaton) {
    BreadthFirstSearch(&automaton->root_, aho_corasick::internal::AutomatonGraph(),
                       aho_corasick::internal::TerminalLinkCalculator(&automaton->root_));
  }

  std::vector<std::string> words_;
  std::vector<size_t> ids_;
};

}  // namespace aho_corasick

// Consecutive delimiters are not grouped together and are deemed
// to delimit empty strings
template <class Predicate>
std::vector<std::string> Split(const std::string& string,
                               Predicate is_delimiter);

// Wildcard is a character that may be substituted
// for any of all possible characters.
class WildcardMatcher {
 public:
  WildcardMatcher() : number_of_words_(0), pattern_length_(0) {}

  WildcardMatcher static BuildFor(const std::string& pattern, char wildcard);

  // Resets the matcher. Call allows to abandon all data which was already
  // scanned,
  // a new stream can be scanned afterwards.
  void Reset();

  template <class Callback>
  void Scan(char character, Callback on_match);

 private:
  void UpdateWordOccurrences();

  void ShiftWordOccurrencesCounters();

  // Storing only O(|pattern|) elements allows us
  // to consume only O(|pattern|) memory for matcher.
  std::deque<size_t> words_occurrences_by_position_;
  aho_corasick::NodeReference state_;
  size_t number_of_words_;
  size_t pattern_length_;
  std::unique_ptr<aho_corasick::Automaton> aho_corasick_automaton_;
};

std::string ReadString(std::istream& input_stream);

// Returns positions of the first character of an every match.
std::vector<size_t> FindFuzzyMatches(const std::string& pattern_with_wildcards,
                                     const std::string& text, char wildcard);

void Print(const std::vector<size_t>& sequence);

void TestAll();

int main() {
  const char wildcard = '?';
  const std::string pattern_with_wildcards = ReadString(std::cin);
  const std::string text = ReadString(std::cin);

  Print(FindFuzzyMatches(pattern_with_wildcards, text, wildcard));

  return 0;
}

// === Realization ===

template <class Vertex, class Graph, class Visitor>
void traverses::BreadthFirstSearch(Vertex origin_vertex, const Graph& graph,
                        Visitor visitor) {

  std::unordered_set<Vertex> discovered_vertices;

  std::queue<Vertex> vertices_to_process;
  vertices_to_process.push(origin_vertex);

  visitor.DiscoverVertex(origin_vertex);
  discovered_vertices.insert(origin_vertex);

  while (!vertices_to_process.empty()) {
    const Vertex examined_vertex = vertices_to_process.front();
    vertices_to_process.pop();

    visitor.ExamineVertex(examined_vertex);
    discovered_vertices.insert(examined_vertex);

    for (const auto& examined_edge : OutgoingEdges(graph, examined_vertex)) {
      visitor.ExamineEdge(examined_edge);

      const Vertex child_vertex = GetTarget(graph, examined_edge);
      if (discovered_vertices.find(child_vertex) ==
          discovered_vertices.end()) {
        vertices_to_process.push(child_vertex);
        visitor.DiscoverVertex(child_vertex);
      }
    }
  }
}

aho_corasick::AutomatonNode* aho_corasick::GetTrieTransition(
    AutomatonNode* node, char character) {
  auto target_node = node->trie_transitions.find(character);
  if (target_node != node->trie_transitions.end()) {
    return &(target_node->second);
  }

  return nullptr;
}

// Returns an automaton transition, updates 'node->automaton_transitions_cache'
// if necessary.
// Provides constant amortized runtime.
aho_corasick::AutomatonNode* aho_corasick::GetAutomatonTransition(
    AutomatonNode* node, const AutomatonNode* root, char character) {
  auto target_cached_node = node->automaton_transitions_cache.emplace(character, nullptr);
  auto& cached_target = target_cached_node.first->second;

  if (!target_cached_node.second) {
    return cached_target;
  }

  auto target_node = node->trie_transitions.find(character);
  if (target_node != node->trie_transitions.end()) {
    return cached_target = &target_node->second;
  }

  if (node == root) {
    return cached_target = node;
  }

  auto automaton_transition = GetAutomatonTransition(node->suffix_link,
                                                     root, character);
  return cached_target = automaton_transition;
}

std::vector<typename aho_corasick::internal::AutomatonGraph::Edge>
aho_corasick::internal::OutgoingEdges(const AutomatonGraph& /* graph */,
    AutomatonNode* vertex) {
  std::vector<typename AutomatonGraph::Edge> edges;
  edges.reserve(vertex->trie_transitions.size());

  for (auto& transition : vertex->trie_transitions) {
    edges.emplace_back(vertex, &transition.second, transition.first);
  }

  return edges;
}

aho_corasick::AutomatonNode* aho_corasick::internal::GetTarget(
    const AutomatonGraph& /* graph */, const AutomatonGraph::Edge& edge) {
  return edge.target;
}

void aho_corasick::internal::SuffixLinkCalculator::ExamineVertex(
    AutomatonNode* node) {
  if (node == root_) {
    node->suffix_link = node;
  }
}

void aho_corasick::internal::SuffixLinkCalculator::ExamineEdge(
    const AutomatonGraph::Edge& edge) {
  AutomatonNode* origin = edge.target;
  AutomatonNode* parent = edge.source;

  if (parent->suffix_link == parent) {
    origin->suffix_link = parent;
    return;
  }

  origin->suffix_link = GetAutomatonTransition(parent->suffix_link,
                                               root_, edge.character);
}

void aho_corasick::internal::TerminalLinkCalculator::DiscoverVertex(
    AutomatonNode* node) {
  AutomatonNode* suffix_parent = node->suffix_link;

  if (node == suffix_parent) {
    return;
  }

  if (!suffix_parent->terminated_string_ids.empty()) {
    node->terminal_link = suffix_parent;
  } else {
    node->terminal_link = suffix_parent->terminal_link;
  }
}

template <class Callback>
void aho_corasick::NodeReference::GenerateMatches(Callback on_match) const {
  NodeReference current_node = *this;

  do {
    for (int id : current_node.TerminatedStringIds()) {
      on_match(id);
    }

    current_node = current_node.TerminalLink();
  } while (current_node);
}

void aho_corasick::AutomatonBuilder::AddString(AutomatonNode* root,
    size_t string_id, const std::string& string) {
  size_t position = 0;
  auto last_node = root;

  for (char label : string) {
    last_node = &last_node->trie_transitions[label];
    ++position;
  }

  last_node->terminated_string_ids.push_back(string_id);
}

template <class Predicate>
std::vector<std::string> Split(const std::string& string,
    Predicate is_delimiter) {
  std::vector<std::string> splitted_strings;
  splitted_strings.push_back("");

  for (char symbol : string) {
    if (is_delimiter(symbol)) {
      splitted_strings.push_back("");
    } else {
      splitted_strings.back().push_back(symbol);
    }
  }

  return splitted_strings;
}

WildcardMatcher WildcardMatcher::BuildFor(const std::string& pattern, char wildcard) {
  WildcardMatcher wmatcher;
  wmatcher.pattern_length_ = pattern.size();

  aho_corasick::AutomatonBuilder abuilder;
  const auto words = Split(pattern, [wildcard] (char x) { return x == wildcard; });

  wmatcher.number_of_words_ = words.size();

  size_t string_id = 0;
  for (const auto& word : words) {
    string_id += word.size();
    abuilder.Add(word, string_id);
    ++string_id;
  }

  wmatcher.aho_corasick_automaton_ = abuilder.Build();
  wmatcher.Reset();

  return wmatcher;
}

void WildcardMatcher::Reset() {
  state_ = aho_corasick_automaton_->Root();
  words_occurrences_by_position_.assign(pattern_length_ + 1, 0);
  UpdateWordOccurrences();
  ShiftWordOccurrencesCounters();
}

template <class Callback>
void WildcardMatcher::Scan(char character, Callback on_match) {
  state_ = state_.Next(character);
  UpdateWordOccurrences();

  if (words_occurrences_by_position_[0] == number_of_words_) {
    on_match();
  }

  ShiftWordOccurrencesCounters();
}

void WildcardMatcher::UpdateWordOccurrences() {
  state_.GenerateMatches([this](size_t id) {
        ++words_occurrences_by_position_[this->pattern_length_ - id];
      });
}

void WildcardMatcher::ShiftWordOccurrencesCounters() {
  words_occurrences_by_position_.push_back(0);
  words_occurrences_by_position_.pop_front();
}

std::string ReadString(std::istream& input_stream) {
  std::string in_string;
  input_stream >> in_string;

  return in_string;
}

void Print(const std::vector<size_t>& sequence) {
  std::cout << sequence.size() << "\n";

  for (const auto& element : sequence) {
    std::cout << element << " ";
  }

  std::cout << "\n";
}

std::vector<size_t> FindFuzzyMatches(const std::string& pattern_with_wildcards,
    const std::string& text, char wildcard) {
  std::vector<size_t> occurrences;
  const size_t pattern_length = pattern_with_wildcards.size();

  WildcardMatcher wmatcher = WildcardMatcher::BuildFor(pattern_with_wildcards, wildcard);
  for (size_t i = 0; i < text.size(); ++i) {
    wmatcher.Scan(
        text[i],
        [&occurrences, i, pattern_length]() {
          occurrences.push_back(i + 1 - pattern_length);
        });
  }

  return occurrences;
}
