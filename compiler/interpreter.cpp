#include "interpreter.hpp"

#include "debug_output.hpp"

#include <algorithm>
#include <charconv>
#include <map>
#include <set>
#include <span>
#include <iostream>
#include <sstream>

namespace aoc2022 {
namespace {

std::string StrCat(const auto&... args) {
  std::ostringstream output;
  (output << ... << args);
  return output.str();
}

struct Node {
  virtual ~Node() = default;

  void Mark(std::vector<Node*>& frontier) {
    if (reachable) return;
    reachable = true;
    AddChildren(frontier);
  }

  virtual void AddChildren(std::vector<Node*>& frontier) = 0;

  bool reachable = false;
};

struct Value;
struct Interpreter;

struct GCPtrBase {
  virtual void Mark(std::vector<Node*>& frontier) = 0;
  GCPtrBase* prev;
  GCPtrBase* next;
};

template <std::derived_from<Node> T>
class GCPtr : public GCPtrBase {
 public:
  GCPtr() = default;
  GCPtr(std::nullptr_t) : GCPtr() {}
  GCPtr(Interpreter& interpreter, T* value);
  template <typename U = T>
  requires std::convertible_to<U*, T*>
  GCPtr(const GCPtr<U>& other) : GCPtr(*other.interpreter_, other.value_) {}
  GCPtr(const GCPtr& other) : GCPtr(*other.interpreter_, other.value_) {}
  GCPtr& operator=(const GCPtr& other);

  GCPtr& operator=(T* value) {
    value_ = value;
    return *this;
  }

  void Mark(std::vector<Node*>& frontier) override {
    if (value_) value_->Mark(frontier);
  }

  ~GCPtr();

  T* get() const { return value_; }
  T& operator*() const { return *value_; }
  T* operator->() const { return value_; }

  operator T*() const { return value_; }
  explicit operator bool() const { return value_ != nullptr; }

 private:
  template <std::derived_from<Node> U>
  friend class GCPtr;

  Interpreter* interpreter_ = nullptr;
  T* value_ = nullptr;
};

class Lazy;
class Union;

struct Value : public Node {
  enum class Type {
    kInt64,
    kChar,
    kLambda,
    kTuple,
    kUnion,
  };

  virtual Type GetType() const = 0;

  bool AsBool() const;
  std::int64_t AsInt64() const;
  char AsChar() const;
  std::span<Lazy* const> AsTuple() const;
  const Union& AsUnion() const;
  void Enter(Interpreter&);
};

struct Thunk : Node {
  virtual GCPtr<Value> Run(Interpreter& interpreter) = 0;
};

class Lazy final : public Node {
 public:
  Lazy(Value* value) : has_value_(true), value_(value) {}
  Lazy(Thunk* thunk) : has_value_(false), thunk_(thunk) {}
  GCPtr<Value> Get(Interpreter& interpreter) {
    if (!has_value_) {
      // Evaluation of the thunk relies on evaluating itself: the expression
      // diverges without reaching weak head normal form.
      if (computing_) throw std::runtime_error("divergence");
      computing_ = true;
      value_ = thunk_->Run(interpreter);
      has_value_ = true;
      computing_ = false;
    }
    return GCPtr<Value>(interpreter, value_);
  }
  void AddChildren(std::vector<Node*>& frontier) override;
 private:
  bool has_value_;
  bool computing_ = false;
  union {
    Value* value_;
    Thunk* thunk_;
  };
};

template <typename K>
class flat_set {
 public:
  flat_set() = default;

  flat_set(std::initializer_list<K> contents)
      : flat_set(std::vector(contents)) {}

  flat_set(std::vector<K> contents)
      : contents_(std::move(contents)) {
    std::sort(contents_.begin(), contents_.end());
  }

  bool contains(const K& k) const {
    return !contents_.empty() && contents_[Index(k)] == k;
  }

  template <typename... Args>
  requires std::constructible_from<K, Args...>
  std::pair<typename std::vector<K>::const_iterator, bool>
  emplace(Args&&... args) {
    if (contents_.empty()) {
      return std::pair(
          contents_.emplace(contents_.end(), std::forward<Args>(args)...),
          true);
    }
    K k(std::forward<Args>(args)...);
    const int i = Index(k);
    if (contents_[i] == k) return std::pair(contents_.begin() + i, false);
    return std::pair(contents_.emplace(contents_.begin() + i + 1, std::move(k)),
                     true);
  }

  std::pair<typename std::vector<K>::const_iterator, bool> insert(const K& k) {
    return emplace(k);
  }

  void erase(const K& k) {
    if (contents_.empty()) return;
    const int i = Index(k);
    if (contents_[i] == k) contents_.erase(contents_.begin() + i);
  }

  bool empty() const { return contents_.empty(); }
  auto size() const { return contents_.size(); }
  auto begin() const { return contents_.begin(); }
  auto end() const { return contents_.end(); }

 private:
  int Index(const K& k) const {
    int a = 0, b = contents_.size();
    while (b - a > 1) {
      const int i = (a + b) / 2;
      if (k < contents_[i]) {
        b = i;
      } else {
        a = i;
      }
    }
    return a;
  }

  std::vector<K> contents_;
};

template <typename K, typename V>
class flat_map {
 public:
  V& operator[](const K& k) {
    if (contents_.empty()) {
      auto& entry = contents_.emplace_back(k, V());
      return entry.second;
    }
    const int i = Index(k);
    if (contents_[i].first == k) return contents_[i].second;
    auto entry = contents_.emplace(contents_.begin() + i + 1, k, V());
    return entry->second;
  }

  const V& at(const K& k) const {
    if (contents_.empty()) throw std::runtime_error("not found");
    const int i = Index(k);
    if (contents_[i].first != k) throw std::runtime_error("not found");
    return contents_[i].second;
  }

  bool empty() const { return contents_.empty(); }
  auto size() const { return contents_.size(); }
  auto begin() { return contents_.begin(); }
  auto end() { return contents_.end(); }
  auto begin() const { return contents_.begin(); }
  auto end() const { return contents_.end(); }

 private:
  int Index(const K& k) const {
    int a = 0, b = contents_.size();
    while (b - a > 1) {
      const int i = (a + b) / 2;
      if (k < contents_[i].first) {
        b = i;
      } else {
        a = i;
      }
    }
    return a;
  }

