#include "checker.hpp"

#include "debug_output.hpp"

#include <cassert>
#include <map>
#include <optional>
#include <set>
#include <sstream>

namespace aoc2022 {
namespace {

constexpr Source kBuiltin = {.filename = "builtin", .contents = "\n"};
constexpr Location kBuiltinLocation = {&kBuiltin, 1, 1};

// bool IsUpper(char c) { return 'A' <= c && c <= 'Z'; }
// bool IsTypeName(std::string_view name) {
//   return !name.empty() && IsUpper(name.front());
// }

template <typename... Args>
std::string MakeMessage(Location location, const Args&... args) {
  std::ostringstream message;
  message << "input:" << location.line << ":" << location.column << ": error: ";
  (message << ... << args);
  return message.str();
}

class Error : public std::runtime_error {
 public:
  template <typename... Args>
  Error(Location location, const Args&... args)
      : runtime_error(MakeMessage(location, args...)) {}
};

struct Checker {
  struct Name {
    const core::Expression& AsExpression(Location location) {
      if (auto* v = std::get_if<core::Expression>(&value)) {
        return *v;
      } else {
        throw Error(location, "type used in value context");
      }
    }

    const core::Type& AsType(Location location) {
      if (auto* t = std::get_if<core::Type>(&value)) {
        return *t;
      } else {
        throw Error(location, "value used in type context");
      }
    }

    Location location;
    std::string name;
    std::variant<core::Expression, core::Type> value;
  };

  Name* TryLookup(std::string_view name) {
    for (int i = names.size() - 1; i >= 0; i--) {
      if (names[i].name == name) return &names[i];
    }
    return nullptr;
  }

  Name& Lookup(const syntax::Identifier& x) {
    Name* name = TryLookup(x.value);
    if (!name) throw Error(x.location, "use of undefined identifier");
    return *name;
  }

  bool TryUnify(const core::BuiltinType& l, const core::BuiltinType& r) {
    return l == r;
  }

  bool TryUnify(const core::TupleType& l, const core::TupleType& r) {
    if (l.elements.size() != r.elements.size()) return false;
    for (int i = 0, n = l.elements.size(); i < n; i++) {
      if (!TryUnify(l.elements[i], r.elements[i])) return false;
    }
    return true;
  }

  bool TryUnify(const core::TypeConstructor& l,
                const core::TypeConstructor& r) {
    if (l.id != r.id) return false;
    assert(l.parameters.size() == r.parameters.size());
    for (int i = 0, n = l.parameters.size(); i < n; i++) {
      if (!TryUnify(l.parameters[i], r.parameters[i])) return false;
    }
    return true;
  }

  bool TryUnify(const core::ApplyType& l, const core::ApplyType& r) {
    if (!TryUnify(l.constructor, r.constructor)) return false;
    assert(l.arguments.size() == r.arguments.size());
    for (int i = 0, n = l.arguments.size(); i < n; i++) {
      if (!TryUnify(l.arguments[i], r.arguments[i])) return false;
    }
    return true;
  }

  bool TryUnify(const core::ArrowType& l, const core::ArrowType& r) {
    return TryUnify(l.a, r.a) && TryUnify(l.b, r.b);
  }

  bool TryUnify(const core::TypeVariable& l, const core::TypeVariable& r) {
    if (l == r) return true;
    throw std::runtime_error("unimplemented");
  }

  bool TryUnify(const core::TypeVariable& v, const auto& x) {
    throw std::runtime_error("unimplemented");
  }

  bool TryUnify(const auto& x, const core::TypeVariable& v) {
    throw std::runtime_error("unimplemented");
  }

  bool TryUnify(const auto&, const auto&) { return false; }

  bool TryUnify(const core::Type& l, const core::Type& r) {
    return std::visit([this](const auto& l, const auto& r) {
      return TryUnify(l, r);
    }, l->value, r->value);
  }

  void Unify(Location location, const core::Type& l, const core::Type& r) {
    if (!TryUnify(l, r)) {
      throw Error(location, "cannot unify ", l, " and ", r);
    }
  }

  core::Expression Check(const syntax::Identifier& x) {
    return Lookup(x).AsExpression(x.location);
  }

