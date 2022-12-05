#include "interpreter.hpp"

#include "debug_output.hpp"

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

  void Mark() {
    if (reachable) return;
    reachable = true;
    MarkChildren();
  }

  virtual void MarkChildren() = 0;

  bool reachable;
};

struct Value;
struct Interpreter;

struct Thunk : Node {
  virtual Value* Run(Interpreter& interpreter) = 0;
};

class Lazy final : public Node {
 public:
  Lazy(Value* value) : has_value(true), value(value) {}
  Lazy(Thunk* thunk) : has_value(false), thunk(thunk) {}
  Value* Get(Interpreter& interpreter) {
    if (!has_value) {
      // Evaluation of the thunk relies on evaluating itself: the expression
      // diverges without reaching weak head normal form.
      if (computing) throw std::runtime_error("divergence");
      computing = true;
      value = thunk->Run(interpreter);
      has_value = true;
      computing = false;
    }
    return value;
  }
  void MarkChildren() override;
 private:
  bool has_value;
  bool computing = false;
  union {
    Value* value;
    Thunk* thunk;
  };
};

struct Interpreter {
  template <std::derived_from<Node> T, typename... Args>
  requires std::constructible_from<T, Args...>
  T* Allocate(Args&&... args);

  void CollectGarbage();

  std::map<core::Identifier, Lazy*> ResolveImpl(std::set<core::Identifier>&,
                                                const core::Builtin& x);
  std::map<core::Identifier, Lazy*> ResolveImpl(std::set<core::Identifier>&,
                                                const core::Identifier& x);
  std::map<core::Identifier, Lazy*> ResolveImpl(std::set<core::Identifier>&,
                                                const core::Integer& x);
  std::map<core::Identifier, Lazy*> ResolveImpl(std::set<core::Identifier>&,
                                                const core::Character& x);
  std::map<core::Identifier, Lazy*> ResolveImpl(std::set<core::Identifier>&,
                                                const core::String& x);
  std::map<core::Identifier, Lazy*> ResolveImpl(std::set<core::Identifier>&,
                                                const core::Cons& x);
  std::map<core::Identifier, Lazy*> ResolveImpl(std::set<core::Identifier>&,
                                                const core::Apply& x);
  std::map<core::Identifier, Lazy*> ResolveImpl(std::set<core::Identifier>&,
                                                const core::Lambda& x);
  std::map<core::Identifier, Lazy*> ResolveImpl(std::set<core::Identifier>&,
                                                const core::Let& x);
  std::map<core::Identifier, Lazy*> ResolveImpl(std::set<core::Identifier>&,
                                                const core::LetRecursive& x);
  std::map<core::Identifier, Lazy*> ResolveImpl(
      std::set<core::Identifier>&, const core::Case::Alternative& x);
  std::map<core::Identifier, Lazy*> ResolveImpl(std::set<core::Identifier>&,
                                                const core::Case& x);
  std::map<core::Identifier, Lazy*> Resolve(std::set<core::Identifier>& bound,
                                            const core::Expression& x);
  std::map<core::Identifier, Lazy*> Resolve(const core::Expression& x);

  std::set<core::Identifier> GetBindingsImpl(const core::Builtin&);
  std::set<core::Identifier> GetBindingsImpl(const core::Identifier&);
  std::set<core::Identifier> GetBindingsImpl(const core::Decons&);
  std::set<core::Identifier> GetBindingsImpl(const core::Integer&);
  std::set<core::Identifier> GetBindingsImpl(const core::Character&);
  std::set<core::Identifier> GetBindings(const core::Pattern&);

  Lazy* LazyEvaluate(const core::Builtin& x);
  Lazy* LazyEvaluate(const core::Identifier& x);
  Lazy* LazyEvaluate(const core::Integer& x);
  Lazy* LazyEvaluate(const core::Character& x);
  Lazy* LazyEvaluate(const core::String& x);
  Lazy* LazyEvaluate(const core::Cons& x);
  Lazy* LazyEvaluate(const core::Apply& x);
  Lazy* LazyEvaluate(const core::Lambda& x);
  Lazy* LazyEvaluate(const core::Let& x);
  Lazy* LazyEvaluate(const core::LetRecursive& x);
  Lazy* LazyEvaluate(const core::Case& x);
  Lazy* LazyEvaluate(const core::Expression& x);

