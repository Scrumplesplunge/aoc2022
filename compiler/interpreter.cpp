#include "interpreter.hpp"

#include <map>
#include <set>
#include <span>
#include <iostream>
#include <sstream>

namespace aoc2022 {
namespace {

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

struct Thunk : Node {
  virtual Value* Run() = 0;
};

class Lazy final : public Node {
 public:
  Lazy(Value* value) : has_value(true), value(value) {}
  Lazy(Thunk* thunk) : has_value(false), thunk(thunk) {}
  Value* Get() {
    if (!has_value) {
      // Evaluation of the thunk relies on evaluating itself: the expression
      // diverges without reaching weak head normal form.
      if (computing) throw std::runtime_error("divergence");
      computing = true;
      value = thunk->Run();
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
  void Enter();
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

struct Let final : public Thunk {
  Let(Interpreter* interpreter, const core::Let& definition)
      : interpreter(interpreter),
        definition(definition),
        captures(interpreter->Resolve(definition)) {}
  void MarkChildren() override {
    for (const auto& [id, value] : captures) value->Mark();
  }
  Value* Run() override {
    for (const auto& [id, value] : captures) {
      interpreter->names[id].push_back(value);
    }
    interpreter->names[definition.binding.name].push_back(
        interpreter->LazyEvaluate(definition.binding.result));
    Value* result = interpreter->LazyEvaluate(definition.result)->Get();
    interpreter->names[definition.binding.name].pop_back();
    for (const auto& [id, value] : captures) {
      interpreter->names[id].pop_back();
    }
    return result;
  }
  Interpreter* interpreter;
  const core::Let& definition;
  std::map<core::Identifier, Lazy*> captures;
};

struct Error final : public Thunk {
  Error(std::string message) : message(std::move(message)) {}
  void MarkChildren() override {}
  Value* Run() override {
    throw std::runtime_error(message);
  }
  std::string message;
};

struct LetRecursive final : public Thunk {
  LetRecursive(Interpreter* interpreter, const core::LetRecursive& definition)
      : interpreter(interpreter),
        definition(definition),
        captures(interpreter->Resolve(definition)) {}
  void MarkChildren() override {
    for (const auto& [id, value] : captures) value->Mark();
  }
  Value* Run() override {
    for (const auto& [id, value] : captures) {
      interpreter->names[id].push_back(value);
    }
    std::vector<Lazy*> holes;
    for (const auto& [id, value] : definition.bindings) {
      Lazy* l = interpreter->Allocate<Lazy>(
          interpreter->Allocate<Error>("this should never be executed"));
      interpreter->names[id].push_back(l);
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
      Lazy* value = interpreter->LazyEvaluate(definition.bindings[i].result);
      if (holes[i] == value) {
        *holes[i] = Lazy(interpreter->Allocate<Error>("divergence"));
      } else {
        *holes[i] = *value;
      }
    }
    Value* result = interpreter->LazyEvaluate(definition.result)->Get();
    for (const auto& [id, value] : definition.bindings) {
      interpreter->names[id].pop_back();
    }
    for (const auto& [id, value] : captures) {
      interpreter->names[id].pop_back();
    }
    return result;
  }
  Interpreter* interpreter;
  const core::LetRecursive& definition;
  std::map<core::Identifier, Lazy*> captures;
};

struct Lambda final : public Value {
  Lambda(Interpreter* interpreter, const core::Lambda& definition)
      : interpreter(interpreter), definition(definition),
        captures(interpreter->Resolve(definition)) {}
  void Enter() {
    for (const auto& [id, value] : captures) {
      interpreter->names[id].push_back(value);
    }
    Lazy* v = interpreter->stack.back();
    interpreter->names[definition.parameter].push_back(v);
    interpreter->stack.back() = interpreter->LazyEvaluate(definition.result);
    interpreter->names[definition.parameter].pop_back();
    for (const auto& [id, value] : captures) {
      interpreter->names[id].pop_back();
    }
  }
  Type GetType() const override { return Type::kLambda; }
  void MarkChildren() override {
    for (const auto& [id, value] : captures) value->Mark();
  }
  Interpreter* interpreter;
  const core::Lambda& definition;
  std::map<core::Identifier, Lazy*> captures;
};

struct Apply final : public Thunk {
  Apply(Interpreter* interpreter, Lazy* f, Lazy* x)
      : interpreter(interpreter), f(f), x(x) {}
  void MarkChildren() override {
    f->Mark();
    x->Mark();
  }
  Value* Run() override {
    interpreter->stack.push_back(x);
    f->Get()->Enter();
    Value* v = interpreter->stack.back()->Get();
    interpreter->stack.pop_back();
    return v;
  }
  Interpreter* interpreter;
  Lazy* f;
  Lazy* x;
};

struct Read final : public Thunk {
  Read(Interpreter* interpreter) : interpreter(interpreter) {}
  void MarkChildren() override {}
  Value* Run() override {
    char c;
    if (std::cin.get(c)) {
      return interpreter->Allocate<Cons>(
          interpreter->Allocate<Lazy>(interpreter->Allocate<Char>(c)),
          interpreter->Allocate<Lazy>(this));
    } else {
      return interpreter->Allocate<Nil>();
    }
  }
  Interpreter* interpreter;
};

struct StringTail final : public Thunk {
  StringTail(Interpreter* interpreter, std::string_view tail)
      : interpreter(interpreter), tail(tail) {}
  Value* Run() override {
    if (tail.empty()) {
      return interpreter->Allocate<Nil>();
    } else {
      const char head = tail.front();
      tail.remove_prefix(1);
      return interpreter->Allocate<Cons>(
          interpreter->Allocate<Lazy>(interpreter->Allocate<Char>(head)),
          interpreter->Allocate<Lazy>(this));
    }
  }
  void MarkChildren() override {}
  Interpreter* interpreter;
  std::string_view tail;
};

void Lazy::MarkChildren() {
  if (has_value) {
    value->Mark();
  } else {
    thunk->Mark();
  }
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

void Value::Enter() {
  if (GetType() != Type::kLambda) throw std::runtime_error("not a lambda");
  return static_cast<Lambda*>(this)->Enter();
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
    default:
      throw std::runtime_error("unimplemented");
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
  return Allocate<Lazy>(Allocate<StringTail>(this, x.value));
}

Lazy* Interpreter::LazyEvaluate(const core::Cons& x) {
  return Allocate<Lazy>(
      Allocate<Cons>(LazyEvaluate(x.head), LazyEvaluate(x.tail)));
}

Lazy* Interpreter::LazyEvaluate(const core::Apply& x) {
  return Allocate<Lazy>(
      Allocate<Apply>(this, LazyEvaluate(x.f), LazyEvaluate(x.x)));
}

Lazy* Interpreter::LazyEvaluate(const core::Lambda& x) {
  return Allocate<Lazy>(Allocate<Lambda>(this, x));
}

Lazy* Interpreter::LazyEvaluate(const core::Let& x) {
  return Allocate<Lazy>(Allocate<Let>(this, x));
}

Lazy* Interpreter::LazyEvaluate(const core::LetRecursive& x) {
  return Allocate<Lazy>(Allocate<LetRecursive>(this, x));
}

Lazy* Interpreter::LazyEvaluate(const core::Case& x) {
  throw std::runtime_error("not implemented");
  // std::vector<Lazy*> values;
  // Value* v = LazyEvaluate(x.value)->Get();
  // for (const auto& alternative : x.alternatives) {
  //   if (MatchesPattern(v, alternative.pattern)) {
  //     return StrictEvaluate(alternative.value);
  //   }
  // }
  // throw std::runtime_error("non-exhaustative case");
}

Lazy* Interpreter::LazyEvaluate(const core::Expression& x) {
  return std::visit([this](const auto& x) { return LazyEvaluate(x); },
                    x->value);
}

void Interpreter::Run(const core::Expression& program) {
  Lazy* output = Allocate<Lazy>(Allocate<Apply>(
      this, LazyEvaluate(program), Allocate<Lazy>(Allocate<Read>(this))));
  try {
    while (true) {
      Value* v = output->Get();
      if (v->GetType() == Value::Type::kCons) {
        auto& cons = v->AsCons();
        std::cout << cons.head->Get()->AsChar();
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
