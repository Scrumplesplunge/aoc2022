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
  GCPtr(Interpreter* interpreter, T* value);
  template <typename U = T>
  requires std::convertible_to<U*, T*>
  GCPtr(const GCPtr<U>& other) : GCPtr(other.interpreter_, other.value_) {}
  GCPtr(const GCPtr& other) : GCPtr(other.interpreter_, other.value_) {}
  GCPtr& operator=(const GCPtr& other);

  template <typename U = T>
  requires std::convertible_to<U*, T*>
  GCPtr(GCPtr<U>&& other);
  GCPtr& operator=(GCPtr&& other);

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
  virtual Value* Run(Interpreter& interpreter) = 0;
};

class Lazy final : public Node {
 public:
  Lazy(Value* value) : has_value_(true), value_(value) {}
  Lazy(Thunk* thunk) : has_value_(false), thunk_(thunk) {}
  Value* Get(Interpreter& interpreter) {
    if (!has_value_) {
      // Evaluation of the thunk relies on evaluating itself: the expression
      // diverges without reaching weak head normal form.
      if (computing_) throw std::runtime_error("divergence");
      computing_ = true;
      value_ = thunk_->Run(interpreter);
      has_value_ = true;
      computing_ = false;
    }
    return value_;
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

  Value* Nil();
  Value* Cons(Lazy* head, Lazy* tail);
  Value* Bool(bool value);
  std::string EvaluateString(Lazy* list);

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

  Value* Evaluate(const core::Builtin& x);
  Value* Evaluate(const core::Identifier& x);
  Value* Evaluate(const core::Integer& x);
  Value* Evaluate(const core::Character& x);
  Value* Evaluate(const core::Tuple& x);
  Value* Evaluate(const core::UnionConstructor& x);
  Value* Evaluate(const core::Apply& x);
  Value* Evaluate(const core::Lambda& x);
  Value* Evaluate(const core::Let& x);
  Value* Evaluate(const core::LetRecursive& x);
  Value* Evaluate(const core::Case& x);
  Value* Evaluate(const core::Expression& x);

  template <typename T>
  GCPtr<T> Wrap(T* x) { return GCPtr<T>(this, x); }

  Lazy* LazyEvaluate(const core::Builtin& x);
  Lazy* LazyEvaluate(const core::Identifier& x);
  Lazy* LazyEvaluate(const core::Integer& x);
  Lazy* LazyEvaluate(const core::Character& x);
  Lazy* LazyEvaluate(const core::Tuple& x);
  Lazy* LazyEvaluate(const core::UnionConstructor& x);
  Lazy* LazyEvaluate(const core::Apply& x);
  Lazy* LazyEvaluate(const core::Lambda& x);
  Lazy* LazyEvaluate(const core::Let& x);
  Lazy* LazyEvaluate(const core::LetRecursive& x);
  Lazy* LazyEvaluate(const core::Case& x);
  Lazy* LazyEvaluate(const core::Expression& x);

  Value* TryAlternative(Value*, const core::Case::Alternative& x);
  Value* TryAlternative(Value*, const core::Identifier&,
                        const core::Expression& x);
  Value* TryAlternative(Value*, const core::MatchTuple&,
                        const core::Expression& x);
  Value* TryAlternative(Value*, const core::MatchUnion&,
                        const core::Expression& x);
  Value* TryAlternative(Value*, const core::Integer&,
                        const core::Expression& x);
  Value* TryAlternative(Value*, const core::Character&,
                        const core::Expression& x);

  void Run(const core::Expression& program);

  std::vector<std::unique_ptr<Node>> heap;
  int collect_at_size = 128;
  std::map<core::Identifier, std::vector<Lazy*>> names;
  std::vector<Lazy*> stack;
  GCPtrBase* live = nullptr;
};

template <std::derived_from<Node> T>
GCPtr<T>::GCPtr(Interpreter* interpreter, T* value)
    : interpreter_(interpreter), value_(value) {
  if (interpreter_) interpreter_->AddPtr(this);
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
template <typename U>
requires std::convertible_to<U*, T*>
GCPtr<T>::GCPtr(GCPtr<U>&& other)
    : interpreter_(other.interpreter_), value_(other.value_) {
  if (interpreter_) {
    next = other.next;
    prev = other.prev;
    other.next->prev = this;
    other.prev->next = this;
    other.interpreter_ = nullptr;
  }
}

template <std::derived_from<Node> T>
GCPtr<T>& GCPtr<T>::operator=(GCPtr<T>&& other) {
  if (interpreter_ != other.interpreter_) {
    if (interpreter_) interpreter_->RemovePtr(this);
    interpreter_ = other.interpreter_;
    if (interpreter_) {
      next = other.next;
      prev = other.prev;
      other.next->prev = this;
      other.prev->next = this;
      other.interpreter_ = nullptr;
    }
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
  virtual Value* RunBody(Interpreter&) = 0;
  Value* Run(Interpreter& interpreter) final {
    for (const auto& [id, value] : captures) {
      interpreter.names[id].push_back(value);
    }
    Value* result = RunBody(interpreter);
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
  Value* RunBody(Interpreter& interpreter) override {
    interpreter.names[definition.binding.variable].push_back(
        interpreter.LazyEvaluate(definition.binding.value));
    Value* result = interpreter.Evaluate(definition.value);
    interpreter.names[definition.binding.variable].pop_back();
    return result;
  }
  const core::Let& definition;
};

struct Error final : public Thunk {
  Error(std::string message) : message(std::move(message)) {}
  void AddChildren(std::vector<Node*>&) override {}
  Value* Run(Interpreter&) override {
    throw std::runtime_error(message);
  }
  std::string message;
};

struct LetRecursive final : public Closure {
  LetRecursive(Interpreter& interpreter, const core::LetRecursive& definition)
      : Closure(interpreter.Resolve(definition)), definition(definition) {}
  Value* RunBody(Interpreter& interpreter) override {
    std::vector<Lazy*> holes;
    for (const auto& [id, value] : definition.bindings) {
      Lazy* l = interpreter.Allocate<Lazy>(
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
      Lazy* value = interpreter.LazyEvaluate(definition.bindings[i].value);
      if (holes[i] == value) {
        *holes[i] = Lazy(interpreter.Allocate<Error>("divergence"));
      } else {
        *holes[i] = *value;
      }
    }
    Value* result = interpreter.Evaluate(definition.value);
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
  Value* RunBody(Interpreter& interpreter) override {
    GCPtr<Value> v(&interpreter, interpreter.Evaluate(definition.value));
    for (const auto& alternative : definition.alternatives) {
      if (Value* r = interpreter.TryAlternative(v, alternative)) return r;
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

struct NativeFunctionBase {
  NativeFunctionBase(int arity) : arity(arity) {}
  virtual void Enter(Interpreter& interpreter) = 0;
  const int arity;
};

struct UnionConstructor final : public NativeFunctionBase {
  UnionConstructor(const core::UnionConstructor& x)
      : NativeFunctionBase(x.type->alternatives.at(x.index).num_members),
        type_id(x.type->id),
        index(x.index) {}
  void Enter(Interpreter& interpreter) override {
    Lazy* value = interpreter.Allocate<Lazy>(interpreter.Allocate<Union>(
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
    std::array<Lazy*, n> args;
    const int m = interpreter.stack.size();
    for (int i = 0; i < n; i++) args[i] = interpreter.stack[m - n + i];
    Value* v = Run(interpreter, args);
    interpreter.stack.resize(interpreter.stack.size() - n + 1);
    interpreter.stack.back() = interpreter.Allocate<Lazy>(interpreter.Wrap(v));
  }
  virtual Value* Run(Interpreter& interpreter,
                     std::span<Lazy* const, n> args) = 0;
};

struct Not : public NativeFunction<1> {
  Value* Run(Interpreter& interpreter,
             std::span<Lazy* const, 1> args) override {
    return interpreter.Bool(!args[0]->Get(interpreter)->AsBool());
  }
};

struct Chr : public NativeFunction<1> {
  Value* Run(Interpreter& interpreter,
             std::span<Lazy* const, 1> args) override {
    const std::int64_t i = args[0]->Get(interpreter)->AsInt64();
    if (0 <= i && i < 128) {
      return interpreter.Allocate<Char>(static_cast<char>(i));
    } else {
      throw std::runtime_error(StrCat("Value ", i, " is out of range for chr"));
    }
  }
};

struct Ord : public NativeFunction<1> {
  Value* Run(Interpreter& interpreter,
             std::span<Lazy* const, 1> args) override {
    const char c = args[0]->Get(interpreter)->AsChar();
    return interpreter.Allocate<Int64>(static_cast<std::int64_t>(c));
  }
};

template <auto F>
struct BinaryOperatorInt64 : public NativeFunction<2> {
  Value* Run(Interpreter& interpreter,
             std::span<Lazy* const, 2> args) override {
    const std::int64_t l = args[0]->Get(interpreter)->AsInt64();
    const std::int64_t r = args[1]->Get(interpreter)->AsInt64();
    return interpreter.Allocate<Int64>(F(l, r));
  }
};

using Add = BinaryOperatorInt64<[](auto l, auto r) { return l + r; }>;
using Subtract = BinaryOperatorInt64<[](auto l, auto r) { return l - r; }>;
using Multiply = BinaryOperatorInt64<[](auto l, auto r) { return l * r; }>;
using Divide = BinaryOperatorInt64<[](auto l, auto r) { return l / r; }>;
using Modulo = BinaryOperatorInt64<[](auto l, auto r) { return l % r; }>;
using BitwiseAnd = BinaryOperatorInt64<[](auto l, auto r) { return l & r; }>;
using BitwiseOr = BinaryOperatorInt64<[](auto l, auto r) { return l | r; }>;
using BitShift = BinaryOperatorInt64<[](auto l, auto r) {
  // These brackets look redundant, but the compiler cannot parse this without
  // them, presumably because it is inside <> brackets for the template.
  return (r > 0 ? l << r : l >> -r);
}>;

struct And : public NativeFunction<2> {
  Value* Run(Interpreter& interpreter,
             std::span<Lazy* const, 2> args) override {
    Value* l = args[0]->Get(interpreter);
    if (!l->AsBool()) return l;
    return args[1]->Get(interpreter);
  }
};

struct Or : public NativeFunction<2> {
  Value* Run(Interpreter& interpreter,
             std::span<Lazy* const, 2> args) override {
    Value* l = args[0]->Get(interpreter);
    if (l->AsBool()) return l;
    return args[1]->Get(interpreter);
  }
};

struct Equal : public NativeFunction<2> {
  bool Run(Interpreter& interpreter, Lazy* lazy_l, Lazy* lazy_r) {
    Value* l = lazy_l->Get(interpreter);
    Value* r = lazy_r->Get(interpreter);
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
  Value* Run(Interpreter& interpreter,
             std::span<Lazy* const, 2> args) override {
    return interpreter.Bool(Run(interpreter, args[0], args[1]));
  }
};

struct LessThan : public NativeFunction<2> {
  Value* Run(Interpreter& interpreter,
             std::span<Lazy* const, 2> args) override {
    Value* l = args[0]->Get(interpreter);
    Value* r = args[1]->Get(interpreter);
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
  Value* Run(Interpreter& interpreter,
             std::span<Lazy* const, 1> args) override {
    const std::int64_t value = args[0]->Get(interpreter)->AsInt64();
    std::string text = std::to_string(value);
    GCPtr<Value> result(&interpreter, interpreter.Nil());
    for (int i = text.size() - 1; i >= 0; i--) {
      result = interpreter.Cons(
          interpreter.Allocate<Lazy>(interpreter.Allocate<Char>(text[i])),
          interpreter.Allocate<Lazy>(result));
    }
    return result;
  }
};

struct ReadInt : public NativeFunction<1> {
  Value* Run(Interpreter& interpreter,
             std::span<Lazy* const, 1> args) override {
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

template <typename F>
struct NativeClosure : public Lambda {
  NativeClosure(F f = F(), std::vector<Lazy*> b = {})
      : f(std::move(f)), bound(std::move(b)) {
    if ((int)bound.size() >= f.arity) {
      throw std::logic_error("creating (over)saturated native closure");
    }
  }
  void AddChildren(std::vector<Node*>& frontier) override {
    frontier.insert(frontier.end(), bound.begin(), bound.end());
  }
  void Enter(Interpreter& interpreter) override {
    const int required = f.arity - bound.size();
    if (required > 1) {
      std::vector<Lazy*> newly_bound = bound;
      newly_bound.push_back(interpreter.stack.back());
      interpreter.stack.back() = interpreter.Allocate<Lazy>(
          interpreter.Allocate<NativeClosure<F>>(f, std::move(newly_bound)));
    } else {
      interpreter.stack.insert(interpreter.stack.end() - 1, bound.begin(),
                               bound.end());
      f.Enter(interpreter);
    }
  }
  F f;
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
    interpreter.stack.back() = interpreter.Allocate<Lazy>(
        interpreter.Wrap(interpreter.Evaluate(definition.result)));
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
  Value* Run(Interpreter& interpreter) override {
    interpreter.stack.push_back(x);
    f->Get(interpreter)->Enter(interpreter);
    Value* v = interpreter.stack.back()->Get(interpreter);
    interpreter.stack.pop_back();
    return v;
  }
  Lazy* f;
  Lazy* x;
};

struct Read final : public Thunk {
  void AddChildren(std::vector<Node*>&) override {}
  Value* Run(Interpreter& interpreter) override {
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
  Value* Run(Interpreter& interpreter) override {
    GCPtr<Value> v(&interpreter, l->Get(interpreter));
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
  Value* Run(Interpreter& interpreter,
             std::span<Lazy* const, 2> args) override {
    return interpreter.Allocate<ConcatThunk>(args[0], args[1])
        ->Run(interpreter);
  }
};

struct MakeError : public NativeFunction<1> {
  Value* Run(Interpreter& interpreter,
             std::span<Lazy* const, 1> args) override {
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
  GCPtr<T> p(this, u.get());
  heap.push_back(std::move(u));
  return p;
}

Union nil(core::UnionType::Id::kList, 1);

Value* Interpreter::Nil() { return &nil; }

Value* Interpreter::Cons(Lazy* head, Lazy* tail) {
  return Allocate<Union>(core::UnionType::Id::kList, 0,
                         std::span<Lazy* const>({head, tail}));
}

Union true_value(core::UnionType::Id::kBool, 1);
Union false_value(core::UnionType::Id::kBool, 0);

Value* Interpreter::Bool(bool value) {
  return value ? &true_value : &false_value;
}

std::string Interpreter::EvaluateString(Lazy* list) {
  std::string text;
  while (true) {
    Value* v = list->Get(*this);
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
    Value* head = u.elements[0]->Get(*this);
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
  Resolve(bound, result, x.binding.value);
  auto [i, is_new] = bound.emplace(x.binding.variable);
  Resolve(bound, result, x.value);
  if (is_new) bound.erase(x.binding.variable);
}

void Interpreter::ResolveImpl(flat_set<core::Identifier>& bound,
                              Captures& result, const core::LetRecursive& x) {
  std::vector<core::Identifier> newly_bound;
  for (const auto& binding : x.bindings) {
    auto [i, is_new] = bound.emplace(binding.variable);
    if (is_new) newly_bound.push_back(binding.variable);
  }
  for (const auto& binding : x.bindings) {
    Resolve(bound, result, binding.value);
  }
  Resolve(bound, result, x.value);
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

NativeClosure<Add> builtin_add;
NativeClosure<And> builtin_and;
NativeClosure<BitShift> builtin_bit_shift;
NativeClosure<BitwiseAnd> builtin_bitwise_and;
NativeClosure<BitwiseOr> builtin_bitwise_or;
NativeClosure<Chr> builtin_chr;
NativeClosure<Concat> builtin_concat;
NativeClosure<Divide> builtin_divide;
NativeClosure<MakeError> builtin_error;
NativeClosure<Equal> builtin_equal;
NativeClosure<LessThan> builtin_less_than;
NativeClosure<Modulo> builtin_modulo;
NativeClosure<Multiply> builtin_multiply;
NativeClosure<Not> builtin_not;
NativeClosure<Or> builtin_or;
NativeClosure<Ord> builtin_ord;
NativeClosure<ReadInt> builtin_read_int;
NativeClosure<ShowInt> builtin_show_int;
NativeClosure<Subtract> builtin_subtract;

Value* Interpreter::Evaluate(const core::Builtin& x) {
  switch (x) {
    case core::Builtin::kAdd:
      return &builtin_add;
    case core::Builtin::kAnd:
      return &builtin_and;
    case core::Builtin::kBitShift:
      return &builtin_bit_shift;
    case core::Builtin::kBitwiseAnd:
      return &builtin_bitwise_and;
    case core::Builtin::kBitwiseOr:
      return &builtin_bitwise_or;
    case core::Builtin::kChr:
      return &builtin_chr;
    case core::Builtin::kConcat:
      return &builtin_concat;
    case core::Builtin::kDivide:
      return &builtin_divide;
    case core::Builtin::kError:
      return &builtin_error;
    case core::Builtin::kEqual:
      return &builtin_equal;
    case core::Builtin::kLessThan:
      return &builtin_less_than;
    case core::Builtin::kModulo:
      return &builtin_modulo;
    case core::Builtin::kMultiply:
      return &builtin_multiply;
    case core::Builtin::kNot:
      return &builtin_not;
    case core::Builtin::kOr:
      return &builtin_or;
    case core::Builtin::kOrd:
      return &builtin_ord;
    case core::Builtin::kReadInt:
      return &builtin_read_int;
    case core::Builtin::kShowInt:
      return &builtin_show_int;
    case core::Builtin::kSubtract:
      return &builtin_subtract;
  }
  throw std::runtime_error(StrCat("unimplemented builtin: ", x));
}

Value* Interpreter::Evaluate(const core::Identifier& identifier) {
  return names.at(identifier).back()->Get(*this);
}

Value* Interpreter::Evaluate(const core::Integer& x) {
  return Allocate<Int64>(x.value);
}

Value* Interpreter::Evaluate(const core::Character& x) {
  return Allocate<Char>(x.value);
}

Value* Interpreter::Evaluate(const core::Tuple& x) {
  GCPtr<Tuple> tuple = Allocate<Tuple>();
  for (const auto& element : x.elements) {
    tuple->elements.push_back(LazyEvaluate(element));
  }
  return tuple;
}

Value* Interpreter::Evaluate(const core::UnionConstructor& x) {
  const int arity = x.type->alternatives.at(x.index).num_members;
  if (arity == 0) {
    return Allocate<Union>(x.type->id, x.index);
  } else {
    return Allocate<NativeClosure<UnionConstructor>>(UnionConstructor(x));
  }
}

Value* Interpreter::Evaluate(const core::Apply& x) {
  stack.push_back(LazyEvaluate(x.x));
  Wrap(Evaluate(x.f))->Enter(*this);
  Value* v = stack.back()->Get(*this);
  stack.pop_back();
  return v;
}

Value* Interpreter::Evaluate(const core::Lambda& x) {
  return Allocate<UserLambda>(*this, x);
}

Value* Interpreter::Evaluate(const core::Let& x) {
  names[x.binding.variable].push_back(LazyEvaluate(x.binding.value));
  Value* result = Evaluate(x.value);
  names[x.binding.variable].pop_back();
  return result;
}

Value* Interpreter::Evaluate(const core::LetRecursive& x) {
  std::vector<Lazy*> holes;
  for (const auto& [id, value] : x.bindings) {
    GCPtr<Lazy> l =
        Allocate<Lazy>(Allocate<Error>("this should never be executed"));
    names[id].push_back(l);
    holes.push_back(l);
  }
  for (int i = 0, n = x.bindings.size(); i < n; i++) {
    // There are two possible cases for the return value here.
    //
    //   * The return value is the value itself, which is currently just
    //     a hole. In this case, the expression has no weak head normal form:
    //     it diverges, so we replace it with an error.
    //   * The return value is *not* the value itself. In this case, we will
    //     overwrite the hole with the thunk for the actual value. This may
    //     refer to the value itself internally, at which point it will
    //     evaluate as the newly-assigned value.
    Lazy* value = LazyEvaluate(x.bindings[i].value);
    if (holes[i] == value) {
      *holes[i] = Lazy(Allocate<Error>("divergence"));
    } else {
      *holes[i] = *value;
    }
  }
  Value* result = Evaluate(x.value);
  for (const auto& [id, value] : x.bindings) {
    names[id].pop_back();
  }
  return result;
}

Value* Interpreter::Evaluate(const core::Case& x) {
  GCPtr<Value> v(this, Evaluate(x.value));
  for (const auto& alternative : x.alternatives) {
    if (Value* r = TryAlternative(v, alternative)) return r;
  }
  throw std::runtime_error(StrCat("non-exhaustative case: nothing to match ",
                                  Name(v->GetType()),
                                  ". core: ", x));
}

Value* Interpreter::Evaluate(const core::Expression& x) {
  return std::visit([this](const auto& x) { return Evaluate(x); },
                    x->value);
}

Lazy* Interpreter::LazyEvaluate(const core::Builtin& x) {
  return Allocate<Lazy>(Wrap(Evaluate(x)));
}

Lazy* Interpreter::LazyEvaluate(const core::Identifier& identifier) {
  return names.at(identifier).back();
}

Lazy* Interpreter::LazyEvaluate(const core::Integer& x) {
  return Allocate<Lazy>(Wrap(Evaluate(x)));
}

Lazy* Interpreter::LazyEvaluate(const core::Character& x) {
  return Allocate<Lazy>(Wrap(Evaluate(x)));
}

Lazy* Interpreter::LazyEvaluate(const core::Tuple& x) {
  return Allocate<Lazy>(Wrap(Evaluate(x)));
}

Lazy* Interpreter::LazyEvaluate(const core::UnionConstructor& x) {
  return Allocate<Lazy>(Wrap(Evaluate(x)));
}

Lazy* Interpreter::LazyEvaluate(const core::Apply& x) {
  return Allocate<Lazy>(
      Allocate<Apply>(Wrap(LazyEvaluate(x.f)), Wrap(LazyEvaluate(x.x))));
}

Lazy* Interpreter::LazyEvaluate(const core::Lambda& x) {
  return Allocate<Lazy>(Wrap(Evaluate(x)));
}

Lazy* Interpreter::LazyEvaluate(const core::Let& x) {
  return Allocate<Lazy>(Allocate<Let>(*this, x));
}

Lazy* Interpreter::LazyEvaluate(const core::LetRecursive& x) {
  return Allocate<Lazy>(Allocate<LetRecursive>(*this, x));
}

Lazy* Interpreter::LazyEvaluate(const core::Case& x) {
  return Allocate<Lazy>(Allocate<Case>(*this, x));
}

Lazy* Interpreter::LazyEvaluate(const core::Expression& x) {
  return std::visit([this](const auto& x) { return LazyEvaluate(x); },
                    x->value);
}

Value* Interpreter::TryAlternative(Value* v, const core::Case::Alternative& x) {
  return std::visit(
      [&](const auto& pattern) { return TryAlternative(v, pattern, x.value); },
      x.pattern->value);
}

Value* Interpreter::TryAlternative(Value* v, const core::Identifier& i,
                                   const core::Expression& x) {
  names[i].push_back(Allocate<Lazy>(v));
  Value* result = Evaluate(x);
  names[i].pop_back();
  return result;
}

Value* Interpreter::TryAlternative(Value* v, const core::MatchTuple& d,
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
  Value* result = Evaluate(x);
  for (int i = 0; i < n; i++) {
    names[d.elements[i]].pop_back();
  }
  return result;
}

Value* Interpreter::TryAlternative(Value* v, const core::MatchUnion& d,
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
  Value* result = Evaluate(x);
  for (int i = 0; i < n; i++) {
    names[d.elements[i]].pop_back();
  }
  return result;
}

Value* Interpreter::TryAlternative(Value* v, const core::Integer& i,
                                   const core::Expression& x) {
  if (v->GetType() != Value::Type::kInt64 || v->AsInt64() != i.value) {
    return nullptr;
  }
  return Evaluate(x);
}

Value* Interpreter::TryAlternative(Value* v, const core::Character& c,
                                   const core::Expression& x) {
  if (v->GetType() != Value::Type::kChar || v->AsChar() != c.value) {
    return nullptr;
  }
  return Evaluate(x);
}

void Interpreter::Run(const core::Expression& program) {
  GCPtr<Lazy> output =
      Allocate<Lazy>(Allocate<Apply>(Allocate<Lazy>(Wrap(Evaluate(program))),
                                     Allocate<Lazy>(Allocate<Read>())));
  while (true) {
    Value* v = output->Get(*this);
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
    Value* head = u.elements[0]->Get(*this);
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