  core::Integer Check(const syntax::Integer& x) {
    return core::Integer(x.value);
  }

  core::Character Check(const syntax::Character& x) {
    return core::Character(x.value);
  }

  core::Expression Check(const syntax::String& x) {
    const core::Type type = List(core::BuiltinType::kChar);
    core::Expression result = Nil(type);
    for (int i = x.value.size() - 1; i >= 0; i--) {
      result = Cons(x.location, core::Character(x.value[i]), std::move(result));
    }
    return result;
  }

  core::Expression Check(const syntax::List& x) {
    std::vector<core::Expression> elements;
    for (const auto& element : x.elements) {
      elements.push_back(Check(element));
    }
    core::Expression result =
        Nil(core::TypeVariable(NextIdentifier(x.location)));
    for (int i = elements.size() - 1; i >= 0; i--) {
      result = Cons(x.elements[i].location(), std::move(elements[i]),
                    std::move(result));
    }
    return result;
  }

  core::Expression Check(const syntax::Tuple& x) {
    std::vector<core::Expression> elements;
    std::vector<core::Type> element_types;
    for (const auto& element : x.elements) {
      elements.push_back(Check(element));
      element_types.push_back(elements.back().GetType());
    }
    return core::Tuple(core::TupleType(std::move(element_types)),
                       std::move(elements));
  }

  core::Expression Check(const syntax::BitwiseAnd& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    Unify(x.a.location(), a.GetType(), core::BuiltinType::kInt64);
    Unify(x.b.location(), b.GetType(), core::BuiltinType::kInt64);
    return core::Apply(core::BuiltinType::kInt64,
                       core::Apply(core::ArrowType(core::BuiltinType::kInt64,
                                                   core::BuiltinType::kInt64),
                                   core::Builtin::kBitwiseAnd, std::move(a)),
                       std::move(b));
  }

  core::Expression Check(const syntax::BitwiseOr& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    Unify(x.a.location(), a.GetType(), core::BuiltinType::kInt64);
    Unify(x.b.location(), b.GetType(), core::BuiltinType::kInt64);
    return core::Apply(core::BuiltinType::kInt64,
                       core::Apply(core::ArrowType(core::BuiltinType::kInt64,
                                                   core::BuiltinType::kInt64),
                                   core::Builtin::kBitwiseOr, std::move(a)),
                       std::move(b));
  }

  core::Expression Check(const syntax::Add& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    Unify(x.a.location(), a.GetType(), core::BuiltinType::kInt64);
    Unify(x.b.location(), b.GetType(), core::BuiltinType::kInt64);
    return core::Apply(core::BuiltinType::kInt64,
                       core::Apply(core::ArrowType(core::BuiltinType::kInt64,
                                                   core::BuiltinType::kInt64),
                                   core::Builtin::kAdd, std::move(a)),
                       std::move(b));
  }

  core::Expression Check(const syntax::Subtract& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    Unify(x.a.location(), a.GetType(), core::BuiltinType::kInt64);
    Unify(x.b.location(), b.GetType(), core::BuiltinType::kInt64);
    return core::Apply(core::BuiltinType::kInt64,
                       core::Apply(core::ArrowType(core::BuiltinType::kInt64,
                                                   core::BuiltinType::kInt64),
                                   core::Builtin::kSubtract, std::move(a)),
                       std::move(b));
  }

  core::Expression Check(const syntax::Multiply& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    Unify(x.a.location(), a.GetType(), core::BuiltinType::kInt64);
    Unify(x.b.location(), b.GetType(), core::BuiltinType::kInt64);
    return core::Apply(core::BuiltinType::kInt64,
                       core::Apply(core::ArrowType(core::BuiltinType::kInt64,
                                                   core::BuiltinType::kInt64),
                                   core::Builtin::kMultiply, std::move(a)),
                       std::move(b));
  }

  core::Expression Check(const syntax::Divide& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    Unify(x.a.location(), a.GetType(), core::BuiltinType::kInt64);
    Unify(x.b.location(), b.GetType(), core::BuiltinType::kInt64);
    return core::Apply(core::BuiltinType::kInt64,
                       core::Apply(core::ArrowType(core::BuiltinType::kInt64,
                                                   core::BuiltinType::kInt64),
                                   core::Builtin::kDivide, std::move(a)),
                       std::move(b));
  }

