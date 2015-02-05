#ifndef HILTI_BUILDER_MODULE_H
#define HILTI_BUILDER_MODULE_H

#include "hilti/hilti-intern.h"

namespace hilti {
namespace builder {

class BlockBuilder;

/// Class for building a HILTI module.
class ModuleBuilder : public ast::Logger
{
public:
   /// Constructor setting up building a new module.
   ///
   /// ctx: The compiler context the module is part of.
   ///
   /// id: The name of the module.
   ///
   /// path: A file system path associated with the module.
   ///
   /// libdirs: Directories where to search other modules when importing.
   ///
   /// l: A location associated with the module.
   ModuleBuilder(shared_ptr<CompilerContext> ctx, shared_ptr<hilti::ID> id, const std::string& path = "-", const Location& l = Location::None);

   /// Constructor setting up building a new module.
   ///
   /// ctx: The compiler context the module is part of.
   ///
   /// id: The name of the module.
   ///
   /// path: A file system path associated with the module.
   ///
   /// libdirs: Directories where to search other modules when importing.
   ///
   /// l: A location associated with the module.
   ModuleBuilder(shared_ptr<CompilerContext> ctx, const std::string& id, const std::string& path = "-", const Location& l = Location::None);

   /// Destructor.
   ~ModuleBuilder();

   /// Returns the compiler context that the module is part of.
   shared_ptr<CompilerContext> compilerContext() const;

   /// Imports another module into the current's namespace.
   ///
   /// module: The name of the module to import.
   void importModule(shared_ptr<ID> id);

   /// Imports another module into the current's namespace.
   ///
   /// module: The name of the module to import.
   void importModule(const std::string& id);

   /// Returns true if a given ID has been imported.
   ///
   /// module: The name of the module to check for.
   bool idImported(shared_ptr<ID> id) const;

   /// Returns true if a module of then given name has been imported.
   ///
   /// module: The name of the module to check for.
   bool idImported(const std::string& id) const;

   // Returns a list of paths of modules imported so far.
   std::list<std::string> importedPaths() const;

   /// Exports an ID so that it can be accessed from other modules.
   ///
   /// id: The id to export.
   void exportID(shared_ptr<ID> id);

   /// Exports an ID so that it can be accessed from other modules.
   ///
   /// id: The id to export.
   void exportID(const std::string& id);

   /// Exports a type, which means the compilter will generate externally
   /// visible type information for it.
   ///
   /// type: The type to export.
   void exportType(shared_ptr<hilti::Type> type);

   /// Finishes the building process. This must be called before using the
   /// compiled module, it will resolve an still unresolved references.
   ///
   /// Returns: The finalized module, or null on error.
   shared_ptr<hilti::Module> finalize();

   /// Returns the module's AST being built. Note that before calling
   /// finalize(), this remains in an unresolved state.
   shared_ptr<hilti::Module> module() const;

   /// Saves the module's HILTI source code into a file. This can be called
   /// at any time, even before finalizing, and will reflect the module's
   /// current state.
   void saveHiltiCode(const std::string& path) const;

   /// Returns the current scope. This can be the global scope, a function
   /// scope, or a block scope, depending on the current state.
   shared_ptr<hilti::Scope> scope() const;

   /// Returns the top function form the stack of those currently being
   /// built.
   shared_ptr<hilti::Function> function() const;

   /// Returns the top builder on the current function's stack. If no
   /// function is current being built, return a builder for the module's
   /// global instruction block.
   shared_ptr<BlockBuilder> builder() const;

   /// Returns an expression referencing the block associated with the top
   /// builder on the current function's stack. If no function is current
   /// being built, returns the module's global instruction block.
   shared_ptr<hilti::Expression> block() const;

   /// Pushes a HILTI function onto the stack of functions currently being
   /// built. function() will now return this function. By default, the
   /// method then begins building the function's body and creates an initial
   /// block builder. builder() will now return that.
   ///
   /// func: The function to push.
   ///
   /// no_body: If true, don't push a body.
   ///
   /// Returns: An expression referencing the function.
   shared_ptr<hilti::declaration::Function> pushFunction(shared_ptr<hilti::Function> func, bool no_body = false);