  std::vector<std::pair<K, V>> contents_;
};

struct Interpreter {
  template <std::derived_from<Node> T, typename... Args>
  requires std::constructible_from<T, Args...>
  GCPtr<T> Allocate(Args&&... args);

  void AddPtr(GCPtrBase* p) {
    if (live == nullptr) {
      live = p;
      p->next = p;
      p->prev = p;
    } else {
      p->prev = live;
      p->next = live->next;
      p->next->prev = p;
      p->prev->next = p;
    }
  }

  void RemovePtr(GCPtrBase* p) {
    if (p->next == p) {
      live = nullptr;
    } else {
      live = p->prev;
      p->next->prev = p->prev;
      p->prev->next = p->next;
    }
  }

  GCPtr<Value> Nil();
  GCPtr<Value> Cons(Lazy* head, Lazy* tail);
  GCPtr<Value> Bool(bool value);
  std::string EvaluateString(GCPtr<Lazy> list);

  void CollectGarbage();

  using Captures = flat_map<core::Identifier, Lazy*>;
  void ResolveImpl(flat_set<core::Identifier>&, Captures&,
                   const core::Builtin& x);
  void ResolveImpl(flat_set<core::Identifier>&, Captures&,
                   const core::Identifier& x);
  void ResolveImpl(flat_set<core::Identifier>&, Captures&,
                   const core::Integer& x);
  void ResolveImpl(flat_set<core::Identifier>&, Captures&,
                   const core::Character& x);
  void ResolveImpl(flat_set<core::Identifier>&, Captures&,
                   const core::Tuple& x);
  void ResolveImpl(flat_set<core::Identifier>&, Captures&,
                   const core::UnionConstructor& x);
  void ResolveImpl(flat_set<core::Identifier>&, Captures&,
                   const core::Apply& x);
  void ResolveImpl(flat_set<core::Identifier>&, Captures&,
                   const core::Lambda& x);
  void ResolveImpl(flat_set<core::Identifier>&, Captures&, const core::Let& x);
  void ResolveImpl(flat_set<core::Identifier>&, Captures&,
                   const core::LetRecursive& x);
  void ResolveImpl(flat_set<core::Identifier>&, Captures&,
                   const core::Case::Alternative& x);
  void ResolveImpl(flat_set<core::Identifier>&, Captures&, const core::Case& x);
  void Resolve(flat_set<core::Identifier>& bound, Captures&,
               const core::Expression& x);
  Captures Resolve(const core::Expression& x);

  flat_set<core::Identifier> GetBindingsImpl(const core::Identifier&);
  flat_set<core::Identifier> GetBindingsImpl(const core::MatchTuple&);
  flat_set<core::Identifier> GetBindingsImpl(const core::MatchUnion&);
  flat_set<core::Identifier> GetBindingsImpl(const core::Integer&);
  flat_set<core::Identifier> GetBindingsImpl(const core::Character&);
  flat_set<core::Identifier> GetBindings(const core::Pattern&);

  GCPtr<Value> Evaluate(const core::Builtin& x);
  GCPtr<Value> Evaluate(const core::Identifier& x);
  GCPtr<Value> Evaluate(const core::Integer& x);
  GCPtr<Value> Evaluate(const core::Character& x);
  GCPtr<Value> Evaluate(const core::Tuple& x);
  GCPtr<Value> Evaluate(const core::UnionConstructor& x);
  GCPtr<Value> Evaluate(const core::Apply& x);
  GCPtr<Value> Evaluate(const core::Lambda& x);
  GCPtr<Value> Evaluate(const core::Let& x);
  GCPtr<Value> Evaluate(const core::LetRecursive& x);
  GCPtr<Value> Evaluate(const core::Case& x);
  GCPtr<Value> Evaluate(const core::Expression& x);

  GCPtr<Lazy> LazyEvaluate(const core::Builtin& x);
  GCPtr<Lazy> LazyEvaluate(const core::Identifier& x);
  GCPtr<Lazy> LazyEvaluate(const core::Integer& x);
  GCPtr<Lazy> LazyEvaluate(const core::Character& x);
  GCPtr<Lazy> LazyEvaluate(const core::Tuple& x);
  GCPtr<Lazy> LazyEvaluate(const core::UnionConstructor& x);
  GCPtr<Lazy> LazyEvaluate(const core::Apply& x);
  GCPtr<Lazy> LazyEvaluate(const core::Lambda& x);
  GCPtr<Lazy> LazyEvaluate(const core::Let& x);
  GCPtr<Lazy> LazyEvaluate(const core::LetRecursive& x);
  GCPtr<Lazy> LazyEvaluate(const core::Case& x);
  GCPtr<Lazy> LazyEvaluate(const core::Expression& x);

  GCPtr<Value> TryAlternative(Value*, const core::Case::Alternative& x);
  GCPtr<Value> TryAlternative(Value*, const core::Identifier&,
                              const core::Expression& x);
  GCPtr<Value> TryAlternative(Value*, const core::MatchTuple&,
                              const core::Expression& x);
  GCPtr<Value> TryAlternative(Value*, const core::MatchUnion&,
                              const core::Expression& x);
  GCPtr<Value> TryAlternative(Value*, const core::Integer&,
                              const core::Expression& x);
  GCPtr<Value> TryAlternative(Value*, const core::Character&,
                              const core::Expression& x);

  void Run(const core::Expression& program);