  core::Expression Check(const syntax::Modulo& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    Unify(x.a.location(), a.GetType(), core::BuiltinType::kInt64);
    Unify(x.b.location(), b.GetType(), core::BuiltinType::kInt64);
    return core::Apply(core::BuiltinType::kInt64,
                       core::Apply(core::ArrowType(core::BuiltinType::kInt64,
                                                   core::BuiltinType::kInt64),
                                   core::Builtin::kModulo, std::move(a)),
                       std::move(b));
  }

  core::Expression Check(const syntax::LessThan& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    core::Type argument_type = a.GetType();
    Unify(x.location, b.GetType(), argument_type);
    return core::Apply(
        bool_type,
        core::Apply(core::ArrowType(argument_type, bool_type),
                    core::Builtin::kLessThan, std::move(a)),
        std::move(b));
  }

  core::Expression Check(const syntax::LessOrEqual& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    core::Type argument_type = a.GetType();
    Unify(x.location, b.GetType(), argument_type);
    // a <= b -> not (b < a)
    return core::Apply(
        bool_type, core::Builtin::kNot,
        core::Apply(bool_type,
                    core::Apply(core::ArrowType(argument_type, bool_type),
                                core::Builtin::kLessThan, std::move(a)),
                    std::move(b)));
  }

  core::Expression Check(const syntax::GreaterThan& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    core::Type argument_type = a.GetType();
    Unify(x.location, b.GetType(), argument_type);
    // a > b -> b < a
    return core::Apply(
        bool_type,
        core::Apply(core::ArrowType(argument_type, bool_type),
                    core::Builtin::kLessThan, std::move(b)),
        std::move(a));
  }

  core::Expression Check(const syntax::GreaterOrEqual& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    core::Type argument_type = a.GetType();
    Unify(x.location, b.GetType(), argument_type);
    // a >= b -> not (a < b)
    return core::Apply(
        bool_type, core::Builtin::kNot,
        core::Apply(bool_type,
                    core::Apply(core::ArrowType(argument_type, bool_type),
                                core::Builtin::kLessThan, std::move(a)),
                    std::move(b)));
  }

  core::Expression Check(const syntax::Equal& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    core::Type argument_type = a.GetType();
    Unify(x.location, b.GetType(), argument_type);
    return core::Apply(
        bool_type,
        core::Apply(core::ArrowType(argument_type, bool_type),
                    core::Builtin::kEqual, std::move(a)),
        std::move(b));
  }

  core::Expression Check(const syntax::NotEqual& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    core::Type argument_type = a.GetType();
    Unify(x.location, b.GetType(), argument_type);
    return core::Apply(
        bool_type, core::Builtin::kNot,
        core::Apply(bool_type,
                    core::Apply(core::ArrowType(argument_type, bool_type),
                                core::Builtin::kEqual, std::move(a)),
                    std::move(b)));
  }

  core::Expression Check(const syntax::And& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    Unify(x.a.location(), a.GetType(), bool_type);
    Unify(x.b.location(), b.GetType(), bool_type);
    return core::Apply(bool_type,
                       core::Apply(core::ArrowType(bool_type, bool_type),
                                   core::Builtin::kAnd, std::move(a)),
                       std::move(b));
  }

  core::Expression Check(const syntax::Or& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    Unify(x.a.location(), a.GetType(), bool_type);
    Unify(x.b.location(), b.GetType(), bool_type);
    return core::Apply(bool_type,
                       core::Apply(core::ArrowType(bool_type, bool_type),
                                   core::Builtin::kOr, std::move(a)),
                       std::move(b));
  }

  core::Expression Check(const syntax::Not& x) {
    core::Expression inner = Check(x.inner);
    Unify(x.inner.location(), inner.GetType(), bool_type);
    return core::Apply(bool_type, core::Builtin::kNot, std::move(inner));
  }

  core::Expression Check(const syntax::Cons& x) {
    return Cons(x.location, Check(x.head), Check(x.tail));
  }