   /// Adds a function declaration to the module, without beginning to build
   /// the function. If the function is already declared, the existing
   /// declaration is returned.
   ///
   /// func: The function to declare.
   ///
   /// Returns: An expression referencing the function.
   shared_ptr<hilti::declaration::Function> declareFunction(shared_ptr<hilti::Function> func);

   /// Adds a function declaration to the module, without beginning to build
   /// the function. If the function is already declared, the existing
   /// declaration is returned.
   ///
   /// Returns: An expression referencing the function.
   shared_ptr<hilti::declaration::Function> declareFunction(shared_ptr<ID> id,
                                                           shared_ptr<hilti::function::Result> result = nullptr,
                                                           const hilti::function::parameter_list& params = hilti::function::parameter_list(),
                                                           hilti::type::function::CallingConvention cc = hilti::type::function::HILTI,
                                                           const hilti::AttributeSet& attrs = hilti::AttributeSet(),
                                                           const Location& l=Location::None);

   /// Adds a function declaration to the module, without beginning to build
   /// the function. If the function is already declared, the existing
   /// declaration is returned.
   ///
   /// Returns: An expression referencing the function.
   shared_ptr<hilti::declaration::Function> declareFunction(shared_ptr<ID> id,
							    std::shared_ptr<hilti::type::Function> type,
							    const Location& l=Location::None);

   /// Adds a function declaration to the module, without beginning to build
   /// the function. If the function is already declared, the existing
   /// declaration is returned.
   ///
   /// Returns: An expression referencing the function.
   shared_ptr<hilti::declaration::Function> declareFunction(const std::string& id,
							    std::shared_ptr<hilti::type::Function> type,
							    const Location& l=Location::None);

   /// Adds a function declaration to the module, without beginning to build
   /// the function. If the function is already declared, the existing
   /// declaration is returned.
   ///
   /// Returns: An expression referencing the function.
   shared_ptr<hilti::declaration::Function> declareFunction(const std::string& id,
                                                           shared_ptr<hilti::function::Result> result = nullptr,
                                                           const hilti::function::parameter_list& params = hilti::function::parameter_list(),
                                                           hilti::type::function::CallingConvention cc = hilti::type::function::HILTI,
                                                           const hilti::AttributeSet& attrs = hilti::AttributeSet(),
                                                           const Location& l=Location::None);

   /// Adds a hook declaration to the module, without beginning to build the hook.
   ///
   /// func: The hook to declare.
   ///
   /// Returns: An expression referencing the hook.
   shared_ptr<hilti::declaration::Function> declareHook(shared_ptr<hilti::Hook> func);

   /// Adds a hook declaration to the module, without beginning to build the hook.
   ///
   ///
   /// Returns: An expression referencing the hook.
   shared_ptr<hilti::declaration::Function> declareHook(shared_ptr<ID> id,
                                                        shared_ptr<hilti::function::Result> result = nullptr,
                                                        const hilti::function::parameter_list& params = hilti::function::parameter_list(),
                                                        const hilti::AttributeSet& attrs = hilti::AttributeSet(),
                                                        const Location& l=Location::None);

   /// Adds a hook declaration to the module, without beginning to build the
   /// hook.
   ///
   /// Returns: An expression referencing the hook.
   shared_ptr<hilti::declaration::Function> declareHook(shared_ptr<ID> id,
							std::shared_ptr<hilti::type::Hook> type,
							const Location& l=Location::None);

   /// Adds a hook declaration to the module, without beginning to build the hook.
   ///
   ///
   /// Returns: An expression referencing the hook.
   shared_ptr<hilti::declaration::Function> declareHook(const std::string& id,
                                                       shared_ptr<hilti::function::Result> result = nullptr,
                                                       const hilti::function::parameter_list& params = hilti::function::parameter_list(),
                                                       const hilti::AttributeSet& attrs = hilti::AttributeSet(),
                                                       const Location& l=Location::None);