  std::vector<std::unique_ptr<Node>> heap;
  int collect_at_size = 128;
  std::map<core::Identifier, std::vector<Lazy*>> names;
  std::vector<Lazy*> stack;
  GCPtrBase* live = nullptr;
};

template <std::derived_from<Node> T>
GCPtr<T>::GCPtr(Interpreter& interpreter, T* value)
    : interpreter_(&interpreter), value_(value) {
  interpreter_->AddPtr(this);
}

template <std::derived_from<Node> T>
GCPtr<T>& GCPtr<T>::operator=(const GCPtr& other) {
  if (interpreter_ != other.interpreter_) {
    if (interpreter_) interpreter_->RemovePtr(this);
    interpreter_ = other.interpreter_;
    if (interpreter_) interpreter_->AddPtr(this);
  }
  value_ = other.value_;
  return *this;
}

template <std::derived_from<Node> T>
GCPtr<T>::~GCPtr() {
  if (interpreter_) interpreter_->RemovePtr(this);
}

const char* Name(Value::Type t) {
  switch (t) {
    case Value::Type::kInt64:
      return "int64";
    case Value::Type::kChar:
      return "char";
    case Value::Type::kLambda:
      return "lambda";
    case Value::Type::kTuple:
      return "tuple";
    case Value::Type::kUnion:
      return "union";
  }
  std::abort();
}

struct Int64 final : public Value {
  Int64(std::int64_t value) : value(value) {}
  Type GetType() const override { return Type::kInt64; }
  void AddChildren(std::vector<Node*>& frontier) override {}
  std::int64_t value;
};

struct Char final : public Value {
  Char(char value) : value(value) {}
  Type GetType() const override { return Type::kChar; }
  void AddChildren(std::vector<Node*>& frontier) override {}
  char value;
};

struct Tuple final : public Value {
  Type GetType() const override { return Type::kTuple; }
  void AddChildren(std::vector<Node*>& frontier) override {
    frontier.insert(frontier.end(), elements.begin(), elements.end());
  }
  std::vector<Lazy*> elements;
};

struct Union final : public Value {
  Union(core::UnionType::Id type_id, int index,
        std::span<Lazy* const> elements = {})
      : type_id(type_id),
        index(index),
        elements(elements.begin(), elements.end()) {}
  Type GetType() const override { return Type::kUnion; }
  void AddChildren(std::vector<Node*>& frontier) override {
    frontier.insert(frontier.end(), elements.begin(), elements.end());
  }
  core::UnionType::Id type_id;
  int index;
  std::vector<Lazy*> elements;
};

struct Closure : public Thunk {
  Closure(Interpreter::Captures captures) : captures(std::move(captures)) {}
  void AddChildren(std::vector<Node*>& frontier) override {
    for (const auto& [id, value] : captures) frontier.push_back(value);
  }
  virtual GCPtr<Value> RunBody(Interpreter&) = 0;
  GCPtr<Value> Run(Interpreter& interpreter) final {
    for (const auto& [id, value] : captures) {
      interpreter.names[id].push_back(value);
    }
    GCPtr<Value> result = RunBody(interpreter);
    for (const auto& [id, value] : captures) {
      interpreter.names[id].pop_back();
    }
    return result;
  }
  Interpreter::Captures captures;
};

struct Let final : public Closure {
  Let(Interpreter& interpreter, const core::Let& definition)
      : Closure(interpreter.Resolve(definition)),
        definition(definition) {}
  GCPtr<Value> RunBody(Interpreter& interpreter) override {
    interpreter.names[definition.binding.name].push_back(
        interpreter.LazyEvaluate(definition.binding.result));
    GCPtr<Value> result = interpreter.Evaluate(definition.result);
    interpreter.names[definition.binding.name].pop_back();
    return result;
  }
  const core::Let& definition;
};

struct Error final : public Thunk {
  Error(std::string message) : message(std::move(message)) {}
  void AddChildren(std::vector<Node*>&) override {}
  GCPtr<Value> Run(Interpreter&) override {
    throw std::runtime_error(message);
  }
  std::string message;
};

struct LetRecursive final : public Closure {
  LetRecursive(Interpreter& interpreter, const core::LetRecursive& definition)
      : Closure(interpreter.Resolve(definition)), definition(definition) {}
  GCPtr<Value> RunBody(Interpreter& interpreter) override {
    std::vector<Lazy*> holes;
    for (const auto& [id, value] : definition.bindings) {
      GCPtr<Lazy> l = interpreter.Allocate<Lazy>(
          interpreter.Allocate<Error>("this should never be executed"));
      interpreter.names[id].push_back(l);
      holes.push_back(l);
    }
    for (int i = 0, n = definition.bindings.size(); i < n; i++) {
      // There are two possible cases for the return value here.
      //
      //   * The return value is the value itself, which is currently just
      //     a hole. In this case, the expression has no weak head normal form:
      //     it diverges, so we replace it with an error.
      //   * The return value is *not* the value itself. In this case, we will
      //     overwrite the hole with the thunk for the actual value. This may
      //     refer to the value itself internally, at which point it will
      //     evaluate as the newly-assigned value.
      GCPtr<Lazy> value =
          interpreter.LazyEvaluate(definition.bindings[i].result);
      if (holes[i] == value) {
        *holes[i] = Lazy(interpreter.Allocate<Error>("divergence"));
      } else {
        *holes[i] = *value;
      }
    }
    GCPtr<Value> result = interpreter.Evaluate(definition.result);
    for (const auto& [id, value] : definition.bindings) {
      interpreter.names[id].pop_back();
    }
    return result;
  }
  const core::LetRecursive& definition;
};

struct Case final : public Closure {
  Case(Interpreter& interpreter, const core::Case& definition)
      : Closure(interpreter.Resolve(definition)), definition(definition) {}
  GCPtr<Value> RunBody(Interpreter& interpreter) override {
    std::vector<Lazy*> values;
    GCPtr<Value> v = interpreter.Evaluate(definition.value);
    for (const auto& alternative : definition.alternatives) {
      if (GCPtr<Value> r = interpreter.TryAlternative(v, alternative)) {
        return r;
      }
    }
    throw std::runtime_error(StrCat("non-exhaustative case: nothing to match ",
                                    Name(v->GetType()),
                                    ". core: ", definition));
  }
  const core::Case& definition;
};

struct Lambda : public Value {
  Type GetType() const final { return Type::kLambda; };
  virtual void Enter(Interpreter& interpreter) = 0;
};

struct NativeFunctionBase : public Node {
  NativeFunctionBase(int arity) : arity(arity) {}
  virtual void Enter(Interpreter& interpreter) = 0;
  void AddChildren(std::vector<Node*>&) override {}
  const int arity;
};

struct UnionConstructor final : public NativeFunctionBase {
  UnionConstructor(const core::UnionConstructor& x)
      : NativeFunctionBase(x.type->alternatives.at(x.index).num_members),
        type_id(x.type->id),
        index(x.index) {}
  void Enter(Interpreter& interpreter) override {
    GCPtr<Lazy> value = interpreter.Allocate<Lazy>(interpreter.Allocate<Union>(
        type_id, index, std::span<Lazy*>(interpreter.stack).last(arity)));
    interpreter.stack.resize(interpreter.stack.size() - arity + 1);
    interpreter.stack.back() = value;
  }
  core::UnionType::Id type_id;
  int index;
};

template <int n>
struct NativeFunction : public NativeFunctionBase {
  NativeFunction() : NativeFunctionBase(n) {}
  void Enter(Interpreter& interpreter) final {
    if (interpreter.stack.size() < n) {
      throw std::logic_error("invoking native function with too few arguments");
    }
    std::array<GCPtr<Lazy>, n> args;
    const int m = interpreter.stack.size();
    for (int i = 0; i < n; i++) {
      args[i] = GCPtr<Lazy>(interpreter, interpreter.stack[m - n + i]);
    }
    interpreter.stack.resize(interpreter.stack.size() - n + 1);
    interpreter.stack.back() =
        interpreter.Allocate<Lazy>(Run(interpreter, args));
  }
  virtual GCPtr<Value> Run(Interpreter& interpreter,
                           std::span<GCPtr<Lazy>, n> args) = 0;
};

struct Not : public NativeFunction<1> {
  GCPtr<Value> Run(Interpreter& interpreter,
                   std::span<GCPtr<Lazy>, 1> args) override {
    return interpreter.Bool(!args[0]->Get(interpreter)->AsBool());
  }
};

struct Chr : public NativeFunction<1> {
  GCPtr<Value> Run(Interpreter& interpreter,
                   std::span<GCPtr<Lazy>, 1> args) override {
    const std::int64_t i = args[0]->Get(interpreter)->AsInt64();
    if (0 <= i && i < 128) {
      return interpreter.Allocate<Char>(static_cast<char>(i));
    } else {
      throw std::runtime_error(StrCat("Value ", i, " is out of range for chr"));
    }
  }
};

struct Ord : public NativeFunction<1> {
  GCPtr<Value> Run(Interpreter& interpreter,
                   std::span<GCPtr<Lazy>, 1> args) override {
    const char c = args[0]->Get(interpreter)->AsChar();
    return interpreter.Allocate<Int64>(static_cast<std::int64_t>(c));
  }
};

template <auto F>
struct BinaryOperatorInt64 : public NativeFunction<2> {
  GCPtr<Value> Run(Interpreter& interpreter,
                   std::span<GCPtr<Lazy>, 2> args) override {
    GCPtr<Value> l = args[0]->Get(interpreter);
    GCPtr<Value> r = args[1]->Get(interpreter);
    return interpreter.Allocate<Int64>(F(l->AsInt64(), r->AsInt64()));
  }
};

using Add = BinaryOperatorInt64<[](auto l, auto r) { return l + r; }>;
using Subtract = BinaryOperatorInt64<[](auto l, auto r) { return l - r; }>;
using Multiply = BinaryOperatorInt64<[](auto l, auto r) { return l * r; }>;
using Divide = BinaryOperatorInt64<[](auto l, auto r) { return l / r; }>;
using Modulo = BinaryOperatorInt64<[](auto l, auto r) { return l % r; }>;

struct And : public NativeFunction<2> {
  GCPtr<Value> Run(Interpreter& interpreter,
                   std::span<GCPtr<Lazy>, 2> args) override {
    GCPtr<Value> l = args[0]->Get(interpreter);
    if (!l->AsBool()) return l;
    return args[1]->Get(interpreter);
  }
};

struct Or : public NativeFunction<2> {
  GCPtr<Value> Run(Interpreter& interpreter,
                   std::span<GCPtr<Lazy>, 2> args) override {
    GCPtr<Value> l = args[0]->Get(interpreter);
    if (l->AsBool()) return l;
    return args[1]->Get(interpreter);
  }
};

struct Equal : public NativeFunction<2> {
  bool Run(Interpreter& interpreter, Lazy* lazy_l, Lazy* lazy_r) {
    GCPtr<Value> l = lazy_l->Get(interpreter);
    GCPtr<Value> r = lazy_r->Get(interpreter);
    if (l->GetType() == r->GetType()) {
      switch (l->GetType()) {
        case Value::Type::kChar:
          return l->AsChar() == r->AsChar();
        case Value::Type::kInt64:
          return l->AsInt64() == r->AsInt64();
        case Value::Type::kTuple: {
          const std::span<Lazy* const> elements_l = l->AsTuple();
          const std::span<Lazy* const> elements_r = r->AsTuple();
          if (elements_l.size() != elements_r.size()) {
            throw std::runtime_error("tuple size mismatch in (==)");
          }
          const int n = elements_l.size();
          for (int i = 0; i < n; i++) {
            if (!Run(interpreter, elements_l[i], elements_r[i])) return false;
          }
          return true;
        }
        case Value::Type::kUnion: {
          const Union& union_l = l->AsUnion();
          const Union& union_r = r->AsUnion();
          if (union_l.type_id != union_r.type_id) {
            throw std::runtime_error(
                StrCat("unsupported (==) comparison between ", union_l.type_id,
                       " and ", union_r.type_id));
          }
          if (union_l.index != union_r.index) return false;
          if (union_l.elements.size() != union_r.elements.size()) {
            throw std::logic_error(StrCat(
                "mismatched size for object of type ", union_l.type_id,
                ", constructor ", union_l.index, ": ", union_l.elements.size(),
                " vs ", union_r.elements.size()));
          }
          for (int i = 0, n = union_l.elements.size(); i < n; i++) {
            if (!Run(interpreter, union_l.elements[i], union_r.elements[i])) {
              return false;
            }
          }
          return true;
        }
        default:
          throw std::runtime_error(
              StrCat("unsupported (==) comparison for ", Name(l->GetType())));
      }
    } else {
      throw std::runtime_error(StrCat("unsupported (==) comparison between ",
                                      Name(l->GetType()), " and ",
                                      Name(r->GetType())));
    }
  }
  GCPtr<Value> Run(Interpreter& interpreter,
                   std::span<GCPtr<Lazy>, 2> args) override {
    return interpreter.Bool(Run(interpreter, args[0], args[1]));
  }
};

struct LessThan : public NativeFunction<2> {
  GCPtr<Value> Run(Interpreter& interpreter,
                   std::span<GCPtr<Lazy>, 2> args) override {
    GCPtr<Value> l = args[0]->Get(interpreter);
    GCPtr<Value> r = args[1]->Get(interpreter);
    if (l->GetType() != r->GetType()) {
      throw std::runtime_error(StrCat("unsupported (<) comparison between ",
                                      Name(l->GetType()), " and ",
                                      Name(r->GetType())));
    }
    switch (l->GetType()) {
      case Value::Type::kChar:
        return interpreter.Bool(l->AsChar() < r->AsChar());
      case Value::Type::kInt64:
        return interpreter.Bool(l->AsInt64() < r->AsInt64());
      default:
        throw std::runtime_error(
            StrCat("unsupported (<) comparison for ", Name(l->GetType())));
    }
  }
};

struct ShowInt : public NativeFunction<1> {
  GCPtr<Value> Run(Interpreter& interpreter,
                   std::span<GCPtr<Lazy>, 1> args) override {
    const std::int64_t value = args[0]->Get(interpreter)->AsInt64();
    std::string text = std::to_string(value);
    GCPtr<Value> result = interpreter.Nil();
    for (int i = text.size() - 1; i >= 0; i--) {
      result = interpreter.Cons(
          interpreter.Allocate<Lazy>(interpreter.Allocate<Char>(text[i])),
          interpreter.Allocate<Lazy>(result));
    }
    return result;
  }
};

struct ReadInt : public NativeFunction<1> {
  GCPtr<Value> Run(Interpreter& interpreter,
                   std::span<GCPtr<Lazy>, 1> args) override {
    const std::string text = interpreter.EvaluateString(args[0]);
    std::int64_t value;
    auto [p, error] = std::from_chars(text.data(), text.data() + text.size(),
                                      value);
    if (error != std::errc()) {
      throw std::runtime_error("bad int in string: " + text);
    }
    return interpreter.Allocate<Int64>(value);
  }
};

struct NativeClosure : public Lambda {
  NativeClosure(NativeFunctionBase* f, std::vector<Lazy*> b = {})
      : f(f), bound(std::move(b)) {
    if ((int)bound.size() >= f->arity) {
      throw std::logic_error("creating (over)saturated native closure");
    }
  }
  void AddChildren(std::vector<Node*>& frontier) override {
    frontier.push_back(f);
    frontier.insert(frontier.end(), bound.begin(), bound.end());
  }
  void Enter(Interpreter& interpreter) override {
    const int required = f->arity - bound.size();
    if (required > 1) {
      std::vector<Lazy*> newly_bound = bound;
      newly_bound.push_back(interpreter.stack.back());
      interpreter.stack.back() = interpreter.Allocate<Lazy>(
          interpreter.Allocate<NativeClosure>(f, std::move(newly_bound)));
    } else {
      interpreter.stack.insert(interpreter.stack.end() - 1, bound.begin(),
                               bound.end());
      f->Enter(interpreter);
    }
  }
  NativeFunctionBase* f;
  std::vector<Lazy*> bound;
};

struct UserLambda final : public Lambda {
  UserLambda(Interpreter& interpreter, const core::Lambda& definition)
      : definition(definition), captures(interpreter.Resolve(definition)) {}
  void Enter(Interpreter& interpreter) override {
    for (const auto& [id, value] : captures) {
      interpreter.names[id].push_back(value);
    }
    Lazy* v = interpreter.stack.back();
    interpreter.names[definition.parameter].push_back(v);
    interpreter.stack.back() = interpreter.LazyEvaluate(definition.result);
    interpreter.names[definition.parameter].pop_back();
    for (const auto& [id, value] : captures) {
      interpreter.names[id].pop_back();
    }
  }
  void AddChildren(std::vector<Node*>& frontier) override {
    for (const auto& [id, value] : captures) frontier.push_back(value);
  }
  const core::Lambda& definition;
  Interpreter::Captures captures;
};

struct Apply final : public Thunk {
  Apply(Lazy* f, Lazy* x) : f(f), x(x) {}
  void AddChildren(std::vector<Node*>& frontier) override {
    frontier.push_back(f);
    frontier.push_back(x);
  }
  GCPtr<Value> Run(Interpreter& interpreter) override {
    interpreter.stack.push_back(x);
    f->Get(interpreter)->Enter(interpreter);
    GCPtr<Value> v = interpreter.stack.back()->Get(interpreter);
    interpreter.stack.pop_back();
    return v;
  }
  Lazy* f;
  Lazy* x;
};

struct Read final : public Thunk {
  void AddChildren(std::vector<Node*>&) override {}
  GCPtr<Value> Run(Interpreter& interpreter) override {
    char c;
    if (std::cin.get(c)) {
      return interpreter.Cons(
          interpreter.Allocate<Lazy>(interpreter.Allocate<Char>(c)),
          interpreter.Allocate<Lazy>(this));
    } else {
      return interpreter.Nil();
    }
  }
};

struct ConcatThunk final : public Thunk {
  ConcatThunk(Lazy* l, Lazy* r) : l(l), r(r) {}
  GCPtr<Value> Run(Interpreter& interpreter) override {
    GCPtr<Value> v = l->Get(interpreter);
    if (v->GetType() != Value::Type::kUnion) {
      throw std::runtime_error(StrCat("malformed string: tail is ",
                                      Name(v->GetType()), ", not list"));
    }
    const Union& u = v->AsUnion();
    if (u.type_id != core::UnionType::Id::kList) {
      throw std::runtime_error(
          StrCat("malformed string: tail is ", u.type_id, ", not list"));
    }
    if (u.index == 0) {
      l = u.elements[1];
      return interpreter.Cons(u.elements[0], interpreter.Allocate<Lazy>(this));
    } else if (u.index == 1) {
      return r->Get(interpreter);
    } else {
      throw std::runtime_error("concat argument is not a list");
    }
  }
  void AddChildren(std::vector<Node*>& frontier) override {
    frontier.push_back(l);
    frontier.push_back(r);
  }
  Lazy* l;
  Lazy* r;
};

struct Concat : public NativeFunction<2> {
  GCPtr<Value> Run(Interpreter& interpreter,
                   std::span<GCPtr<Lazy>, 2> args) override {
    return interpreter.Allocate<ConcatThunk>(args[0], args[1])
        ->Run(interpreter);
  }
};

struct MakeError : public NativeFunction<1> {
  GCPtr<Value> Run(Interpreter& interpreter,
                   std::span<GCPtr<Lazy>, 1> args) override {
    throw std::runtime_error("error: " + interpreter.EvaluateString(args[0]));
  }
};

void Lazy::AddChildren(std::vector<Node*>& frontier) {
  if (has_value_) {
    frontier.push_back(value_);
  } else {
    frontier.push_back(thunk_);
  }
}

bool Value::AsBool() const {
  if (GetType() != Type::kUnion ||
      AsUnion().type_id != core::UnionType::Id::kBool) {
    throw std::runtime_error("not a bool");
  }
  return AsUnion().index;
}

std::int64_t Value::AsInt64() const {
  if (GetType() != Type::kInt64) throw std::runtime_error("not an int64");
  return static_cast<const Int64*>(this)->value;
}

char Value::AsChar() const {
  if (GetType() != Type::kChar) throw std::runtime_error("not a char");
  return static_cast<const Char*>(this)->value;
}

std::span<Lazy* const> Value::AsTuple() const {
  if (GetType() != Type::kTuple) throw std::runtime_error("not a tuple");
  return static_cast<const Tuple*>(this)->elements;
}

const Union& Value::AsUnion() const {
  if (GetType() != Type::kUnion) throw std::runtime_error("not a union");
  return *static_cast<const Union*>(this);
}

void Value::Enter(Interpreter& interpreter) {
  if (GetType() != Type::kLambda) throw std::runtime_error("not a lambda");
  return static_cast<Lambda*>(this)->Enter(interpreter);
}

template <std::derived_from<Node> T, typename... Args>
requires std::constructible_from<T, Args...>
GCPtr<T> Interpreter::Allocate(Args&&... args) {
  if (int(heap.size()) >= collect_at_size) CollectGarbage();
  auto u = std::make_unique<T>(std::forward<Args>(args)...);
  GCPtr<T> p(*this, u.get());
  heap.push_back(std::move(u));
  return p;
}

GCPtr<Value> Interpreter::Nil() {
  return Allocate<Union>(core::UnionType::Id::kList, 1);
}

GCPtr<Value> Interpreter::Cons(Lazy* head, Lazy* tail) {
  return Allocate<Union>(core::UnionType::Id::kList, 0,
                         std::span<Lazy* const>({head, tail}));
}

GCPtr<Value> Interpreter::Bool(bool value) {
  return Allocate<Union>(core::UnionType::Id::kBool, value ? 1 : 0);
}

std::string Interpreter::EvaluateString(GCPtr<Lazy> list) {
  std::string text;
  while (true) {
    GCPtr<Value> v = list->Get(*this);
    if (v->GetType() != Value::Type::kUnion) {
      throw std::runtime_error(StrCat("malformed string: tail is ",
                                      Name(v->GetType()), ", not list"));
    }
    const Union& u = v->AsUnion();
    if (u.type_id != core::UnionType::Id::kList) {
      throw std::runtime_error(
          StrCat("malformed string: tail is ", u.type_id, ", not list"));
    }
    if (u.index == 1) break;
    if (u.elements.size() != 2) {
      throw std::logic_error("corrupt cons in string");
    }
    GCPtr<Value> head = u.elements[0]->Get(*this);
    text.push_back(head->AsChar());
    list = u.elements[1];
  }
  return text;
}

void Interpreter::CollectGarbage() {
  for (auto& node : heap) node->reachable = false;
  std::vector<Node*> frontier;
  auto dfs = [&frontier](auto* n) {
    frontier.clear();
    n->Mark(frontier);
    while (!frontier.empty()) {
      Node* n = frontier.back();
      frontier.pop_back();
      n->Mark(frontier);
    }
  };
  for (auto& [name, nodes] : names) {
    for (const auto& node : nodes) dfs(node);
  }
  for (auto& node : stack) dfs(node);
  if (live) {
    GCPtrBase* i = live;
    do {
      dfs(i);
      i = i->next;
    } while (i != live);
  }
  std::erase_if(heap, [](const auto& node) { return !node->reachable; });
  collect_at_size = std::max<int>(128, 8 * heap.size());
}

void Interpreter::ResolveImpl(flat_set<core::Identifier>&, Captures&,
                              const core::Builtin&) {}

void Interpreter::ResolveImpl(flat_set<core::Identifier>& bound,
                              Captures& result, const core::Identifier& x) {
  if (!bound.contains(x)) result[x] = names.at(x).back();
}

void Interpreter::ResolveImpl(flat_set<core::Identifier>&, Captures&,
                              const core::Integer&) {}

void Interpreter::ResolveImpl(flat_set<core::Identifier>&, Captures&,
                              const core::Character&) {}

void Interpreter::ResolveImpl(flat_set<core::Identifier>& bound,
                              Captures& result,
                              const core::Tuple& x) {
  for (const auto& element : x.elements) {
    Resolve(bound, result, element);
  }
}

void Interpreter::ResolveImpl(flat_set<core::Identifier>& bound,
                              Captures& result,
                              const core::UnionConstructor& x) {}

void Interpreter::ResolveImpl(flat_set<core::Identifier>& bound,
                              Captures& result, const core::Apply& x) {
  Resolve(bound, result, x.f);
  Resolve(bound, result, x.x);
}

void Interpreter::ResolveImpl(flat_set<core::Identifier>& bound,
                              Captures& result, const core::Lambda& x) {
  auto [i, is_new] = bound.emplace(x.parameter);
  Resolve(bound, result, x.result);
  if (is_new) bound.erase(x.parameter);
}

void Interpreter::ResolveImpl(flat_set<core::Identifier>& bound,
                              Captures& result, const core::Let& x) {
  Resolve(bound, result, x.binding.result);
  auto [i, is_new] = bound.emplace(x.binding.name);
  Resolve(bound, result, x.result);
  if (is_new) bound.erase(x.binding.name);
}

void Interpreter::ResolveImpl(flat_set<core::Identifier>& bound,
                              Captures& result, const core::LetRecursive& x) {
  std::vector<core::Identifier> newly_bound;
  for (const auto& binding : x.bindings) {
    auto [i, is_new] = bound.emplace(binding.name);
    if (is_new) newly_bound.push_back(binding.name);
  }
  for (const auto& binding : x.bindings) {
    Resolve(bound, result, binding.result);
  }
  Resolve(bound, result, x.result);
  for (const auto& id : newly_bound) bound.erase(id);
}

void Interpreter::ResolveImpl(flat_set<core::Identifier>& bound,
                              Captures& result,
                              const core::Case::Alternative& x) {
  flat_set<core::Identifier> bindings = GetBindings(x.pattern);
  flat_set<core::Identifier> newly_bound;
  for (const auto& binding : bindings) {
    auto [i, is_new] = bound.emplace(binding);
    if (is_new) newly_bound.insert(binding);
  }
  Resolve(bound, result, x.value);
  for (const auto& id : newly_bound) bound.erase(id);
}

void Interpreter::ResolveImpl(flat_set<core::Identifier>& bound,
                              Captures& result, const core::Case& x) {
  Resolve(bound, result, x.value);
  for (const auto& alternative : x.alternatives) {
    ResolveImpl(bound, result, alternative);
  }
}

void Interpreter::Resolve(flat_set<core::Identifier>& bound, Captures& result,
                          const core::Expression& x) {
  return std::visit(
      [this, &bound, &result](const auto& x) {
        return ResolveImpl(bound, result, x);
      },
      x->value);
}

Interpreter::Captures Interpreter::Resolve(const core::Expression& x) {
  flat_set<core::Identifier> bound;
  Captures result;
  Resolve(bound, result, x);
  return result;
}

flat_set<core::Identifier> Interpreter::GetBindingsImpl(
    const core::Identifier& x) {
  return {x};
}

flat_set<core::Identifier> Interpreter::GetBindingsImpl(
    const core::MatchTuple& x) {
  return flat_set<core::Identifier>(x.elements);
}

flat_set<core::Identifier> Interpreter::GetBindingsImpl(
    const core::MatchUnion& x) {
  return flat_set<core::Identifier>(x.elements);
}

flat_set<core::Identifier> Interpreter::GetBindingsImpl(const core::Integer&) {
  return {};
}

flat_set<core::Identifier> Interpreter::GetBindingsImpl(
    const core::Character&) {
  return {};
}

flat_set<core::Identifier> Interpreter::GetBindings(const core::Pattern& x) {
  return std::visit([this](const auto& x) { return GetBindingsImpl(x); },
                    x->value);
}

GCPtr<Value> Interpreter::Evaluate(const core::Builtin& x) {
  switch (x) {
    case core::Builtin::kAdd:
      return Allocate<NativeClosure>(Allocate<Add>());
    case core::Builtin::kAnd:
      return Allocate<NativeClosure>(Allocate<And>());
    case core::Builtin::kChr:
      return Allocate<NativeClosure>(Allocate<Chr>());
    case core::Builtin::kConcat:
      return Allocate<NativeClosure>(Allocate<Concat>());
    case core::Builtin::kDivide:
      return Allocate<NativeClosure>(Allocate<Divide>());
    case core::Builtin::kError:
      return Allocate<NativeClosure>(Allocate<MakeError>());
    case core::Builtin::kEqual:
      return Allocate<NativeClosure>(Allocate<Equal>());
    case core::Builtin::kLessThan:
      return Allocate<NativeClosure>(Allocate<LessThan>());
    case core::Builtin::kModulo:
      return Allocate<NativeClosure>(Allocate<Modulo>());
    case core::Builtin::kMultiply:
      return Allocate<NativeClosure>(Allocate<Multiply>());
    case core::Builtin::kNot:
      return Allocate<NativeClosure>(Allocate<Not>());
    case core::Builtin::kOr:
      return Allocate<NativeClosure>(Allocate<Or>());
    case core::Builtin::kOrd:
      return Allocate<NativeClosure>(Allocate<Ord>());
    case core::Builtin::kReadInt:
      return Allocate<NativeClosure>(Allocate<ReadInt>());
    case core::Builtin::kShowInt:
      return Allocate<NativeClosure>(Allocate<ShowInt>());
    case core::Builtin::kSubtract:
      return Allocate<NativeClosure>(Allocate<Subtract>());
  }
  throw std::runtime_error(StrCat("unimplemented builtin: ", x));
}

GCPtr<Value> Interpreter::Evaluate(const core::Identifier& identifier) {
  return LazyEvaluate(identifier)->Get(*this);
}

GCPtr<Value> Interpreter::Evaluate(const core::Integer& x) {
  return Allocate<Int64>(x.value);
}

GCPtr<Value> Interpreter::Evaluate(const core::Character& x) {
  return Allocate<Char>(x.value);
}

GCPtr<Value> Interpreter::Evaluate(const core::Tuple& x) {
  GCPtr<Tuple> tuple = Allocate<Tuple>();
  for (const auto& element : x.elements) {
    tuple->elements.push_back(LazyEvaluate(element));
  }
  return tuple;
}

GCPtr<Value> Interpreter::Evaluate(const core::UnionConstructor& x) {
  const int arity = x.type->alternatives.at(x.index).num_members;
  if (arity == 0) {
    return Allocate<Union>(x.type->id, x.index);
  } else {
    return Allocate<NativeClosure>(Allocate<UnionConstructor>(x));
  }
}

GCPtr<Value> Interpreter::Evaluate(const core::Apply& x) {
  return LazyEvaluate(x)->Get(*this);
}

GCPtr<Value> Interpreter::Evaluate(const core::Lambda& x) {
  return Allocate<UserLambda>(*this, x);
}

GCPtr<Value> Interpreter::Evaluate(const core::Let& x) {
  return LazyEvaluate(x)->Get(*this);
}

GCPtr<Value> Interpreter::Evaluate(const core::LetRecursive& x) {
  return LazyEvaluate(x)->Get(*this);
}

GCPtr<Value> Interpreter::Evaluate(const core::Case& x) {
  return LazyEvaluate(x)->Get(*this);
}

GCPtr<Value> Interpreter::Evaluate(const core::Expression& x) {
  return std::visit([this](const auto& x) { return Evaluate(x); },
                    x->value);
}

GCPtr<Lazy> Interpreter::LazyEvaluate(const core::Builtin& x) {
  return Allocate<Lazy>(Evaluate(x));
}

GCPtr<Lazy> Interpreter::LazyEvaluate(const core::Identifier& identifier) {
  return GCPtr<Lazy>(*this, names.at(identifier).back());
}

GCPtr<Lazy> Interpreter::LazyEvaluate(const core::Integer& x) {
  return Allocate<Lazy>(Evaluate(x));
}

GCPtr<Lazy> Interpreter::LazyEvaluate(const core::Character& x) {
  return Allocate<Lazy>(Evaluate(x));
}

GCPtr<Lazy> Interpreter::LazyEvaluate(const core::Tuple& x) {
  return Allocate<Lazy>(Evaluate(x));
}

GCPtr<Lazy> Interpreter::LazyEvaluate(const core::UnionConstructor& x) {
  return Allocate<Lazy>(Evaluate(x));
}

GCPtr<Lazy> Interpreter::LazyEvaluate(const core::Apply& x) {
  return Allocate<Lazy>(Allocate<Apply>(LazyEvaluate(x.f), LazyEvaluate(x.x)));
}

GCPtr<Lazy> Interpreter::LazyEvaluate(const core::Lambda& x) {
  return Allocate<Lazy>(Evaluate(x));
}

GCPtr<Lazy> Interpreter::LazyEvaluate(const core::Let& x) {
  return Allocate<Lazy>(Allocate<Let>(*this, x));
}

GCPtr<Lazy> Interpreter::LazyEvaluate(const core::LetRecursive& x) {
  return Allocate<Lazy>(Allocate<LetRecursive>(*this, x));
}

GCPtr<Lazy> Interpreter::LazyEvaluate(const core::Case& x) {
  return Allocate<Lazy>(Allocate<Case>(*this, x));
}

GCPtr<Lazy> Interpreter::LazyEvaluate(const core::Expression& x) {
  return std::visit([this](const auto& x) { return LazyEvaluate(x); },
                    x->value);
}

GCPtr<Value> Interpreter::TryAlternative(Value* v,
                                         const core::Case::Alternative& x) {
  return std::visit(
      [&](const auto& pattern) { return TryAlternative(v, pattern, x.value); },
      x.pattern->value);
}

GCPtr<Value> Interpreter::TryAlternative(Value* v, const core::Identifier& i,
                                   const core::Expression& x) {
  names[i].push_back(Allocate<Lazy>(v));
  GCPtr<Value> result = Evaluate(x);
  names[i].pop_back();
  return result;
}

GCPtr<Value> Interpreter::TryAlternative(Value* v, const core::MatchTuple& d,
                                         const core::Expression& x) {
  if (v->GetType() != Value::Type::kTuple) {
    throw std::runtime_error(StrCat("attempting to match ", Name(v->GetType()),
                                    " with tuple pattern"));
  }
  std::span<Lazy* const> elements = v->AsTuple();
  if (elements.size() != d.elements.size()) {
    throw std::runtime_error(
        StrCat("attempting to match tuple of size ", elements.size(),
               " with tuple pattern of size ", d.elements.size()));
  }
  const int n = elements.size();
  for (int i = 0; i < n; i++) {
    names[d.elements[i]].push_back(elements[i]);
  }
  GCPtr<Value> result = Evaluate(x);
  for (int i = 0; i < n; i++) {
    names[d.elements[i]].pop_back();
  }
  return result;
}

GCPtr<Value> Interpreter::TryAlternative(Value* v, const core::MatchUnion& d,
                                         const core::Expression& x) {
  if (v->GetType() != Value::Type::kUnion) {
    throw std::runtime_error(StrCat("attempting to match ", Name(v->GetType()),
                                    " with type constructor"));
  }
  const Union& value = v->AsUnion();
  if (value.type_id != d.type->id) {
    throw std::runtime_error(
        StrCat("attempting to match value of type ", value.type_id,
               " with type constructor for type ", d.type->id));
  }
  if (value.index != d.index) return nullptr;
  if (value.elements.size() != d.elements.size()) {
    throw std::logic_error(StrCat(
        "mismatch in cardinality for constructor ", value.index, " in type ",
        value.type_id, ": ", value.elements.size(), " vs ", d.elements.size()));
  }
  const int n = value.elements.size();
  for (int i = 0; i < n; i++) {
    names[d.elements[i]].push_back(value.elements[i]);
  }
  GCPtr<Value> result = Evaluate(x);
  for (int i = 0; i < n; i++) {
    names[d.elements[i]].pop_back();
  }
  return result;
}

GCPtr<Value> Interpreter::TryAlternative(Value* v, const core::Integer& i,
                                   const core::Expression& x) {
  if (v->GetType() != Value::Type::kInt64 || v->AsInt64() != i.value) {
    return nullptr;
  }
  return Evaluate(x);
}

GCPtr<Value> Interpreter::TryAlternative(Value* v, const core::Character& c,
                                   const core::Expression& x) {
  if (v->GetType() != Value::Type::kChar || v->AsChar() != c.value) {
    return nullptr;
  }
  return Evaluate(x);
}

void Interpreter::Run(const core::Expression& program) {
  GCPtr<Lazy> output = Allocate<Lazy>(
      Allocate<Apply>(LazyEvaluate(program), Allocate<Lazy>(Allocate<Read>())));
  while (true) {
    GCPtr<Value> v = output->Get(*this);
    if (v->GetType() != Value::Type::kUnion) {
      throw std::runtime_error(StrCat("malformed string: tail is ",
                                      Name(v->GetType()), ", not list"));
    }
    const Union& u = v->AsUnion();
    if (u.type_id != core::UnionType::Id::kList) {
      throw std::runtime_error(
          StrCat("malformed string: tail is ", u.type_id, ", not list"));
    }
    if (u.index == 1) break;
    GCPtr<Value> head = u.elements[0]->Get(*this);
    std::cout << head->AsChar();
    output = u.elements[1];
  }
}

}  // namespace

void Run(const core::Expression& program) {
  Interpreter interpreter;
  interpreter.Run(program);
}

}  // namespace aoc2022