  core::Expression Check(const syntax::Concat& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    core::Type type = a.GetType();
    Unify(x.a.location(), type,
          List(core::TypeVariable(NextIdentifier(x.location))));
    Unify(x.b.location(), type, b.GetType());
    return core::Apply(type,
                       core::Apply(core::ArrowType(type, type),
                                   core::Builtin::kConcat, std::move(a)),
                       std::move(b));
  }

  core::Expression Check(const syntax::Apply& a) {
    core::Expression f = Check(a.f);
    core::Expression x = Check(a.x);
    const core::TypeVariable result(NextIdentifier(a.location));
    Unify(a.location, f.GetType(), core::ArrowType(x.GetType(), result));
    return core::Apply(result, std::move(f), std::move(x));
  }

  core::Expression Check(const syntax::Compose& x) {
    core::Expression f = Check(x.f);
    core::Expression g = Check(x.g);
    // f :: b -> c
    // g :: a -> b
    // f . g :: a -> c
    const core::TypeVariable a(NextIdentifier(x.location));
    const core::TypeVariable b(NextIdentifier(x.location));
    const core::TypeVariable c(NextIdentifier(x.location));
    Unify(x.f.location(), f.GetType(), core::ArrowType(b, c));
    Unify(x.g.location(), g.GetType(), core::ArrowType(a, b));
    const core::Identifier v = NextIdentifier(x.location);
    return core::Lambda(
        core::ArrowType(a, c), core::Variable(a, v),
        core::Apply(c, std::move(f),
                    core::Apply(b, std::move(g), core::Variable(a, v))));
  }

  core::Case::Alternative CheckAlternativeImpl(
      const syntax::Identifier& x, const syntax::Expression& value) {
    throw Error(x.location, "unimplemented");
    // if (IsTypeName(x.value)) {
    //   // The pattern is a type constructor with no arguments.
    //   const auto* u =
    //       std::get_if<core::UnionConstructor>(&Lookup(x).value->value);
    //   if (!u) throw Error(x.location, "not a data constructor");
    //   if (u->type->alternatives.at(u->index).num_members != 0) {
    //     throw Error(x.location, "wrong arity for data constructor");
    //   }
    //   return core::Case::Alternative(core::MatchUnion(u->type, u->index, {}),
    //                                  Check(value));
    // }
    // const auto n = names.size();
    // core::Identifier variable = NextIdentifier(x.location);
    // names.push_back(
    //     Name{.location = x.location, .name = x.value, .value = variable});
    // core::Expression result = Check(value);
    // names.erase(names.begin() + n, names.end());
    // return core::Case::Alternative(std::move(variable), std::move(result));
  }

  core::Case::Alternative CheckAlternativeImpl(
      const syntax::Integer& x, const syntax::Expression& value) {
    throw Error(x.location, "unimplemented");
    // return core::Case::Alternative(core::Integer(x.value), Check(value));
  }

  core::Case::Alternative CheckAlternativeImpl(
      const syntax::Character& x, const syntax::Expression& value) {
    throw Error(x.location, "unimplemented");
    // return core::Case::Alternative(core::Character(x.value), Check(value));
  }

  core::Case::Alternative CheckAlternativeImpl(
      const syntax::String& x, const syntax::Expression& value) {
    throw Error(x.location, "unimplemented");
    // if (x.value != "") {
    //   throw Error(x.location, "non-empty string patterns are unimplemented");
    // }
    // return core::Case::Alternative(core::MatchUnion(list_type, 1, {}),
    //                                Check(value));
  }

  core::Case::Alternative CheckAlternativeImpl(
      const syntax::Cons& x, const syntax::Expression& value) {
    throw Error(x.location, "unimplemented");
    // const auto* head_identifier =
    //     std::get_if<syntax::Identifier>(&x.head->value);
    // const auto* tail_identifier =
    //     std::get_if<syntax::Identifier>(&x.tail->value);
    // if (!head_identifier) {
    //   throw Error(x.head.location(), "nested patterns are unimplemented");
    // }
    // if (!tail_identifier) {
    //   throw Error(x.tail.location(), "nested patterns are unimplemented");
    // }
    // core::Identifier head = NextIdentifier(x.head.location());
    // core::Identifier tail = NextIdentifier(x.tail.location());
    // const auto n = names.size();
    // names.push_back(Name{
    //     .location = x.location, .name = head_identifier->value, .value = head});
    // names.push_back(Name{
    //     .location = x.location, .name = tail_identifier->value, .value = tail});
    // core::Expression result = Check(value);
    // names.erase(names.begin() + n, names.end());
    // return core::Case::Alternative(
    //     core::MatchUnion(list_type, 0, {std::move(head), std::move(tail)}),
    //     std::move(result));
  }