   /// Pushes a HILTI function onto the stack of functions currently being
   /// built. function() will now return this function. The method then
   /// begins building the function's body and creates an initial block
   /// builder. builder() will now return that.
   ///
   /// id: The name of the function.
   ///
   /// result: The parameter representing the return value. Can be null for a
   /// void function.
   ///
   /// params: The parameters to the functions.
   ///
   /// cc: The function's calling convention.
   ///
   /// no_body: If true, don't push a body.
   ///
   /// l: Location associated with the type.
   ///
   /// Returns: An expression referencing the function.
   shared_ptr<hilti::declaration::Function> pushFunction(shared_ptr<ID> id,
                                                  shared_ptr<hilti::function::Result> result = nullptr,
                                                  const hilti::function::parameter_list& params = hilti::function::parameter_list(),
                                                  hilti::type::function::CallingConvention cc = hilti::type::function::HILTI,
                                                  const hilti::AttributeSet& attrs = hilti::AttributeSet(),
                                                  bool no_body = false,
                                                  const Location& l=Location::None);

   /// Pushes a HILTI function onto the stack of functions currently being
   /// built. function() will now return this function. The method then
   /// begins building the function's body and creates an initial block
   /// builder. builder() will now return that.
   ///
   /// id: The name of the function.
   ///
   /// ftype: The function's type.
   ///
   /// no_body: If true, don't push a body.
   ///
   /// l: Location associated with the type.
   ///
   /// Returns: An expression referencing the function.
   shared_ptr<hilti::declaration::Function> pushFunction(shared_ptr<ID> id,
						  std::shared_ptr<hilti::type::Function> ftype,
                                                  bool no_body = false,
                                                  const Location& l=Location::None);

   /// Pushes a HILTI function onto the stack of functions currently being
   /// built. function() will now return this function. The method then
   /// begins building the function's body and creates an initial block
   /// builder. builder() will now return that.
   ///
   /// name: The name of the function.
   ///
   /// ftype: The function's type.
   ///
   /// no_body: If true, don't push a body.
   ///
   /// l: Location associated with the type.
   ///
   /// Returns: An expression referencing the function.
   shared_ptr<hilti::declaration::Function> pushFunction(const std::string& name,
						  std::shared_ptr<hilti::type::Function> ftype,
                                                  bool no_body = false,
                                                  const Location& l=Location::None);

   /// Pushes a HILTI function onto the stack of functions currently being
   /// built. function() will now return this function. The method then
   /// begins building the function's body and creates an initial block
   /// builder. builder() will now return that.
   ///
   /// id: The name of the function.
   ///
   /// result: The parameter representing the return value. Can be null for a
   /// void function.
   ///
   /// params: The parameters to the functions.
   ///
   /// cc: The function's calling convention.
   ///
   /// no_body: If true, don't push a body.
   ///
   /// l: Location associated with the type.
   ///
   /// Returns: An expression referencing the function.
   shared_ptr<hilti::declaration::Function> pushFunction(const std::string& id,
                                                  shared_ptr<hilti::function::Result> result = nullptr,
                                                  const hilti::function::parameter_list& params = hilti::function::parameter_list(),
                                                  hilti::type::function::CallingConvention cc = hilti::type::function::HILTI,
                                                  const hilti::AttributeSet& attrs = hilti::AttributeSet(),
                                                  bool no_body = false,
                                                  const Location& l=Location::None);

   /// Pops the top function from the stack of functions currently being
   /// built.
   shared_ptr<hilti::expression::Function> popFunction();

   /// Pushes a HILTI hook onto the stack of functions currently being built.
   /// Note that hooks and functions are internally kept on the same stack
   /// (as a hook is just a special form of a function). function() will now
   /// return this hook. The method then begins building the hook's body and
   /// creates an initial block builder. builder() will now return that.
   ///
   /// id: The name of the hook.
   ///
   /// result: The parameter representing the return value. Can be null for a
   /// void hook.
   ///
   /// params: The parameters to the hook.
   ///
   /// attrs: The hook attributes (i.e., priority and group).
   ///
   /// no_body: If true, don't push a body.
   ///
   /// l: Location associated with the type.
   ///
   /// Returns: An expression referencing the hook.
   shared_ptr<hilti::declaration::Function> pushHook(shared_ptr<ID> id,
                                              shared_ptr<hilti::function::Result> result = nullptr,
                                              const hilti::function::parameter_list& params = hilti::function::parameter_list(),
                                              const hilti::AttributeSet& attrs = hilti::AttributeSet(),
                                              bool no_body = false,
                                              const Location& l=Location::None);