  Value* TryAlternative(Value*, const core::Case::Alternative& x);
  Value* TryAlternative(Value*, const core::Builtin&,
                        const core::Expression& x);
  Value* TryAlternative(Value*, const core::Identifier&,
                        const core::Expression& x);
  Value* TryAlternative(Value*, const core::Decons&, const core::Expression& x);
  Value* TryAlternative(Value*, const core::Integer&,
                        const core::Expression& x);
  Value* TryAlternative(Value*, const core::Character&,
                        const core::Expression& x);

  void Run(const core::Expression& program);

  std::vector<std::unique_ptr<Node>> heap;
  int collect_at_size = 128000000;
  std::map<core::Identifier, std::vector<Lazy*>> names;
  std::vector<Lazy*> stack;
};

struct ConsData {
  Lazy* head;
  Lazy* tail;
};

struct Value : public Node {
  enum class Type {
    kBool,
    kInt64,
    kChar,
    kCons,
    kNil,
    kLambda,
  };

  virtual Type GetType() const = 0;

  bool AsBool() const;
  std::int64_t AsInt64() const;
  char AsChar() const;
  const ConsData& AsCons() const;
  void Enter(Interpreter&);
};

struct Bool final : public Value {
  Bool(bool value) : value(value) {}
  Type GetType() const override { return Type::kBool; }
  void MarkChildren() override {}
  bool value;
};

struct Int64 final : public Value {
  Int64(std::int64_t value) : value(value) {}
  Type GetType() const override { return Type::kInt64; }
  void MarkChildren() override {}
  std::int64_t value;
};

struct Char final : public Value {
  Char(char value) : value(value) {}
  Type GetType() const override { return Type::kChar; }
  void MarkChildren() override {}
  char value;
};

struct Cons final : public Value {
  Cons(Lazy* head, Lazy* tail) : data{.head = head, .tail = tail} {}
  Type GetType() const override { return Type::kCons; }
  void MarkChildren() override {
    data.head->Mark();
    data.tail->Mark();
  }
  ConsData data;
};

struct Nil final : public Value {
  Type GetType() const override { return Type::kNil; }
  void MarkChildren() override {}
};

struct Closure : public Thunk {
  Closure(std::map<core::Identifier, Lazy*> captures)
      : captures(std::move(captures)) {}
  void MarkChildren() override {
    for (const auto& [id, value] : captures) value->Mark();
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
  std::map<core::Identifier, Lazy*> captures;
};

struct Let final : public Closure {
  Let(Interpreter& interpreter, const core::Let& definition)
      : Closure(interpreter.Resolve(definition)),
        definition(definition) {}
  Value* RunBody(Interpreter& interpreter) override {
    interpreter.names[definition.binding.name].push_back(
        interpreter.LazyEvaluate(definition.binding.result));
    Value* result =
        interpreter.LazyEvaluate(definition.result)->Get(interpreter);
    interpreter.names[definition.binding.name].pop_back();
    return result;
  }
  const core::Let& definition;
};

struct Error final : public Thunk {
  Error(std::string message) : message(std::move(message)) {}
  void MarkChildren() override {}
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
      Lazy* value = interpreter.LazyEvaluate(definition.bindings[i].result);
      if (holes[i] == value) {
        *holes[i] = Lazy(interpreter.Allocate<Error>("divergence"));
      } else {
        *holes[i] = *value;
      }
    }
    Value* result =
        interpreter.LazyEvaluate(definition.result)->Get(interpreter);
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
    std::vector<Lazy*> values;
    Value* v = interpreter.LazyEvaluate(definition.value)->Get(interpreter);
    for (const auto& alternative : definition.alternatives) {
      if (Value* r = interpreter.TryAlternative(v, alternative)) return r;
    }
    throw std::runtime_error("non-exhaustative case");
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
  void MarkChildren() override {}
  const int arity;
};

template <int n>
struct NativeFunction : public NativeFunctionBase {
  NativeFunction() : NativeFunctionBase(n) {}
  void Enter(Interpreter& interpreter) final {
    if (interpreter.stack.size() < n) {
      throw std::logic_error("invoking native function with too few arguments");
    }
    Lazy* result = interpreter.Allocate<Lazy>(
        Run(interpreter, std::span(interpreter.stack).template last<n>()));
    interpreter.stack.resize(interpreter.stack.size() - n + 1);
    interpreter.stack.back() = result;
  }
  virtual Value* Run(Interpreter& interpreter, std::span<Lazy*, n> args) = 0;
};