  core::Case::Alternative CheckAlternativeImpl(
      const syntax::Tuple& x, const syntax::Expression& value) {
    throw Error(x.location, "unimplemented");
    // const auto n = names.size();
    // std::vector<core::Identifier> elements;
    // for (const auto& element : x.elements) {
    //   const auto* i = std::get_if<syntax::Identifier>(&element->value);
    //   if (!i) {
    //     throw Error(element.location(), "nested patterns are unimplemented");
    //   }
    //   const core::Identifier id = NextIdentifier(element.location());
    //   elements.push_back(id);
    //   names.push_back(Name{
    //       .location = i->location, .name = i->value, .value = id});
    // }
    // core::Expression result = Check(value);
    // names.erase(names.begin() + n, names.end());
    // return core::Case::Alternative(core::MatchTuple(std::move(elements)),
    //                                std::move(result));
  }

  core::Case::Alternative CheckAlternativeImpl(
      const syntax::List& x, const syntax::Expression& value) {
    throw Error(x.location, "unimplemented");
    // if (!x.elements.empty()) {
    //   throw Error(x.location, "non-empty list patterns are unimplemented");
    // }
    // return core::Case::Alternative(core::MatchUnion(list_type, 1, {}),
    //                                Check(value));
  }

  core::Case::Alternative CheckAlternativeImpl(
      const syntax::Apply& x, const syntax::Expression& value) {
    throw Error(x.location, "unimplemented");
    // // An application in a pattern must be a data constructor pattern. We need
    // // to unwrap all the Apply nodes. Since Apply nodes are left-associative,
    // // the unwrapping will produce the parameters in reverse order, so they must
    // // then be examined in reverse.
    // std::vector<const syntax::Identifier*> parameters;
    // const syntax::Apply* e = &x;
    // while (true) {
    //   const auto* i = std::get_if<syntax::Identifier>(&e->x->value);
    //   if (!i) {
    //     throw Error(e->x.location(), "illegal pattern expression");
    //   }
    //   parameters.push_back(i);
    //   if (const auto* a = std::get_if<syntax::Apply>(&e->f->value)) {
    //     e = a;
    //   } else {
    //     break;
    //   }
    // }
    // const auto* c = std::get_if<syntax::Identifier>(&e->f->value);
    // if (!c) throw Error(e->f.location(), "illegal pattern expression");
    // const auto* u =
    //     std::get_if<core::UnionConstructor>(&Lookup(*c).value->value);
    // if (!u) throw Error(c->location, "not a data constructor");
    // const int arity = u->type->alternatives.at(u->index).num_members;
    // if ((int)parameters.size() != arity) {
    //   throw Error(c->location, "wrong arity for data constructor");
    // }
    // const int n = names.size();
    // std::vector<core::Identifier> elements;
    // for (int i = parameters.size() - 1; i >= 0; i--) {
    //   const core::Identifier id = NextIdentifier(parameters[i]->location);
    //   names.push_back(Name{.location = parameters[i]->location,
    //                        .name = parameters[i]->value,
    //                        .value = id});
    //   elements.push_back(id);
    // }
    // core::Pattern pattern =
    //     core::MatchUnion(u->type, u->index, std::move(elements));
    // core::Expression result = Check(value);
    // names.erase(names.begin() + n, names.end());
    // return core::Case::Alternative(std::move(pattern), std::move(result));
  }

  core::Case::Alternative CheckAlternativeImpl(const auto& x,
                                               const syntax::Expression&) {
    throw Error(x.location, "illegal pattern expression");
  }