   /// Pushes a HILTI hook onto the stack of functions currently being built.
   /// Note that hooks and functions are internally kept on the same stack
   /// (as a hook is just a special form of a function). function() will now
   /// return this hook. The method then begins building the hook's body and
   /// creates an initial block builder. builder() will now return that.
   ///
   /// id: The name of the hook.
   ///
   /// htype: The hooks' type.
   ///
   /// attrs: The hook attributes (i.e., priority and group).
   ///
   /// no_body: If true, don't push a body.
   ///
   /// l: Location associated with the type.
   ///
   /// Returns: An expression referencing the hook.
   shared_ptr<hilti::declaration::Function> pushHook(shared_ptr<ID> id,
                                                     std::shared_ptr<hilti::type::Hook> type,
                                                     bool no_body = false,
                                                     const Location& l=Location::None);

   /// Pushes a HILTI hook onto the stack of functions currently being built.
   /// Note that hooks and functions are internally kept on the same stack
   /// (as a hook is just a special form of a function). function() will now
   /// return this hook. The method then begins building the hook's body and
   /// creates an initial block builder. builder() will now return that.
   ///
   /// id: The name of the hook.
   ///
   /// result: The parameter representing the return value. Can be null for a
   /// void hook.
   ///
   /// params: The parameters to the hook.
   ///
   /// attrs: The hook attributes (i.e., priority and group).
   ///
   /// no_body: If true, don't push a body.
   ///
   /// l: Location associated with the type.
   ///
   /// Returns: An expression referencing the hook.
   shared_ptr<hilti::declaration::Function> pushHook(const std::string& id,
                                              shared_ptr<hilti::function::Result> result = nullptr,
                                              const hilti::function::parameter_list& params = hilti::function::parameter_list(),
                                              const hilti::AttributeSet& attrs = hilti::AttributeSet(),
                                              bool no_body = false,
                                              const Location& l=Location::None);

   /// Pushes a HILTI hook onto the stack of functions currently being built.
   /// Note that hooks and functions are internally kept on the same stack
   /// (as a hook is just a special form of a function). function() will now
   /// return this hook. The method then begins building the hook's body and
   /// creates an initial block builder. builder() will now return that.
   ///
   /// id: The name of the hook.
   ///
   /// result: The parameter representing the return value. Can be null for a
   /// void hook.
   ///
   /// params: The parameters to the hook.
   ///
   /// priority: The hook's priority.
   ///
   /// group: The hook's group.
   ///
   /// l: Location associated with the type.
   ///
   /// Returns: An expression referencing the hook.
   shared_ptr<hilti::declaration::Function> pushHook(shared_ptr<ID> id,
                                              shared_ptr<hilti::function::Result> result = nullptr,
                                              const hilti::function::parameter_list& params = hilti::function::parameter_list(),
                                              const hilti::AttributeSet& attrs = hilti::AttributeSet(),
                                              int64_t priority = 0, int64_t group = 0,
                                              bool no_body = false,
                                              const Location& l=Location::None);

   /// Pushes a HILTI hook onto the stack of functions currently being built.
   /// Note that hooks and functions are internally kept on the same stack
   /// (as a hook is just a special form of a function). function() will now
   /// return this hook. The method then begins building the hook's body and
   /// creates an initial block builder. builder() will now return that.
   ///
   /// id: The name of the hook.
   ///
   /// result: The parameter representing the return value. Can be null for a
   /// void hook.
   ///
   /// params: The parameters to the hook.
   ///
   /// priority: The hook's priority.
   ///
   /// group: The hook's group.
   ///
   /// l: Location associated with the type.
   ///
   /// Returns: An expression referencing the hook.
   shared_ptr<hilti::declaration::Function> pushHook(const std::string& id,
                                              shared_ptr<hilti::function::Result> result,
                                              const hilti::function::parameter_list& params,
                                              const hilti::AttributeSet& attrs,
                                              int64_t priority, int64_t group,
                                              bool no_body = false,
                                              const Location& l=Location::None);

