#include "checker.hpp"

#include "debug_output.hpp"

#include <map>
#include <optional>
#include <sstream>

namespace aoc2022 {
namespace {

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
    core::Expression AsExpression() const {
      return std::visit([](auto x) -> core::Expression { return x; }, value);
    }

    Location location;
    std::string name;
    std::variant<core::Identifier, core::Builtin> value;
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

  core::Expression Check(const syntax::Identifier& x) {
    return Lookup(x).AsExpression();
  }

  core::Integer Check(const syntax::Integer& x) {
    return core::Integer(x.value);
  }

  core::Character Check(const syntax::Character& x) {
    return core::Character(x.value);
  }

  core::String Check(const syntax::String& x) {
    return core::String(x.value);
  }

  core::Expression Check(const syntax::List& x) {
    std::vector<core::Expression> elements;
    for (const auto& element : x.elements) {
      elements.push_back(Check(element));
    }
    core::Expression result = core::Builtin::kNil;
    for (int i = elements.size() - 1; i >= 0; i--) {
      result = core::Cons(elements[i], std::move(result));
    }
    return result;
  }

  core::Expression Check(const syntax::LessThan& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    return core::Apply(core::Apply(core::Builtin::kLessThan, std::move(a)),
                       std::move(b));
  }

  core::Expression Check(const syntax::Add& x) {
    core::Expression a = Check(x.a);
    core::Expression b = Check(x.b);
    return core::Apply(core::Apply(core::Builtin::kAdd, std::move(a)),
                       std::move(b));
  }

  core::Expression Check(const syntax::Cons& x) {
    core::Expression head = Check(x.head);
    core::Expression tail = Check(x.tail);
    return core::Cons(std::move(head), std::move(tail));
  }

  core::Expression Check(const syntax::Apply& a) {
    return core::Apply(Check(a.f), Check(a.x));
  }

  core::Expression Check(const syntax::Compose& x) {
    const core::Identifier v = NextIdentifier(x.location);
    return core::Lambda(v, core::Apply(Check(x.f), core::Apply(Check(x.g), v)));
  }

  core::Case::Alternative CheckAlternativeImpl(const syntax::Identifier& x,
                                               const syntax::Expression& value) {
    const auto n = names.size();
    core::Identifier variable = NextIdentifier(x.location);
    names.push_back(
        Name{.location = x.location, .name = x.value, .value = variable});
    core::Expression result = Check(value);
    names.resize(n);
    return core::Case::Alternative(std::move(variable), std::move(result));
  }

  core::Case::Alternative CheckAlternativeImpl(const syntax::Integer& x,
                                               const syntax::Expression& value) {
    return core::Case::Alternative(core::Integer(x.value), Check(value));
  }

  core::Case::Alternative CheckAlternativeImpl(const syntax::Character& x,
                                               const syntax::Expression& value) {
    return core::Case::Alternative(core::Character(x.value), Check(value));
  }

  core::Case::Alternative CheckAlternativeImpl(const syntax::String& x,
                                               const syntax::Expression& value) {
    if (x.value != "") {
      throw Error(x.location, "non-empty string patterns are unimplemented");
    }
    return core::Case::Alternative(core::Builtin::kNil, Check(value));
  }

  core::Case::Alternative CheckAlternativeImpl(
      const syntax::Cons& x, const syntax::Expression& value) {
    const auto* head_identifier =
        std::get_if<syntax::Identifier>(&x.head->value);
    const auto* tail_identifier =
        std::get_if<syntax::Identifier>(&x.tail->value);
    if (!head_identifier) {
      throw Error(x.head.location(), "nested patterns are unimplemented");
    }
    if (!tail_identifier) {
      throw Error(x.tail.location(), "nested patterns are unimplemented");
    }
    core::Identifier head = NextIdentifier(x.head.location());
    core::Identifier tail = NextIdentifier(x.tail.location());
    const auto n = names.size();
    names.push_back(Name{
        .location = x.location, .name = head_identifier->value, .value = head});
    names.push_back(Name{
        .location = x.location, .name = tail_identifier->value, .value = tail});
    core::Expression result = Check(value);
    names.resize(n);
    return core::Case::Alternative(
        core::Decons(std::move(head), std::move(tail)), std::move(result));
  }