struct Add : public NativeFunction<2> {
  Value* Run(Interpreter& interpreter, std::span<Lazy*, 2> args) override {
    Value* l = args[0]->Get(interpreter);
    Value* r = args[1]->Get(interpreter);
    return interpreter.Allocate<Int64>(l->AsInt64() + r->AsInt64());
  }
};

struct ShowInt : public NativeFunction<1> {
  Value* Run(Interpreter& interpreter, std::span<Lazy*, 1> args) override {
    const std::int64_t value = args[0]->Get(interpreter)->AsInt64();
    std::string text = std::to_string(value);
    Value* result = interpreter.Allocate<Nil>();
    for (int i = text.size() - 1; i >= 0; i--) {
      result = interpreter.Allocate<Cons>(
          interpreter.Allocate<Lazy>(interpreter.Allocate<Char>(text[i])),
          interpreter.Allocate<Lazy>(result));
    }
    return result;
  }
};

struct ReadInt : public NativeFunction<1> {
  Value* Run(Interpreter& interpreter, std::span<Lazy*, 1> args) override {
    std::string text;
    Lazy* list = args[0];
    while (true) {
      Value* v = list->Get(interpreter);
      if (v->GetType() == Value::Type::kNil) break;
      auto& cons = v->AsCons();
      Value* head = cons.head->Get(interpreter);
      text.push_back(head->AsChar());
      list = cons.tail;
    }
    std::int64_t value;
    auto [p, error] = std::from_chars(text.data(), text.data() + text.size(),
                                      value);
    if (error != std::errc()) throw std::runtime_error("bad int in string");
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
  void MarkChildren() override {
    f->Mark();
    for (const auto& b : bound) b->Mark();
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
  void MarkChildren() override {
    for (const auto& [id, value] : captures) value->Mark();
  }
  const core::Lambda& definition;
  std::map<core::Identifier, Lazy*> captures;
};

struct Apply final : public Thunk {
  Apply(Lazy* f, Lazy* x) : f(f), x(x) {}
  void MarkChildren() override {
    f->Mark();
    x->Mark();
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
  void MarkChildren() override {}
  Value* Run(Interpreter& interpreter) override {
    char c;
    if (std::cin.get(c)) {
      return interpreter.Allocate<Cons>(
          interpreter.Allocate<Lazy>(interpreter.Allocate<Char>(c)),
          interpreter.Allocate<Lazy>(this));
    } else {
      return interpreter.Allocate<Nil>();
    }
  }
};

struct StringTail final : public Thunk {
  StringTail(std::string_view tail) : tail(tail) {}
  Value* Run(Interpreter& interpreter) override {
    if (tail.empty()) {
      return interpreter.Allocate<Nil>();
    } else {
      const char head = tail.front();
      tail.remove_prefix(1);
      return interpreter.Allocate<Cons>(
          interpreter.Allocate<Lazy>(interpreter.Allocate<Char>(head)),
          interpreter.Allocate<Lazy>(this));
    }
  }
  void MarkChildren() override {}
  std::string_view tail;
};

void Lazy::MarkChildren() {
  if (has_value) {
    value->Mark();
  } else {
    thunk->Mark();
  }
}

bool Value::AsBool() const {
  if (GetType() != Type::kBool) throw std::runtime_error("not a bool");
  return static_cast<const Bool*>(this)->value;
}

std::int64_t Value::AsInt64() const {
  if (GetType() != Type::kInt64) throw std::runtime_error("not an int64");
  return static_cast<const Int64*>(this)->value;
}

char Value::AsChar() const {
  if (GetType() != Type::kChar) throw std::runtime_error("not a char");
  return static_cast<const Char*>(this)->value;
}

const ConsData& Value::AsCons() const {
  if (GetType() != Type::kCons) throw std::runtime_error("not a cons node");
  return static_cast<const Cons*>(this)->data;
}

void Value::Enter(Interpreter& interpreter) {
  if (GetType() != Type::kLambda) throw std::runtime_error("not a lambda");
  return static_cast<Lambda*>(this)->Enter(interpreter);
}

template <std::derived_from<Node> T, typename... Args>
requires std::constructible_from<T, Args...>
T* Interpreter::Allocate(Args&&... args) {
  if (int(heap.size()) >= collect_at_size) CollectGarbage();
  auto u = std::make_unique<T>(std::forward<Args>(args)...);
  T* p = u.get();
  heap.push_back(std::move(u));
  return p;
}

void Interpreter::CollectGarbage() {
  for (auto& node : heap) node->reachable = false;
  for (auto& [name, nodes] : names) {
    for (auto& node : nodes) node->Mark();
  }
  std::erase_if(heap, [](const auto& node) { return !node->reachable; });
  collect_at_size = std::max<int>(128, heap.size());
}

/*
Value* Interpreter::StrictEvaluate(const syntax::Identifier& x) {
  return Allocate<Int64>(x.value);
}

Value* Interpreter::StrictEvaluate(const syntax::Integer& x) {
  return Allocate<Int64>(x.value);
}

Value* Interpreter::StrictEvaluate(const syntax::Character& x) {
  return Allocate<Char>(x.value);
}

Value* Interpreter::StrictEvaluate(const syntax::LessThan& x) {
  Value* va = StrictEvaluate(x.a);
  Value* vb = StrictEvaluate(x.b);
  const Value::Type type = va->GetType();
  if (type != vb->GetType()) {
    throw std::runtime_error("cannot compare dissimilar types");
  }
  if (type == Value::Type::kInt64) {
    return Allocate<Bool>(va->AsInt64() < vb->AsInt64());
  } else if (type == Value::Type::kChar) {
    return Allocate<Bool>(va->AsChar() < vb->AsChar());
  } else {
    throw std::runtime_error("cannot compare this type");
  }
}

Value* Interpreter::StrictEvaluate(const syntax::Add& x) {
  Value* va = StrictEvaluate(x.a);
  Value* vb = StrictEvaluate(x.b);
  if (va->GetType() != Value::Type::kInt64 ||
      vb->GetType() != Value::Type::kInt64) {
    throw std::runtime_error("cannot add types");
  }
  return Allocate<Int64>(va->AsInt64() + vb->AsInt64());
}

Value* Interpreter::StrictEvaluate(const syntax::Cons& x) {
  return Allocate<Cons>(LazyEvaluate(x.head), LazyEvaluate(x.tail));
}

Value* Interpreter::StrictEvaluate(const syntax::Apply& x) {
  throw std::runtime_error(":(");
}

Value* Interpreter::StrictEvaluate(const syntax::Compose& x) {
  throw std::runtime_error(":(");
}

Value* Interpreter::StrictEvaluate(const syntax::Case& x) {
  Value* v = StrictEvaluate(x.value);
  for (const auto& alternative : x.alternatives) {
    if (MatchesPattern(v, alternative.pattern)) {
      return StrictEvaluate(alternative.value);
    }
  }
  throw std::runtime_error("non-exhaustative case");
}

Value* Interpreter::StrictEvaluate(const syntax::If& x) {
  Value* c = StrictEvaluate(x.condition);
  if (c->GetType() != Value::Type::kBool) {
    throw std::runtime_error("not a bool");
  }
  if (c->AsBool()) {
    return StrictEvaluate(x.then_branch);
  } else {
    return StrictEvaluate(x.else_branch);
  }
}

Value* Interpreter::StrictEvaluate(const syntax::Expression& x) {
  return std::visit([this](const auto& x) { return StrictEvaluate(x); },
                    x->value);
}
*/

std::map<core::Identifier, Lazy*> Interpreter::ResolveImpl(
    std::set<core::Identifier>&, const core::Builtin&) {
  return {};
}

std::map<core::Identifier, Lazy*> Interpreter::ResolveImpl(
    std::set<core::Identifier>& bound, const core::Identifier& x) {
  std::map<core::Identifier, Lazy*> result;
  if (!bound.contains(x)) result[x] = names.at(x).back();
  return result;
}

std::map<core::Identifier, Lazy*> Interpreter::ResolveImpl(
    std::set<core::Identifier>&, const core::Integer&) {
  return {};
}

std::map<core::Identifier, Lazy*> Interpreter::ResolveImpl(
    std::set<core::Identifier>&, const core::Character&) {
  return {};
}

std::map<core::Identifier, Lazy*> Interpreter::ResolveImpl(
    std::set<core::Identifier>&, const core::String&) {
  return {};
}

std::map<core::Identifier, Lazy*> Interpreter::ResolveImpl(
    std::set<core::Identifier>& bound, const core::Cons& x) {
  std::map<core::Identifier, Lazy*> result = Resolve(bound, x.head);
  result.merge(Resolve(bound, x.tail));
  return result;
}

std::map<core::Identifier, Lazy*> Interpreter::ResolveImpl(
    std::set<core::Identifier>& bound, const core::Apply& x) {
  std::map<core::Identifier, Lazy*> result = Resolve(bound, x.f);
  result.merge(Resolve(bound, x.x));
  return result;
}

std::map<core::Identifier, Lazy*> Interpreter::ResolveImpl(
    std::set<core::Identifier>& bound, const core::Lambda& x) {
  auto [i, is_new] = bound.emplace(x.parameter);
  std::map<core::Identifier, Lazy*> result = Resolve(bound, x.result);
  if (is_new) bound.erase(i);
  return result;
}

std::map<core::Identifier, Lazy*> Interpreter::ResolveImpl(
    std::set<core::Identifier>& bound, const core::Let& x) {
  std::map<core::Identifier, Lazy*> result = Resolve(bound, x.binding.result);
  auto [i, is_new] = bound.emplace(x.binding.name);
  result.merge(Resolve(bound, x.result));
  if (is_new) bound.erase(i);
  return result;
}

std::map<core::Identifier, Lazy*> Interpreter::ResolveImpl(
    std::set<core::Identifier>& bound, const core::LetRecursive& x) {
  std::set<core::Identifier> newly_bound;
  for (const auto& binding : x.bindings) {
    auto [i, is_new] = bound.emplace(binding.name);
    if (is_new) newly_bound.insert(binding.name);
  }
  std::map<core::Identifier, Lazy*> result;
  for (const auto& binding : x.bindings) {
    result.merge(Resolve(bound, binding.result));
  }
  result.merge(Resolve(bound, x.result));
  for (const auto& id : newly_bound) bound.erase(id);
  return result;
}

std::map<core::Identifier, Lazy*> Interpreter::ResolveImpl(
    std::set<core::Identifier>& bound, const core::Case::Alternative& x) {
  std::set<core::Identifier> bindings = GetBindings(x.pattern);
  std::set<core::Identifier> newly_bound;
  for (const auto& binding : bindings) {
    auto [i, is_new] = bound.emplace(binding);
    if (is_new) newly_bound.insert(binding);
  }
  std::map<core::Identifier, Lazy*> result = Resolve(bound, x.value);
  for (const auto& id : newly_bound) bound.erase(id);
  return result;
}

std::map<core::Identifier, Lazy*> Interpreter::ResolveImpl(
    std::set<core::Identifier>& bound, const core::Case& x) {
  std::map<core::Identifier, Lazy*> result = Resolve(bound, x.value);
  for (const auto& alternative : x.alternatives) {
    result.merge(ResolveImpl(bound, alternative));
  }
  return result;
}

std::map<core::Identifier, Lazy*> Interpreter::Resolve(
    std::set<core::Identifier>& bound, const core::Expression& x) {
  return std::visit(
      [this, &bound](const auto& x) { return ResolveImpl(bound, x); },
      x->value);
}

std::map<core::Identifier, Lazy*> Interpreter::Resolve(
    const core::Expression& x) {
  std::set<core::Identifier> bound;
  return Resolve(bound, x);
}

std::set<core::Identifier> Interpreter::GetBindingsImpl(const core::Builtin&) {
  return {};
}

std::set<core::Identifier> Interpreter::GetBindingsImpl(
    const core::Identifier& x) {
  return {x};
}

std::set<core::Identifier> Interpreter::GetBindingsImpl(const core::Decons& x) {
  return {x.head, x.tail};
}

std::set<core::Identifier> Interpreter::GetBindingsImpl(const core::Integer&) {
  return {};
}

std::set<core::Identifier> Interpreter::GetBindingsImpl(
    const core::Character&) {
  return {};
}

std::set<core::Identifier> Interpreter::GetBindings(const core::Pattern& x) {
  return std::visit([this](const auto& x) { return GetBindingsImpl(x); },
                    x->value);
}

Lazy* Interpreter::LazyEvaluate(const core::Builtin& x) {
  switch (x) {
    case core::Builtin::kTrue:
      return Allocate<Lazy>(Allocate<Bool>(true));
    case core::Builtin::kFalse:
      return Allocate<Lazy>(Allocate<Bool>(false));
    case core::Builtin::kNil:
      return Allocate<Lazy>(Allocate<Nil>());
    case core::Builtin::kAdd:
      return Allocate<Lazy>(Allocate<NativeClosure>(Allocate<Add>()));
    case core::Builtin::kShowInt:
      return Allocate<Lazy>(Allocate<NativeClosure>(Allocate<ShowInt>()));
    case core::Builtin::kReadInt:
      return Allocate<Lazy>(Allocate<NativeClosure>(Allocate<ReadInt>()));
    default:
      throw std::runtime_error(StrCat("unimplemented builtin: ", x));
  }
}

Lazy* Interpreter::LazyEvaluate(const core::Identifier& identifier) {
  return names.at(identifier).back();
}

Lazy* Interpreter::LazyEvaluate(const core::Integer& x) {
  return Allocate<Lazy>(Allocate<Int64>(x.value));
}

Lazy* Interpreter::LazyEvaluate(const core::Character& x) {
  return Allocate<Lazy>(Allocate<Char>(x.value));
}

Lazy* Interpreter::LazyEvaluate(const core::String& x) {
  return Allocate<Lazy>(Allocate<StringTail>(x.value));
}

Lazy* Interpreter::LazyEvaluate(const core::Cons& x) {
  return Allocate<Lazy>(
      Allocate<Cons>(LazyEvaluate(x.head), LazyEvaluate(x.tail)));
}

Lazy* Interpreter::LazyEvaluate(const core::Apply& x) {
  return Allocate<Lazy>(Allocate<Apply>(LazyEvaluate(x.f), LazyEvaluate(x.x)));
}

Lazy* Interpreter::LazyEvaluate(const core::Lambda& x) {
  return Allocate<Lazy>(Allocate<UserLambda>(*this, x));
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

Value* Interpreter::TryAlternative(Value* v, const core::Builtin& b,
                                   const core::Expression& x) {
  switch (b) {
    case core::Builtin::kNil:
      if (v->GetType() == Value::Type::kNil) {
        return LazyEvaluate(x)->Get(*this);
      } else {
        return nullptr;
      }
    case core::Builtin::kTrue:
      if (v->GetType() == Value::Type::kBool && v->AsBool()) {
        return LazyEvaluate(x)->Get(*this);
      } else {
        return nullptr;
      }
    case core::Builtin::kFalse:
      if (v->GetType() == Value::Type::kBool && !v->AsBool()) {
        return LazyEvaluate(x)->Get(*this);
      } else {
        return nullptr;
      }
    default:
      throw std::runtime_error("unsupported pattern");
  }
}

Value* Interpreter::TryAlternative(Value* v, const core::Identifier& i,
                                   const core::Expression& x) {
  names[i].push_back(Allocate<Lazy>(v));
  Lazy* l = LazyEvaluate(x);
  names[i].pop_back();
  return l->Get(*this);
}

Value* Interpreter::TryAlternative(Value* v, const core::Decons& d,
                                   const core::Expression& x) {
  if (v->GetType() != Value::Type::kCons) return nullptr;
  const auto& cons = v->AsCons();
  names[d.head].push_back(cons.head);
  names[d.tail].push_back(cons.tail);
  Lazy* l = LazyEvaluate(x);
  names[d.head].pop_back();
  names[d.tail].pop_back();
  return l->Get(*this);
}

Value* Interpreter::TryAlternative(Value* v, const core::Integer& i,
                                   const core::Expression& x) {
  if (v->GetType() != Value::Type::kInt64 || v->AsInt64() != i.value) {
    return nullptr;
  }
  return LazyEvaluate(x)->Get(*this);
}

Value* Interpreter::TryAlternative(Value* v, const core::Character& c,
                                   const core::Expression& x) {
  if (v->GetType() != Value::Type::kChar || v->AsChar() != c.value) {
    return nullptr;
  }
  return LazyEvaluate(x)->Get(*this);
}

void Interpreter::Run(const core::Expression& program) {
  Lazy* output = Allocate<Lazy>(
      Allocate<Apply>(LazyEvaluate(program), Allocate<Lazy>(Allocate<Read>())));
  try {
    while (true) {
      Value* v = output->Get(*this);
      if (v->GetType() == Value::Type::kCons) {
        auto& cons = v->AsCons();
        std::cout << cons.head->Get(*this)->AsChar();
        output = cons.tail;
      } else if (v->GetType() == Value::Type::kNil) {
        break;
      } else {
        throw std::runtime_error("type error in output");
      }
    }
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
  }
}

}  // namespace

void Run(const core::Expression& program) {
  Interpreter interpreter;
  interpreter.Run(program);
}

}  // namespace aoc2022
