
#ifndef HILTI_FUNCTION_H
#define HILTI_FUNCTION_H

#include <ast/ast.h>

#include "common.h"
#include "module.h"
#include "type.h"
#include "statement.h"

namespace hilti {

namespace function {

typedef type::function::Parameter Parameter;
typedef type::function::parameter_list parameter_list;

}

/// AST node for a function.
class Function : public ast::Function<AstInfo>
{
public:
   /// Constructor.
   ///
   /// id: A non-scoped ID with the function's name.
   ///
   /// module: The module the function is part of. Note that it will not
   /// automatically be added to that module.
   ///
   /// body: A statement with the function's body. Typically, the statement type will be that of a block of statements.
   ///
   /// l: Location associated with the node.
   Function(shared_ptr<ID> id, shared_ptr<hilti::type::Function> ftype, shared_ptr<Module> module, shared_ptr<Statement> body = nullptr, const Location& l=Location::None);

   ACCEPT_VISITOR_ROOT()
};

/// AST node for a hook.
class Hook : public Function
{
public:
   typedef std::pair<string, int64_t> attribute;
   typedef std::list<attribute> attribute_list;

   /// id: A non-scoped ID with the hook's name.
   ///
   /// module: The module the hook is part of. Note that it will not
   /// automatically be added to that module.
   ///
   /// priority: If body is given, the hook's priority. Should default to 0.
   ///
   /// group: If body is given, the hook's priority, the hook's group. 0 for
   /// none.
   ///
   /// body: A statement with the hook's body. Typically, the statement type
   /// will be that of a block of statements. body can be null if this is
   /// just a hook prototype.
   ///
   /// l: Location associated with the node.
   Hook(shared_ptr<ID> id, shared_ptr<hilti::type::Hook> ftype, shared_ptr<Module> module,
        uint64_t priority = 0, uint64_t group = 0, shared_ptr<Statement> body = nullptr,
        const Location& l=Location::None)
       : Function(id, ftype, module, body, l) { _priority = priority; _group = group; }

   Hook(shared_ptr<ID> id, shared_ptr<hilti::type::Hook> ftype, shared_ptr<Module> module, const attribute_list& attrs,
        shared_ptr<Statement> body = nullptr, const Location& l=Location::None);

   /// Returns the hook's group, with 0 meaning no group.
   int64_t group() const { return _group; }

   /// Returns the hook's priority.
   int64_t priority() const { return _priority; }

   ACCEPT_VISITOR(Function)

private:
   int64_t _group;
   int64_t _priority;
};

}

#endif