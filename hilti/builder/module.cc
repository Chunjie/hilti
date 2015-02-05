
#include "module.h"
#include "hilti/hilti-intern.h"

namespace hilti {
namespace builder {

ModuleBuilder::ModuleBuilder(shared_ptr<CompilerContext> ctx, shared_ptr<ID> id, const std::string& path, const Location& l)
{
    Init(ctx, id, path, l);
}

ModuleBuilder::ModuleBuilder(shared_ptr<CompilerContext> ctx, const std::string& id, const std::string& path, const Location& l)
{
    Init(ctx, std::make_shared<ID>(id, l), path, l);
}

void ModuleBuilder::Init(shared_ptr<CompilerContext> ctx, shared_ptr<ID> id, const std::string& path, const Location& l)
{
    _module = builder::module::create(ctx, id, path, l);

    // Push a top-level function state as a place-holder for the module's statement.
    auto func = std::make_shared<ModuleBuilder::Function>();
    func->function = nullptr;
    func->location = l;
    _functions.push_back(func);

    // Push a top level body.
    auto body = std::make_shared<ModuleBuilder::Body>();
    body->stmt = std::make_shared<statement::Block>(nullptr, nullptr, l);
    body->scope = body->stmt->scope();

    auto builder = shared_ptr<BlockBuilder>(new BlockBuilder(body->stmt, body->stmt, this));  // No need for two level of blocks here.
    body->builders.push_back(builder);

    func->bodies.push_back(body);
    _module->setBody(body->stmt);
    body->stmt->scope()->setID(_module->id());
}

ModuleBuilder::~ModuleBuilder()
{
}

shared_ptr<CompilerContext> ModuleBuilder::compilerContext() const
{
    return _module->compilerContext();
}

shared_ptr<ModuleBuilder::Function> ModuleBuilder::_currentFunction() const
{
    assert(_functions.size());
    return _functions.back();
}

shared_ptr<ModuleBuilder::Body> ModuleBuilder::_currentBody() const
{
    auto func = _currentFunction();
    assert(func->bodies.size());
    return func->bodies.back();
}

shared_ptr<BlockBuilder> ModuleBuilder::_currentBuilder() const
{
    auto body = _currentBody();
    assert(body->builders.size());
    return body->builders.back();
}

void ModuleBuilder::exportType(shared_ptr<hilti::Type> type)
{
    _module->exportType(type);
}


void ModuleBuilder::exportID(shared_ptr<hilti::ID> type)
{
    _module->exportID(type);
}

void ModuleBuilder::exportID(const std::string& name)
{
    _module->exportID(std::make_shared<ID>(name));
}

shared_ptr<ID> ModuleBuilder::_normalizeID(shared_ptr<ID> id) const
{
    if ( id->pathAsString().find("-") )
        id = builder::id::node(util::strreplace(id->pathAsString(), "-", "_"), id->location());

    return id;
}

std::pair<shared_ptr<ID>, shared_ptr<Declaration>> ModuleBuilder::_uniqueDecl(shared_ptr<ID> id, shared_ptr<Type> type, const std::string& kind, declaration_map* decls, _DeclStyle style, bool global)
{
    id = _normalizeID(id);

    int i = 1;
    shared_ptr<ID> uid = id;

    while ( true ) {
        auto d = decls->find(uid->pathAsString(_module->id()));

        if ( d == decls->end() )
            return std::make_pair(uid, nullptr);

        if ( style == REUSE &&
             (*d).second.kind == kind &&
             (! (*d).second.type || (*d).second.type->equal(type)) )
            return std::make_pair(uid, (*d).second.declaration);

        if ( style == CHECK_UNIQUE )
            fatalError(::util::fmt("ModuleBuilder: ID %s already defined", uid->pathAsString(_module->id())));

        std::string s = ::util::fmt("%s_%d", id->pathAsString(_module->id()), ++i);
        uid = std::make_shared<ID>(s, uid->location());
    }

    assert(false); // can't be reached.
}

void ModuleBuilder::_addDecl(shared_ptr<ID> id, shared_ptr<Type> type, const std::string& kind, declaration_map* decls, shared_ptr<Declaration> decl)
{
    DeclarationMapValue dv;
    dv.kind = kind;
    dv.type = type;
    dv.declaration = decl;
    decls->insert(std::make_pair(id->pathAsString(_module->id()), dv));
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::pushFunction(shared_ptr<hilti::Function> function, bool no_body)
{
    auto func = std::make_shared<ModuleBuilder::Function>();
    func->function = function;
    func->location = function->location();
    _functions.push_back(func);

    shared_ptr<declaration::Function> decl = nullptr;

    auto hook = ast::as<hilti::Hook>(function);

    if ( hook )
        decl = std::make_shared<declaration::Hook>(hook, function->location());
    else
        decl = std::make_shared<declaration::Function>(function, function->location());

    _module->body()->addDeclaration(decl);
    _addDecl(function->id(), function->type(), "function", &_globals, decl);

    if ( ! no_body )
        pushBody();

    return decl;
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::declareFunction(shared_ptr<hilti::Function> func)
{
    auto decls = _module->body()->declarations();

    for ( auto d : decls ) {
        if ( d->id()->pathAsString() == func->id()->pathAsString() )
            // TODO: Should double-check that declarations match.
            return ast::checkedCast<declaration::Function>(d);
    }

    auto decl = std::make_shared<declaration::Function>(func, func->location());
    _module->body()->addDeclaration(decl);
    _addDecl(func->id(), func->type(), "function", &_globals, decl);
    return decl;
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::pushFunction(shared_ptr<ID> id,
                                                              shared_ptr<hilti::function::Result> result,
                                                              const hilti::function::parameter_list& params,
                                                              hilti::type::function::CallingConvention cc,
                                                              const hilti::AttributeSet& attrs,
                                                              bool no_body,
                                                              const Location& l)
{
    if ( ! result )
        result = std::make_shared<hilti::function::Result>(builder::void_::type(), true);

    auto ftype = std::make_shared<hilti::type::HiltiFunction>(result, params, cc, l);
    ftype->setAttributes(attrs);

    auto func = std::make_shared<hilti::Function>(id, ftype, _module, nullptr, l);

    return pushFunction(func, no_body);
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::pushFunction(shared_ptr<ID> id,
                                                                     std::shared_ptr<hilti::type::Function> ftype,
                                                                     bool no_body,
                                                                     const Location& l)
{
    auto func = std::make_shared<hilti::Function>(id, ftype, _module, nullptr, l);
    return pushFunction(func, no_body);
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::pushFunction(const std::string& name,
                                                              std::shared_ptr<hilti::type::Function> ftype,
                                                              bool no_body,
                                                              const Location& l)
{
    auto id = ::std::make_shared<hilti::ID>(name, l);
    auto func = std::make_shared<hilti::Function>(id, ftype, _module, nullptr, l);
    return pushFunction(func, no_body);
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::pushFunction(const std::string& id,
                                                              shared_ptr<hilti::function::Result> result,
                                                              const hilti::function::parameter_list& params,
                                                              hilti::type::function::CallingConvention cc,
                                                              const hilti::AttributeSet& attrs,
                                                              bool no_body,
                                                              const Location& l)
{
    return pushFunction(std::make_shared<ID>(id, l), result, params, cc, attrs, no_body, l);
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::declareHook(shared_ptr<hilti::Hook> hook)
{
    auto decl = std::make_shared<declaration::Hook>(hook, hook->location());
    _module->body()->addDeclaration(decl);
    _addDecl(hook->id(), hook->type(), "hook", &_globals, decl);
    return decl;
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::declareFunction(shared_ptr<ID> id,
                                                                       shared_ptr<hilti::function::Result> result,
                                                                       const hilti::function::parameter_list& params,
                                                                       hilti::type::function::CallingConvention cc,
                                                                       const hilti::AttributeSet& attrs,
                                                                       const Location& l)
{
    if ( ! result )
        result = std::make_shared<hilti::function::Result>(builder::void_::type(), true);

    auto ftype = std::make_shared<hilti::type::HiltiFunction>(result, params, cc, l);
    ftype->setAttributes(attrs);

    auto func = std::make_shared<hilti::Function>(id, ftype, _module, nullptr, l);
    return declareFunction(func);
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::declareFunction(shared_ptr<ID> id,
                                                                        std::shared_ptr<hilti::type::Function> ftype,
                                                                       const Location& l)
{
    auto func = std::make_shared<hilti::Function>(id, ftype, _module, nullptr, l);
    return declareFunction(func);
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::declareFunction(const std::string& name,
                                                                        std::shared_ptr<hilti::type::Function> ftype,
                                                                        const Location& l)
{
    auto id = ::std::make_shared<hilti::ID>(name, l);
    auto func = std::make_shared<hilti::Function>(id, ftype, _module, nullptr, l);
    return declareFunction(func);
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::declareFunction(const std::string& id,
                                                                       shared_ptr<hilti::function::Result> result,
                                                                       const hilti::function::parameter_list& params,
                                                                       hilti::type::function::CallingConvention cc,
                                                                       const hilti::AttributeSet& attrs,
                                                                       const Location& l)
{
    return declareFunction(std::make_shared<ID>(id, l), result, params, cc, attrs, l);
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::declareHook(shared_ptr<ID> id,
                                                                    shared_ptr<hilti::function::Result> result,
                                                                    const hilti::function::parameter_list& params,
                                                                    const hilti::AttributeSet& attrs,
                                                                    const Location& l)
{
    if ( ! result )
        result = std::make_shared<hilti::function::Result>(builder::void_::type(), true);

    auto ftype = std::make_shared<hilti::type::Hook>(result, params, l);
    ftype->setAttributes(attrs);

    auto func = std::make_shared<hilti::Hook>(id, ftype, _module, nullptr, l);
    return declareHook(func);
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::declareHook(shared_ptr<ID> id,
								    std::shared_ptr<hilti::type::Hook> ftype,
								    const Location& l)
{
    auto func = std::make_shared<hilti::Hook>(id, ftype, _module, nullptr, l);
    return declareHook(func);
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::declareHook(const std::string& id,
                                                                    shared_ptr<hilti::function::Result> result,
                                                                    const hilti::function::parameter_list& params,
                                                                    const hilti::AttributeSet& attrs,
                                                                    const Location& l)
{
    return declareHook(std::make_shared<hilti::ID>(id, l), result, params, attrs, l);
}


shared_ptr<hilti::declaration::Function> ModuleBuilder::pushHook(shared_ptr<ID> id,
                                                          shared_ptr<hilti::function::Result> result,
                                                          const hilti::function::parameter_list& params,
                                                          const hilti::AttributeSet& attrs,
                                                          bool no_body,
                                                          const Location& l)
{
    if ( ! result )
        result = std::make_shared<hilti::function::Result>(builder::void_::type(), true);

    auto ftype = std::make_shared<hilti::type::Hook>(result, params, l);
    ftype->setAttributes(attrs);

    auto func =  std::make_shared<hilti::Hook>(id, ftype, _module, nullptr, l);
    return pushFunction(func, no_body);
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::pushHook(shared_ptr<ID> id,
								 std::shared_ptr<hilti::type::Hook> ftype,
								 bool no_body,
								 const Location& l)
{
    auto func =  std::make_shared<hilti::Hook>(id, ftype, _module, nullptr, l);
    return pushFunction(func, no_body);
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::pushHook(const std::string& id,
                                                          shared_ptr<hilti::function::Result> result,
                                                          const hilti::function::parameter_list& params,
                                                          const hilti::AttributeSet& attrs,
                                                          bool no_body,
                                                          const Location& l)
{
    return pushHook(std::make_shared<ID>(id, l), result, params, attrs, no_body, l);

}

shared_ptr<hilti::declaration::Function> ModuleBuilder::pushHook(shared_ptr<ID> id,
                                                          shared_ptr<hilti::function::Result> result,
                                                          const hilti::function::parameter_list& params,
                                                          const hilti::AttributeSet& attrs,
                                                          int64_t priority, int64_t group,
                                                          bool no_body,
                                                          const Location& l)
{
    if ( ! result )
        result = std::make_shared<hilti::function::Result>(builder::void_::type(), true);

    hilti::AttributeSet lattrs = attrs;

    if ( priority )
        lattrs.add(attribute::PRIORITY, priority);

    if ( group )
        lattrs.add(attribute::GROUP, group);

    auto ftype = std::make_shared<hilti::type::Hook>(result, params, l);
    ftype->setAttributes(lattrs);

    auto func = std::make_shared<hilti::Hook>(id, ftype, _module, nullptr, l);
    return pushFunction(func, no_body);
}

shared_ptr<hilti::declaration::Function> ModuleBuilder::pushHook(const std::string& id,
                                                          shared_ptr<hilti::function::Result> result,
                                                          const hilti::function::parameter_list& params,
                                                          const hilti::AttributeSet& attrs,
                                                          int64_t priority, int64_t group,
                                                          bool no_body,
                                                          const Location& l)
{
    return pushHook(std::make_shared<ID>(id, l), result, params, attrs, priority, group, no_body, l);
}

shared_ptr<hilti::expression::Function> ModuleBuilder::popFunction()
{
    auto func = _functions.back()->function;
    _functions.pop_back();
    return std::make_shared<hilti::expression::Function>(func, func->location());
}

shared_ptr<hilti::expression::Function> ModuleBuilder::popHook()
{
    return popFunction();
}

shared_ptr<BlockBuilder> ModuleBuilder::pushBody(bool no_builder, const Location& l)
{
    auto func = _currentFunction();
    auto body = std::make_shared<ModuleBuilder::Body>();
    auto location = (l != Location::None ? l : _currentFunction()->location);

    body->stmt = std::make_shared<statement::Block>(scope(), nullptr, location);
    body->scope = body->stmt->scope();
    func->bodies.push_back(body);

    if ( ! func->function->body() )
        func->function->setBody(body->stmt);

    if ( ! no_builder )
        return pushBuilder(std::shared_ptr<ID>(), l);
    else
        return nullptr;
}

shared_ptr<hilti::expression::Block> ModuleBuilder::popBody()
{
    auto func = _currentFunction();
    auto body = func->bodies.back();
    func->bodies.pop_back();
    return std::make_shared<hilti::expression::Block>(body->stmt, body->stmt->location());
}

shared_ptr<BlockBuilder> ModuleBuilder::newBuilder(shared_ptr<ID> id, const Location& l)
{
    if ( id ) {
        if ( id->name().size() && ! util::startsWith(id->name(), "@") )
            id = builder::id::node(util::fmt("@%s", id->name()), id->location());

        id = _normalizeID(id);
    }

    if ( id ) {
        shared_ptr<ID> nid = id;
        int cnt = 1;
        while ( _currentFunction()->labels.find(nid->name()) != _currentFunction()->labels.end() ) {
            nid = builder::id::node(util::fmt("%s_%d", id->name(), ++cnt), id->location());
        }

        id = nid;

        _currentFunction()->labels.insert(id->name());
    }

    auto block = std::make_shared<statement::Block>(nullptr, id, l);
    return shared_ptr<BlockBuilder>(new BlockBuilder(block, nullptr, this));
}

shared_ptr<BlockBuilder> ModuleBuilder::newBuilder(const std::string& id, const Location& l)
{
    return newBuilder(std::make_shared<ID>(id, l), l);
}

shared_ptr<BlockBuilder> ModuleBuilder::pushBuilder(shared_ptr<BlockBuilder> builder)
{
    auto block = builder->statement();
    auto body = _currentBody();

    body->stmt->addStatement(block);
    block->scope()->setParent(body->stmt->scope());

    body->builders.push_back(builder);
    builder->setBody(body->stmt);

    return builder;
}

shared_ptr<BlockBuilder> ModuleBuilder::pushBuilder(shared_ptr<ID> id, const Location& l)
{
    auto location = (l != Location::None ? l : _currentBody()->stmt->location());
    return pushBuilder(newBuilder(id, l));
}

shared_ptr<BlockBuilder> ModuleBuilder::pushBuilder(const std::string& id, const Location& l)
{
    return pushBuilder(std::make_shared<ID>(id, l), l);
}

shared_ptr<BlockBuilder> ModuleBuilder::pushModuleInit()
{
    if ( ! _init ) {
        auto decl = pushFunction("__init_module", nullptr, function::parameter_list());
        decl->function()->setInitFunction();
        _init = _currentFunction();
    }

    else
        _functions.push_back(_init);

    auto body = _currentBody();
    return body->builders.back();
}

void ModuleBuilder::popModuleInit()
{
    popFunction();
}

shared_ptr<BlockBuilder> ModuleBuilder::popBuilder(shared_ptr<BlockBuilder> builder)
{
    auto body = _currentBody();
    auto i = std::find(body->builders.begin(), body->builders.end(), builder);

    if ( i == body->builders.end() ) {
        internalError("unbalanced builder push/pop");
    }

    body->builders.erase(i, body->builders.end());

    return builder;
}

shared_ptr<BlockBuilder> ModuleBuilder::popBuilder()
{
    auto body = _currentBody();
    auto builder = body->builders.back();
    body->builders.pop_back();
    return builder;
}

shared_ptr<hilti::Module> ModuleBuilder::module() const
{
    return _module;
}

shared_ptr<hilti::Module> ModuleBuilder::finalize()
{
    if ( ! _module->compilerContext()->finalize(_module, _buildForBinPAC) )
        return nullptr;

    return _module;
}

void ModuleBuilder::saveHiltiCode(const std::string& path) const
{
    std::ofstream out(path);
    _module->compilerContext()->print(_module, out);
    out.close();
}

shared_ptr<hilti::Function> ModuleBuilder::function() const
{
    return _currentFunction()->function;
}

shared_ptr<hilti::Scope> ModuleBuilder::scope() const
{
    // Return the scope from the most recent function which has a body.

    for ( auto f = _functions.rbegin(); f != _functions.rend(); f++ ) {
        if ( (*f)->bodies.size() )
            return (*f)->bodies.back()->stmt->scope();
    }

    assert(false); // Can't get here.
    return nullptr;
}

shared_ptr<BlockBuilder> ModuleBuilder::builder() const
{
    return _currentBody()->builders.back();
}

shared_ptr<hilti::Expression> ModuleBuilder::block() const
{
    return builder()->block();
}

bool ModuleBuilder::declared(shared_ptr<hilti::ID> id) const
{
    id = _normalizeID(id);
    auto d = _globals.find(id->pathAsString(_module->id()));
    return d != _globals.end();
}

bool ModuleBuilder::declared(std::string& name) const
{
    return declared(std::make_shared<hilti::ID>(name));
}

shared_ptr<hilti::expression::Variable> ModuleBuilder::addGlobal(shared_ptr<ID> id, shared_ptr<Type> type, shared_ptr<Expression> init, const AttributeSet& attrs, bool force_unique, const Location& l)
{
    auto t = _uniqueDecl(id, type, "global", &_globals, (force_unique ? MAKE_UNIQUE : CHECK_UNIQUE), true);
    id = t.first;
    auto decl = t.second ? ast::checkedCast<declaration::Variable>(t.second) : nullptr;

    if ( ! decl ) {
        auto var = std::make_shared<variable::Global>(id, type, init, l);
        var->setAttributes(attrs);
        decl = std::make_shared<declaration::Variable>(id, var, l);
        _module->body()->addDeclaration(decl);
        _addDecl(id, type, "global", &_globals, decl);
    }

    return std::make_shared<hilti::expression::Variable>(decl->variable(), l);
}

shared_ptr<hilti::expression::Variable> ModuleBuilder::addGlobal(const std::string& id, shared_ptr<Type> type, shared_ptr<Expression> init, const AttributeSet& attrs, bool force_unique, const Location& l)
{
    return addGlobal(std::make_shared<ID>(id, l), type, init, attrs, force_unique, l);
}

shared_ptr<hilti::expression::Variable> ModuleBuilder::declareGlobal(shared_ptr<hilti::ID> id, shared_ptr<Type> type, const AttributeSet& attrs, const Location& l)
{
    if ( id->scope().empty() ) {
        error("declared globals must be external", id);
        return nullptr;
    }

    id = _normalizeID(id);

    shared_ptr<hilti::declaration::Variable> decl;

    auto d = _globals.find(id->pathAsString(_module->id()));

    if ( d != _globals.end() )
        decl = ast::checkedCast<declaration::Variable>((*d).second.declaration);

    else {
        auto var = std::make_shared<variable::Global>(id, type, nullptr, l);
        var->setAttributes(attrs);
        decl = std::make_shared<declaration::Variable>(id, var, l);
        decl->setLinkage(Declaration::IMPORTED);
        _module->body()->addDeclaration(decl);
        _addDecl(id, type, "global", &_globals, decl);
    }

    return std::make_shared<hilti::expression::Variable>(decl->variable(), l);
}

shared_ptr<hilti::expression::Variable> ModuleBuilder::declareGlobal(const std::string& id, shared_ptr<Type> type, const AttributeSet& attrs, const Location& l)
{
    return declareGlobal(::std::make_shared<ID>(id, l), type, attrs, l);
}

shared_ptr<hilti::Expression> ModuleBuilder::addConstant(shared_ptr<ID> id, shared_ptr<Type> type, shared_ptr<Expression> init, bool force_unique, const Location& l)
{
    if ( ! init->isConstant() ) {
        error("constant initialized with non-constant expression", init);
        return nullptr;
    }

    if ( ! init->canCoerceTo(type) ) {
        error("constant initialization does not match type", init);
        return nullptr;
    }

    auto const_ = init->coerceTo(type);
    auto t = _uniqueDecl(id, type, "const", &_globals, (force_unique ? MAKE_UNIQUE : CHECK_UNIQUE), true);
    id = t.first;
    auto decl = t.second ? ast::checkedCast<declaration::Constant>(t.second) : nullptr;

    if ( ! decl ) {
        decl = std::make_shared<declaration::Constant>(id, const_, l);
        _module->body()->addDeclaration(decl);
        _addDecl(id, type, "const", &_globals, decl);
    }

    return std::make_shared<hilti::expression::ID>(id, l);
}

shared_ptr<hilti::Expression> ModuleBuilder::addConstant(const std::string& id, shared_ptr<Type> type, shared_ptr<Expression> init, bool force_unique, const Location& l)
{
    return addConstant(std::make_shared<ID>(id, l), type, init, force_unique, l);
}

shared_ptr<hilti::Type> ModuleBuilder::addType(shared_ptr<hilti::ID> id, shared_ptr<Type> type, bool force_unique, const Location& l)
{
    if ( id->isScoped() )
        id = std::make_shared<ID>(id->pathAsString(_module->id()), id->location());

    auto t = _uniqueDecl(id, type, "type", &_globals, (force_unique ? MAKE_UNIQUE : CHECK_UNIQUE), true);
    id = t.first;
    auto decl = t.second ? ast::checkedCast<declaration::Type>(t.second) : nullptr;

    if ( ! decl ) {
        decl = std::make_shared<declaration::Type>(id, type, l);
        _module->body()->addDeclaration(decl);
        _addDecl(id, type, "type", &_globals, decl);
    }

    type->setID(id);
    type->setScope(_module->id()->name());
    return std::make_shared<hilti::type::Unknown>(id, l);
}

shared_ptr<hilti::Type> ModuleBuilder::addType(const std::string& id, shared_ptr<Type> type, bool force_unique, const Location& l)
{
    return addType(std::make_shared<ID>(id, l), type, force_unique, l);
}

bool ModuleBuilder::hasType(const std::string& id)
{
    return hasType(std::make_shared<ID>(id));
}

bool ModuleBuilder::hasType(shared_ptr<hilti::ID> id)
{
    id = _normalizeID(id);
    auto d = _globals.find(id->pathAsString(_module->id()));
    return d != _globals.end();
}

shared_ptr<Type> ModuleBuilder::lookupType(shared_ptr<hilti::ID> id)
{
    id = _normalizeID(id);
    auto d = _globals.find(id->pathAsString(_module->id()));
    return d != _globals.end() ? (*d).second.type : nullptr;
}

shared_ptr<Type> ModuleBuilder::lookupType(const std::string& id)
{
    return lookupType(std::make_shared<ID>(id));
}

shared_ptr<Type> ModuleBuilder::resolveType(shared_ptr<Type> type)
{
    if ( auto u = ast::tryCast<::hilti::type::Unknown>(type) ) {
        type = lookupType(u->id());

        if ( ! type )
            fatalError(::util::fmt("module builder cannot resolve type %s", u->id()->name()), type);
    }

    return type;
}

shared_ptr<hilti::expression::Type> ModuleBuilder::addContext(shared_ptr<Type> type, const Location& l)
{
    auto id = std::make_shared<hilti::ID>("Context", l);
    auto t = addType(id, type, false, l);
    return std::make_shared<hilti::expression::Type>(t, l);
}

shared_ptr<hilti::expression::Variable> ModuleBuilder::addLocal(shared_ptr<hilti::ID> id, shared_ptr<Type> type, shared_ptr<Expression> init, const AttributeSet& attrs, bool force_unique, const Location& l)
{
    return _addLocal(_currentBody()->stmt, id, type, init, attrs, force_unique, l);
}

shared_ptr<hilti::expression::Variable> ModuleBuilder::_addLocal(shared_ptr<statement::Block> stmt, shared_ptr<hilti::ID> id, shared_ptr<Type> type, shared_ptr<Expression> init, const AttributeSet& attrs, bool force_unique, const Location& l)
{
    auto t = _uniqueDecl(id, type, "local", &_currentFunction()->locals, (force_unique ? MAKE_UNIQUE : CHECK_UNIQUE), false);
    id = t.first;
    auto decl = t.second ? ast::checkedCast<declaration::Variable>(t.second) : nullptr;

    if ( ! decl ) {
        auto var = std::make_shared<variable::Local>(id, type, init, l);
        var->setAttributes(attrs);
        decl = std::make_shared<declaration::Variable>(id, var, l);
        stmt->addDeclaration(decl);
        _addDecl(id, type, "local", &_currentFunction()->locals, decl);
    }

    return std::make_shared<hilti::expression::Variable>(decl->variable(), l);
}

shared_ptr<hilti::expression::Variable> ModuleBuilder::addLocal(const std::string& id, shared_ptr<Type> type, shared_ptr<Expression> init, const AttributeSet& attrs, bool force_unique, const Location& l)
{
    return addLocal(std::make_shared<ID>(id, l), type, init, attrs, force_unique, l);
}

bool ModuleBuilder::hasLocal(const std::string& id)
{
    return hasLocal(std::make_shared<ID>(id));
}

bool ModuleBuilder::hasLocal(shared_ptr<hilti::ID> id)
{
    id = _normalizeID(id);

    auto func = _currentFunction();

    for ( auto p : func->function->type()->parameters() ) {
        if ( *p->id() == *id )
            return true;
    }

    auto locals = func->locals;
    auto d = locals.find(id->pathAsString(_module->id()));
    return d != locals.end();
}

shared_ptr<hilti::expression::Variable> ModuleBuilder::addTmp(shared_ptr<hilti::ID> id, shared_ptr<Type> type, shared_ptr<Expression> init, bool reuse, const Location& l)
{
    auto local = (_currentFunction()->function != nullptr);

    id = std::make_shared<ID>("__tmp_" + id->name(), id->location());

    auto t = _uniqueDecl(id, type, "tmp", local ? &_currentFunction()->locals : &_globals, (reuse ? REUSE : MAKE_UNIQUE), false);
    id = t.first;
    auto decl = t.second ? ast::checkedCast<declaration::Variable>(t.second) : nullptr;

    if ( ! decl ) {
        shared_ptr<Variable> var = nullptr;

        if ( local )
            var = std::make_shared<variable::Local>(id, type, init, l);
        else
            var = std::make_shared<variable::Global>(id, type, init, l);

        decl = std::make_shared<declaration::Variable>(id, var, l);
        _currentFunction()->bodies.front()->stmt->addDeclaration(decl);
        _addDecl(id, type, "tmp", local ? &_currentFunction()->locals : &_globals, decl);
    }

    return std::make_shared<hilti::expression::Variable>(decl->variable(), l);
}

shared_ptr<hilti::expression::Variable> ModuleBuilder::addTmp(const std::string& id, shared_ptr<Type> type, shared_ptr<Expression> init, bool reuse, const Location& l)
{
    return addTmp(std::make_shared<ID>(id, l), type, init, reuse, l);
}

void ModuleBuilder::importModule(shared_ptr<ID> id)
{
    std::string path;
    _module->import(id);

    if ( ! _module->compilerContext()->importModule(id, &path) )
        error("import error");

    _imported_paths.push_back(path);
}

void ModuleBuilder::importModule(const std::string& id)
{
    return importModule(std::make_shared<ID>(id));
}

bool ModuleBuilder::idImported(shared_ptr<ID> id) const
{
    auto m = util::strtolower(id->name());

    for ( auto i : _module->importedIDs() ) {
        if ( m == util::strtolower(i->pathAsString()) )
            return true;
    }

    return false;
}

bool ModuleBuilder::idImported(const std::string& id) const
{
    return idImported(std::make_shared<ID>(id));
}

std::list<std::string> ModuleBuilder::importedPaths() const
{
    return _imported_paths;
}

void ModuleBuilder::cacheNode(const std::string& component, const std::string& idx, shared_ptr<Node> node)
{
    auto key = component + "$" + idx;
    _node_cache[key] = node;
}

shared_ptr<Node> ModuleBuilder::lookupNode(const std::string& component, const std::string& idx)
{
    auto key = component + "$" + idx;
    auto i = _node_cache.find(key);

    return i != _node_cache.end() ? i->second : nullptr;
}

shared_ptr<BlockBuilder> ModuleBuilder::cacheBlockBuilder(const std::string& tag, cache_builder_callback cb)
{
    auto b = _currentFunction()->builders.find(tag);

    if ( b != _currentFunction()->builders.end() )
        return b->second;

    auto builder = pushBuilder(tag);
    cb();
    popBuilder(builder);

    _currentFunction()->builders.insert(std::make_pair(tag, builder));
    return builder;
}
    
void ModuleBuilder::buildForBinPAC()
{
    _buildForBinPAC = true;
}

}
}