   /// Pops the top hook from the stack of functions currently being built.
   /// Note that hooks and functions are internally kept on the same stack
   /// (as a hook is just a special form of a function).
   shared_ptr<hilti::expression::Function> popHook();

   /// Pushes a new body onto the stack currently being built. A body is a
   /// series of blocks with a joint scope. The method also pushes an initial
   /// block builder on the body's stack. builder() will now return that.
   ///
   /// no_builder: If true, don't push a builder.
   ///
   /// l: An associated location.
   ///
   /// Returns: The also pushed builder, or null if no_builder is set.
   shared_ptr<BlockBuilder> pushBody(bool no_builder = false, const Location& l = Location::None);

   /// Pops athe top body from the stack currently being built.
   shared_ptr<hilti::expression::Block> popBody();

   /// Pushes a block builder onto the current body's stack of builders. This
   /// works like a sequence of newBuilder() and pushBuilder().
   ///
   /// name: The block's name. The method will always create a new block and
   /// adapt the name as necessary to make it unique.
   ///
   /// Returns: The pushed builder.
   shared_ptr<BlockBuilder> pushBuilder(shared_ptr<ID> id, const Location& l = Location::None);

   /// Pushes a block builder onto the current body's stack of builders. This
   /// works like a sequence of newBuilder() and pushBuilder().
   ///
   /// name: The block's name. The method will always create a new block and
   /// adapt the name as necessary to make it unique.
   ///
   /// Returns: The pushed builder.
   shared_ptr<BlockBuilder> pushBuilder(const std::string& id, const Location& l = Location::None);

   /// Pushes a block builder onto the current body's stack of builders.
   ///
   /// builder: The block builder to push.
   ///
   /// Returns: The pushed builder.
   shared_ptr<BlockBuilder> pushBuilder(shared_ptr<BlockBuilder> builder);

   /// Removes a previously pushed block builder from a function's stack,
   /// including everything on top of it.
   ///
   /// builder: The builder to remove with everythin on top of it.
   shared_ptr<BlockBuilder> popBuilder(shared_ptr<BlockBuilder> builder);

   /// Removes the top-most block builder from a function's stack.
   shared_ptr<BlockBuilder> popBuilder();

   /// Pushes a new body onto the stack that will be executed in the context
   /// of a module-wide \a init function. This can be used to initialize
   /// globals. This must be popped with popModuleInit().
   shared_ptr<BlockBuilder> pushModuleInit();

   /// Pops the most recent body pushed with pushModuleInit().
   void popModuleInit();

   /// Instantiates a new block builder.
   ///
   /// name: The block's name. The method will always create a new block and
   /// adapt the name as necessary to make it unique.
   ///
   /// Returns the new builder.
   shared_ptr<BlockBuilder> newBuilder(shared_ptr<ID> id, const Location& l = Location::None);

   /// Instantiates a new block builder.
   ///
   /// name: The block's name. The method will always create a new block and
   /// adapt the name as necessary to make it unique.
   ///
   /// Returns the new builder.
   shared_ptr<BlockBuilder> newBuilder(const std::string& id, const Location& l = Location::None);

   /// Adds a global variable to the module.
   ///
   /// id: The name of the variable.
   ///
   /// type: The type of the variable.
   ///
   /// init: Value to initialize the global with, or null for default.
   ///
   /// force_unique: If true and an ID of the given name already exists,
   /// adapts the name to be unique. If false and the name is already taken,
   /// aborts with error.
   ///
   /// l: An associated location.
   ///
   /// Returns: An expression referencing the global.
   shared_ptr<hilti::expression::Variable> addGlobal(shared_ptr<hilti::ID> id, shared_ptr<Type> type, shared_ptr<Expression> init = nullptr, const AttributeSet& attrs = AttributeSet(), bool force_unique = false, const Location& l = Location::None);

   /// Returns true if there is already a global declaration under a given id.
   ///
   /// id: The name to test for.
   bool declared(shared_ptr<hilti::ID> id) const;

   /// Returns true if there is already a global declaration under a given id.
   ///
   /// id: The name of the ID to test for.
   bool declared(std::string& id) const;