  core::Case::Alternative CheckAlternative(
      const syntax::Alternative& alternative) {
    return std::visit(
        [&](const auto& x) {
          return CheckAlternativeImpl(x, alternative.value);
        },
        alternative.pattern->value);
  }

  core::Expression Check(const syntax::Case& x) {
    throw Error(x.location, "unimplemented");
    // core::Expression value = Check(x.value);
    // std::vector<core::Case::Alternative> alternatives;
    // for (const auto& alternative : x.alternatives) {
    //   alternatives.push_back(CheckAlternative(alternative));
    // }
    // if (alternatives.empty()) throw Error(x.location, "no case alternatives");
    // return core::Case(std::move(value), std::move(alternatives));
  }

  core::Binding CheckBinding(core::Variable variable,
                             const syntax::Binding& definition) {
    const auto n = names.size();
    std::vector<core::Variable> parameters;
    for (const auto& parameter : definition.parameters) {
      const core::Variable value(
          core::TypeVariable(NextIdentifier(definition.location)),
          NextIdentifier(definition.location));
      names.push_back(Name{.location = parameter.location,
                           .name = parameter.value,
                           .value = value});
      parameters.push_back(value);
    }
    core::Expression result = Check(definition.value);
    names.erase(names.begin() + n, names.end());
    for (int i = definition.parameters.size() - 1; i >= 0; i--) {
      result =
          core::Lambda(core::ArrowType(parameters[i].type, result.GetType()),
                       parameters[i], std::move(result));
    }
    Unify(definition.location, variable.type, result.GetType());
    return core::Binding(variable, std::move(result));
  }

  core::Expression Check(const syntax::Let& x) {
    throw Error(x.location, "unimplemented");
    // struct BindingData {
    //   core::Identifier name;
    //   const syntax::Binding* definition;
    // };
    // const int n = names.size();
    // std::set<std::string> binding_names;
    // std::vector<BindingData> definitions;
    // for (const auto& definition : x.bindings) {
    //   if (!binding_names.insert(definition.name.value).second) {
    //     throw Error(definition.location, "redefinition of ",
    //                 definition.name.value, " within let binding");
    //   }
    //   const core::Identifier name = NextIdentifier(definition.location);
    //   names.push_back(Name{.location = definition.location,
    //                        .name = definition.name.value,
    //                        .value = name});
    //   definitions.push_back(BindingData{name, &definition});
    // }

    // std::vector<core::Binding> bindings;
    // for (const auto [name, definition] : definitions) {
    //   bindings.push_back(CheckBinding(name, *definition));
    // }

    // core::Expression value = Check(x.value);
    // names.erase(names.begin() + n, names.end());
    // return core::LetRecursive(std::move(bindings), value);
  }

  core::Expression Check(const syntax::If& x) {
    throw Error(x.location, "unimplemented");
    // core::Expression condition = Check(x.condition),
    //                  then_branch = Check(x.then_branch),
    //                  else_branch = Check(x.else_branch);
    // return core::Case(
    //     std::move(condition),
    //     {core::Case::Alternative(core::MatchUnion(bool_type, 1, {}),
    //                              std::move(then_branch)),
    //      core::Case::Alternative(core::MatchUnion(bool_type, 0, {}),
    //                              std::move(else_branch))});
  }

  core::Expression Check(const syntax::Expression& x) {
    return std::visit(
        [&](const auto& x) -> core::Expression { return Check(x); }, x->value);
  }

  void Check(const syntax::DataDefinition& x) {
    throw Error(x.location, "unimplemented");
    // const core::UnionType::Id id = NextUnion(x.location);
    // std::vector<core::TupleType> alternatives;
    // for (const auto& alternative : x.alternatives) {
    //   alternatives.push_back(core::TupleType(alternative.members.size()));
    // }
    // const auto type =
    //     std::make_shared<core::UnionType>(id, std::move(alternatives));
    // for (int i = 0, n = x.alternatives.size(); i < n; i++) {
    //   if (TryLookup(x.alternatives[i].name.value)) {
    //     throw Error(x.alternatives[i].location, "redefinition of ",
    //                 x.alternatives[i].name.value);
    //   }
    //   names.push_back(Name{.location = x.alternatives[i].location,
    //                        .name = x.alternatives[i].name.value,
    //                        .value = core::UnionConstructor(type, i)});
    // }
  }