  core::Case::Alternative CheckAlternativeImpl(
      const syntax::List& x, const syntax::Expression& value) {
    if (!x.elements.empty()) {
      throw Error(x.location, "non-empty list patterns are unimplemented");
    }
    return core::Case::Alternative(core::Builtin::kNil, Check(value));
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
    core::Expression value = Check(x.value);
    std::vector<core::Case::Alternative> alternatives;
    for (const auto& alternative : x.alternatives) {
      alternatives.push_back(CheckAlternative(alternative));
    }
    if (alternatives.empty()) throw Error(x.location, "no case alternatives");
    return core::Case(std::move(value), std::move(alternatives));
  }

  core::Expression Check(const syntax::If& x) {
    core::Expression condition = Check(x.condition),
                     then_branch = Check(x.then_branch),
                     else_branch = Check(x.then_branch);
    return core::Case(
        std::move(condition),
        {core::Case::Alternative(core::Builtin::kTrue, std::move(then_branch)),
         core::Case::Alternative(core::Builtin::kFalse,
                                 std::move(else_branch))});
  }

  core::Expression Check(const syntax::Expression& x) {
    return std::visit(
        [&](const auto& x) -> core::Expression { return Check(x); }, x->value);
  }

  core::Binding CheckDefinition(core::Identifier name,
                                const syntax::Definition& definition) {
    const auto n = names.size();
    std::vector<core::Identifier> parameters;
    for (const auto& parameter : definition.parameters) {
      const core::Identifier value = NextIdentifier(definition.location);
      names.push_back(Name{.location = parameter.location,
                           .name = parameter.value,
                           .value = value});
      parameters.push_back(value);
    }
    core::Expression result = Check(definition.value);
    names.resize(n);
    for (int i = definition.parameters.size() - 1; i >= 0; i--) {
      result = core::Lambda(parameters[i], std::move(result));
    }
    return core::Binding(name, std::move(result));
  }

  core::Expression Check(const syntax::Program& program) {
    struct BindingData {
      core::Identifier name;
      const syntax::Definition* definition;
    };
    std::vector<BindingData> definitions;
    for (const auto& definition : program.definitions) {
      if (TryLookup(definition.name.value)) {
        throw Error(definition.location, "redefinition of ",
                    definition.name.value);
      }
      const core::Identifier name = NextIdentifier(definition.location);
      names.push_back(Name{.location = definition.location,
                           .name = definition.name.value,
                           .value = name});
      definitions.push_back(BindingData{name, &definition});
    }

    std::vector<core::Binding> bindings;
    for (const auto [name, definition] : definitions) {
      bindings.push_back(CheckDefinition(name, *definition));
    }

    const Name* main = TryLookup("main");
    if (!main) throw Error({1, 1}, "no definition for main");
    return core::LetRecursive(std::move(bindings), main->AsExpression());
  }

  core::Identifier NextIdentifier(Location location) {
    core::Identifier next = core::Identifier(next_id++);
    locations.emplace(next, location);
    return next;
  }

  int next_id = 0;
  std::map<core::Identifier, Location> locations;
  std::vector<Name> names = {
      Name{.location = {1, 1}, .name = "map", .value = core::Builtin::kMap},
      Name{.location = {1, 1},
           .name = "readInt",
           .value = core::Builtin::kReadInt},
      Name{.location = {1, 1}, .name = "split", .value = core::Builtin::kSplit},
      Name{.location = {1, 1},
           .name = "showInt",
           .value = core::Builtin::kShowInt},
  };
};

}  // namespace

core::Expression Check(const syntax::Program& program) {
  Checker checker;
  return checker.Check(program);
}

}  // namespace aoc2022