   /// Add a declaration for a global variable to the module, without
   /// defining the global itself. This can be only used for IDs external to
   /// the module (i.e., qualified with a different module name). If the
   /// declaration already exists, that one is returned.
   ///
   /// id: The name of the ID to be declared, which must be an external one.
   ///
   /// type: The type of the ID.
   ///
   /// l: An associated location.
   ///
   /// Returns: An expressing referencing the declared global.
   shared_ptr<hilti::expression::Variable> declareGlobal(shared_ptr<hilti::ID> id, shared_ptr<Type> type, const AttributeSet& attrs = AttributeSet(), const Location& l = Location::None);

   /// Add a declaration for a global variable to the module, without
   /// defining the global itself. This can be only used for IDs external to
   /// the module (i.e., qualified with a different module name). If the
   /// declaration already exists, that one is returned.
   ///
   /// id: The name of the ID to be declared, which must be an external one.
   ///
   /// type: The type of the ID.
   ///
   /// l: An associated location.
   ///
   /// Returns: An expressing referencing the declared global.
   shared_ptr<hilti::expression::Variable> declareGlobal(const std::string& name, shared_ptr<Type> type, const AttributeSet& attrs = AttributeSet(), const Location& l = Location::None);

   /// Adds a global variable to the module.
   ///
   /// id: The name of the variable.
   ///
   /// type: The type of the variable.
   ///
   /// init: Value to initialize the global with, or null for default.
   ///
   /// force_unique: If true and an ID of the given name already exists,
   /// adapts the name to be unique. If false and the name is already taken,
   /// aborts with error.
   ///
   /// l: An associated location.
   ///
   /// Returns: An expression referencing the global.
   shared_ptr<hilti::expression::Variable> addGlobal(const std::string& id, shared_ptr<Type> type, shared_ptr<Expression> init = nullptr, const AttributeSet& attrs = AttributeSet(), bool force_unique = false, const Location& l = Location::None);

   /// Adds a global constant to the module.
   ///
   /// id: The name of the constant.
   ///
   /// type: The type of the constant.
   ///
   /// init: Value to initialize the constant. This must be of
   /// expression::Constant.
   ///
   /// force_unique: If true and an ID of the given name already exists,
   /// adapts the name to be unique. If false and the name is already taken,
   /// aborts with error.
   ///
   /// l: An associated location.
   ///
   /// Returns: An expression referencing the global.
   shared_ptr<hilti::Expression> addConstant(shared_ptr<hilti::ID> id, shared_ptr<Type> type, shared_ptr<Expression> init, bool force_unique = false, const Location& l = Location::None);

   /// Adds a global constant to the module.
   ///
   /// id: The name of the constant.
   ///
   /// type: The type of the constant.
   ///
   /// init: Value to initialize the constant. This must be of
   /// expression::Constant.
   ///
   /// force_unique: If true and an ID of the given name already exists,
   /// adapts the name to be unique. If false and the name is already taken,
   /// aborts with error.
   ///
   /// l: An associated location.
   ///
   /// Returns: An expression referencing the global.
   shared_ptr<hilti::Expression> addConstant(const std::string& id, shared_ptr<Type> type, shared_ptr<Expression> init, bool force_unique = false, const Location& l = Location::None);

   /// Adds a local variable to the current function.
   ///
   /// id: The name of the variable.
   ///
   /// type: The type of the variable.
   ///
   /// init: Value to initialize the local with, or null for default.
   ///
   /// force_unique: If true and an ID of the given name already exists in
   /// the function's scope, adapts the name to be unique. If false and the
   /// name is already taken, aborts with error.
   ///
   /// l: An associated location.
   ///
   /// Returns: An expression referencing the local.
   shared_ptr<hilti::expression::Variable> addLocal(shared_ptr<hilti::ID> id, shared_ptr<Type> type, shared_ptr<Expression> init = nullptr, const AttributeSet& attrs = AttributeSet(), bool force_unique = false, const Location& l = Location::None);