  core::Expression Check(const syntax::Program& program) {
    for (const auto& data_definition : program.data_definitions) {
      Check(data_definition);
    }
    struct BindingData {
      core::Variable variable;
      const syntax::Binding* definition;
    };
    std::vector<BindingData> definitions;
    for (const auto& definition : program.definitions) {
      if (TryLookup(definition.name.value)) {
        throw Error(definition.location, "redefinition of ",
                    definition.name.value);
      }
      const core::Variable value(
          core::TypeVariable(NextIdentifier(definition.location)),
          NextIdentifier(definition.location));
      names.push_back(Name{.location = definition.location,
                           .name = definition.name.value,
                           .value = value});
      definitions.push_back(BindingData{value, &definition});
    }

    std::vector<core::Binding> bindings;
    for (const auto& [variable, definition] : definitions) {
      bindings.push_back(CheckBinding(variable, *definition));
    }

    throw std::runtime_error("that's all, folks");
    // const Name* main = TryLookup("main");
    // if (!main) throw Error(program.end, "no definition for main");
    // return core::LetRecursive(std::move(bindings), main->value);
  }

  core::Identifier NextIdentifier(Location) {
    core::Identifier next = core::Identifier(next_id++);
    return next;
  }

  core::TypeConstructor::Id NextUnion(Location) {
    const auto id = next_union;
    next_union = core::TypeConstructor::Id((int)next_union + 1);
    return id;
  }

  core::Expression Nil(core::Type element_type) {
    return core::DataConstructor(core::ApplyType(list_type, {element_type}), 1);
  }

  core::Type List(core::Type element_type) {
    return core::ApplyType(list_type, {element_type});
  }

  core::Expression Cons(Location location, core::Expression head,
                        core::Expression tail) {
    core::Type element_type = head.GetType();
    core::Type type = tail.GetType();
    Unify(location, type, List(element_type));
    core::Expression cons = core::DataConstructor(
        core::ArrowType(element_type, core::ArrowType(type, type)), 0);
    return core::Apply(type,
                       core::Apply(core::ArrowType(type, type), std::move(cons),
                                   std::move(head)),
                       std::move(tail));
  }

  int next_id = 0;
  core::TypeConstructor::Id next_union =
      core::TypeConstructor::Id::kFirstUserType;
  const core::Type bool_type =
      core::TypeConstructor(core::TypeConstructor::Id::kBool, {});
  const core::TypeConstructor list_type = core::TypeConstructor(
      core::TypeConstructor::Id::kList,
      {core::TypeVariable(NextIdentifier(kBuiltinLocation))});
  std::map<core::Identifier, core::Type> constraints;
  std::vector<Name> names;
  //     Name{.location = kBuiltinLocation,
  //          .name = "False",
  //          .value = core::UnionConstructor(bool_type, 0)},
  //     Name{.location = kBuiltinLocation,
  //          .name = "True",
  //          .value = core::UnionConstructor(bool_type, 1)},
  //     Name{.location = kBuiltinLocation,
  //          .name = "chr",
  //          .value = core::Builtin::kChr},
  //     Name{.location = kBuiltinLocation,
  //          .name = "error",
  //          .value = core::Builtin::kError},
  //     Name{.location = kBuiltinLocation,
  //          .name = "not",
  //          .value = core::Builtin::kNot},
  //     Name{.location = kBuiltinLocation,
  //          .name = "ord",
  //          .value = core::Builtin::kOrd},
  //     Name{.location = kBuiltinLocation,
  //          .name = "readInt",
  //          .value = core::Builtin::kReadInt},
  //     Name{.location = kBuiltinLocation,
  //          .name = "shift",
  //          .value = core::Builtin::kBitShift},
  //     Name{.location = kBuiltinLocation,
  //          .name = "showInt",
  //          .value = core::Builtin::kShowInt},
  // };
};

}  // namespace

core::Expression Check(const syntax::Program& program) {
  Checker checker;
  return checker.Check(program);
}

}  // namespace aoc2022