   /// Adds a local variable to the current function.
   ///
   /// id: The name of the variable.
   ///
   /// type: The type of the variable.
   ///
   /// init: Value to initialize the local with, or null for default.
   ///
   /// force_unique: If true and an ID of the given name already exists in
   /// the function's scope, adapts the name to be unique. If false and the
   /// name is already taken, aborts with error.
   ///
   /// l: An associated location.
   ///
   /// Returns: An expression referencing the local.
   shared_ptr<hilti::expression::Variable> addLocal(const std::string& id, shared_ptr<Type> type, shared_ptr<Expression> init = nullptr, const AttributeSet& attrs = AttributeSet(), bool force_unique = false, const Location& l = Location::None);

   /// Returns true if a local of a given name already exists in the current
   /// function.
   bool hasLocal(const std::string& id);

   /// Returns true if a local of a given name already exists in the current
   /// function.
   bool hasLocal(shared_ptr<hilti::ID> id);

   /// Adds a type declaration to the module.
   ///
   /// id: The name of the type. If the name is not unique, it will be made
   /// so.
   ///
   /// type: The type to associate with *name*.
   ///
   /// force_unique: If true and an ID of the given name already exists,
   /// adapts the name to be unique. If false and the name is already taken,
   /// aborts with an error.
   ///
   /// l: An associated location.
   ///
   /// Returns: The added type.
   shared_ptr<hilti::Type> addType(shared_ptr<hilti::ID> id, shared_ptr<Type> type, bool force_unique = false, const Location& l = Location::None);

   /// Adds a type declaration to the module.
   ///
   /// id: The name of the type. If the name is not unique, it will be made
   /// so.
   ///
   /// type: The type to associate with *name*.
   ///
   /// force_unique: If true and an ID of the given name already exists,
   /// adapts the name to be unique. If false and the name is already taken,
   /// aborts with an error.
   ///
   /// l: An associated location.
   ///
   /// Returns: The added type.
   shared_ptr<hilti::Type> addType(const std::string& id, shared_ptr<Type> type, bool force_unique = false, const Location& l = Location::None);

   /// Returns true if a type of a given name already exists in the current
   /// function.
   bool hasType(const std::string& id);

   /// Returns true if a type of a given name already exists in the current
   /// function.
   bool hasType(shared_ptr<hilti::ID> id);

   /// XXX
   shared_ptr<Type> lookupType(shared_ptr<hilti::ID> id);
   shared_ptr<Type> lookupType(const std::string& id);

   /// Resolves a yet unknown type. If the type passed in is yet unknown, the
   /// function looks it up and returns the one it finds. If it doesn't find
   /// any, that's an error and it aborts. If the type passed in is already
   /// resolved, returns it directly.
   ///
   /// t: The type to resolve.
   ///
   /// Returns: The resolved type.
   shared_ptr<Type> resolveType(shared_ptr<Type> type);

   /// Adds a context declaration to the module. Note that only a module must
   /// have at most one contecct declaration.
   ///
   /// type: The context of type type::Context.
   ///
   /// l: An associated location.
   ///
   /// Returns: An expression referencing the type.
   shared_ptr<hilti::expression::Type> addContext(shared_ptr<Type> type, const Location& l = Location::None);

   /// Adds a temporary variable to the current function.
   ///
   /// id: A name that will be used to build the name of the temporary
   /// variable. The methods always creates a new unique temporary and adapts
   /// the name as necessary (except for \a reuse, as below).
   ///
   /// type: The type of the variable.
   ///
   /// init: Value to initialize the global with, or null for default.
   ///
   /// reuse: If true and a temporary of that name already exists, return
   /// that one. The types must be equivalent, if they don't, that's an
   /// error.
   ///
   /// l: An associated location.
   ///
   /// Returns: An expression referencing the variable.
   shared_ptr<hilti::expression::Variable> addTmp(shared_ptr<hilti::ID> id, shared_ptr<Type> type, shared_ptr<Expression> init = nullptr, bool reuse = false, const Location& l = Location::None);

   /// Adds a temporary variable to the current function.
   ///
   /// id: A name that will be used to build the name of the temporary
   /// variable. The methods always creates a new unique temporary and adapts
   /// the name as necessary (except for \a reuse, as below).
   ///
   /// type: The type of the variable.
   ///
   /// init: Value to initialize the global with, or null for default.
   ///
   /// reuse: If true and a temporary of that name already exists, return
   /// that one. The types must be equivalent, if they don't, that's an
   /// error.
   ///
   /// l: An associated location.
   ///
   /// Returns: An expression referencing the variable.
   shared_ptr<hilti::expression::Variable> addTmp(const std::string& id, shared_ptr<Type> type, shared_ptr<Expression> init = nullptr, bool reuse = false, const Location& l = Location::None);

   /// Caches an AST under a given key.
   ///
   /// component: A component name. The value is arbitrary, but the tuple (\a
   /// component, \a key) will be used to store the node.
   ///
   /// idx: An index name The value is arbitrary, but the tuple (\a component, \a
   /// key) will be used to store the node.
   ///
   /// node: The node to cache.
   void cacheNode(const std::string& component, const std::string& idx, shared_ptr<Node> node);

   /// Looks up a previously cached node.
   ///
   /// component: The component name passed to \a cacheNode.
   ///
   /// idx: The index name passed to \a cacheNode.
   ///
   /// Returns: The cached node, or null if none.
   shared_ptr<Node> lookupNode(const std::string& component, const std::string& idx);

   typedef std::function<void ()> cache_builder_callback;

   /// Builds a block the first time it's called from within the same
   /// function, then caches that block and returns it on subsequent calls.
   ///
   /// tag: Tag to identify the block.
   ///
   /// callback: The callback to build the block. When called, the new block
   /// builder will have been pushed already, and it will be removed
   /// afterwards.
   shared_ptr<BlockBuilder> cacheBlockBuilder(const std::string& tag, cache_builder_callback cb);

   /// Activate BinPAC-specific optimizations in the HILTI compiler
   void buildForBinPAC();
    
protected:
   friend class BlockBuilder;

private:
   shared_ptr<hilti::expression::Variable> _addLocal(shared_ptr<statement::Block> stmt, shared_ptr<hilti::ID> id, shared_ptr<Type> type, shared_ptr<Expression> init = nullptr, const AttributeSet& attrs = AttributeSet(), bool force_unique = false, const Location& l = Location::None);

   struct DeclarationMapValue {
       std::string kind = "";
       shared_ptr<hilti::Type> type = nullptr;
       shared_ptr<hilti::Declaration> declaration = nullptr;
   };

   typedef std::map<std::string, DeclarationMapValue> declaration_map;

   enum _DeclStyle { REUSE, CHECK_UNIQUE, MAKE_UNIQUE };
   std::pair<shared_ptr<ID>, shared_ptr<Declaration>> _uniqueDecl(shared_ptr<ID> id, shared_ptr<Type> type, const std::string& kind, declaration_map* decls, _DeclStyle style, bool global);
   void _addDecl(shared_ptr<ID> id, shared_ptr<Type> type, const std::string& kind, declaration_map* decls, shared_ptr<Declaration>);
   shared_ptr<ID> _normalizeID(shared_ptr<ID> id) const;

   struct Body {
       shared_ptr<statement::Block> stmt;
       shared_ptr<Scope> scope;
       std::list<shared_ptr<BlockBuilder>> builders;
   };

   struct Function {
       shared_ptr<hilti::Function> function;
       Location location;
       std::list<shared_ptr<Body>> bodies;
       declaration_map locals;
       std::map<std::string, shared_ptr<BlockBuilder>> builders;
       std::set<std::string> labels;
   };

   void Init(shared_ptr<CompilerContext> ctx, shared_ptr<ID> id, const std::string& path, const Location& l);
   shared_ptr<Function> _currentFunction() const;
   shared_ptr<Body> _currentBody() const;
   shared_ptr<BlockBuilder> _currentBuilder() const;

   shared_ptr<ID> _module_name;
   path_list _libdirs;
   bool _finalized = false;
   bool _correct = false;

   shared_ptr<Module> _module;
   std::list<shared_ptr<Function>> _functions;
   std::map<std::string, shared_ptr<Node>> _node_cache;
   declaration_map _globals;
   shared_ptr<Function> _init = nullptr;

   struct TryCatch {
       shared_ptr<statement::Block> try_;
       statement::Try::catch_list catches;
   };

   std::list<std::shared_ptr<TryCatch>> _tries;

   std::list<std::string> _imported_paths;

   bool _buildForBinPAC;
};

}

}

#endif
