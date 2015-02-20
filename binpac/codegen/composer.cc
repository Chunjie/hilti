
#include <hilti/hilti.h>

#include "composer.h"
#include "../expression.h"
#include "../grammar.h"
#include "../production.h"
#include "../statement.h"
#include "../type.h"
#include "../declaration.h"
#include "../attribute.h"
#include "../options.h"

using namespace binpac;
using namespace binpac::codegen;

static shared_ptr<hilti::Type> _hiltiTypeOutputFunction(CodeGen* cg)
{
    auto rtype = hilti::builder::function::result(hilti::builder::void_::type());
    auto arg1 = hilti::builder::function::parameter("data", hilti::builder::reference::type(::hilti::builder::bytes::type()), false, nullptr);
    auto arg2 = hilti::builder::function::parameter("obj",  hilti::builder::any::type(), false, nullptr);
    auto arg3 = hilti::builder::function::parameter("__cookie", cg->hiltiTypeCookie(), false, nullptr);

    hilti::builder::function::parameter_list params = { arg1, arg2, arg3 };
    return hilti::builder::function::type(rtype, params, ::hilti::type::function::HILTI_C);
}

// A class collecting the current set of composer arguments.
class binpac::codegen::ComposerState
{
public:
    ComposerState(shared_ptr<binpac::type::Unit> unit,
           shared_ptr<hilti::Expression> self = nullptr,
           shared_ptr<hilti::Expression> output_function = nullptr,
           shared_ptr<hilti::Expression> cookie = nullptr
           );

    shared_ptr<hilti::Expression> hiltiArguments() const;

    shared_ptr<ComposerState> clone() const {
        auto state = std::make_shared<ComposerState>(unit,
                                                   self,
                                                   output_function,
                                                   cookie
                                                   );

        return state;
    }

    shared_ptr<binpac::type::Unit> unit;
    shared_ptr<hilti::Expression> self;
    shared_ptr<hilti::Expression> output_function;
    shared_ptr<hilti::Expression> cookie;
};

ComposerState::ComposerState(shared_ptr<binpac::type::Unit> arg_unit,
               shared_ptr<hilti::Expression> arg_self,
               shared_ptr<hilti::Expression> arg_output_function,
               shared_ptr<hilti::Expression> arg_cookie
               )
{
    unit = arg_unit;
    self = (arg_self ? arg_self : hilti::builder::id::create("__self"));
    output_function = (arg_output_function ? arg_output_function : hilti::builder::id::create("__outfunc"));
    cookie = (arg_cookie ? arg_cookie : hilti::builder::id::create("__cookie"));
}

shared_ptr<hilti::Expression> ComposerState::hiltiArguments() const
{
    hilti::builder::tuple::element_list args;

    args = { self, output_function, cookie };
    return hilti::builder::tuple::create(args, unit->location());
}

Composer::Composer(CodeGen* cg)
    : CGVisitor<shared_ptr<hilti::Expression>, shared_ptr<type::unit::item::Field>>(cg, "Composer")
{
}

Composer::~Composer()
{
}

shared_ptr<binpac::type::Unit> Composer::unit() const
{
    return state()->unit;
}

shared_ptr<hilti::Expression> Composer::hiltiObject(shared_ptr<type::unit::item::Field> field) const
{
    // TODO: Implementation and usage of this method is quite a hack. Can we
    // streamline that?
    shared_ptr<hilti::Expression> val = nullptr;

    if ( field ) {
        val = cg()->hiltiItemGet(state()->self, field);
        val = cg()->hiltiApplyAttributesToValue(val, field->attributes(), true);
    }

    else if ( _object )
        val = _object;

    else {
        assert(arg1());
        val = cg()->hiltiItemGet(state()->self, arg1());
        val = cg()->hiltiApplyAttributesToValue(val, arg1()->attributes(), true);
    }

    return val;
}

shared_ptr<ComposerState> Composer::state() const
{
    assert(_states.size());
    return _states.back();
}

void Composer::pushState(shared_ptr<ComposerState> state)
{
    _states.push_back(state);
}

void Composer::popState()
{
    assert(_states.size());
    _states.pop_back();
}

void Composer::compose(shared_ptr<Node> node, shared_ptr<type::unit::item::Field> field)
{
    compose(node, nullptr, field);
}

void Composer::compose(shared_ptr<Node> node, shared_ptr<hilti::Expression> obj, shared_ptr<type::unit::item::Field> field)
{
    auto prod = ast::tryCast<Production>(node);

    if ( ! prod || prod->atomic() )
        return _hiltiCompose(node, obj, field);

    // We wrap productions into their own functions to handle cycles
    // correctly.

    // If we got an obj, and are parsing a unit, we need push a new state.
    auto child = ast::tryCast<production::ChildGrammar>(node);
    if ( child && obj) {
        auto pstate = std::make_shared<ComposerState>(child->childType(), obj);
        pushState(pstate);
        obj = hilti::builder::id::create("__self");
    }

    auto name = util::fmt("__compose_%s_%s", state()->unit->id()->name(), prod->symbol());
    name = util::strreplace(name, ":", "_");

    auto func = cg()->moduleBuilder()->lookupNode("compose-func", name);

    if ( ! func ) {
        // Don't have the function yet, create it.
        auto vtype = std::make_shared<hilti::type::Unknown>();
        auto nfunc = _newComposeFunction(name, state()->unit);
        cg()->moduleBuilder()->cacheNode("compose-func", name, nfunc);

        _hiltiCompose(node, obj, field);

        _finishComposeFunction();

        func = nfunc;
    }

    // Call the function.
    auto efunc = ast::checkedCast<hilti::expression::Function>(func);
    _hiltiCallComposeFunction(state()->unit, efunc);

    if ( child && obj ) {
        popState();
    }

}

shared_ptr<hilti::Expression> Composer::hiltiCreateHostFunction(shared_ptr<type::Unit> unit)
{
    auto utype = cg()->hiltiType(unit);

    auto rtype = hilti::builder::function::result(hilti::builder::void_::type());
    auto arg1 = hilti::builder::function::parameter("__self", utype, false, nullptr);
    auto arg2 = hilti::builder::function::parameter("__outfunc", _hiltiTypeOutputFunction(cg()), false, nullptr);
    auto arg3 = hilti::builder::function::parameter("__cookie", cg()->hiltiTypeCookie(), false, nullptr);

    hilti::builder::function::parameter_list args = { arg1, arg2, arg3 };

    string name = util::fmt("compose_%s", unit->id()->name());

    auto func = cg()->moduleBuilder()->pushFunction(name, rtype, args);
    cg()->moduleBuilder()->exportID(name);

    auto self = hilti::builder::id::create("__self");
    auto outfunc = hilti::builder::id::create("__outfunc");
    auto cookie = hilti::builder::id::create("__cookie");

    auto pstate = std::make_shared<ComposerState>(unit, self, outfunc, cookie);
    pushState(pstate);

    auto pfunc = cg()->hiltiComposeFunction(unit);

    if ( cg()->options().debug > 0 ) {
        _hiltiDebug(unit->id()->name());
        cg()->builder()->debugPushIndent();
    }

    _hiltiCallComposeFunction(unit, pfunc);

    if ( cg()->options().debug > 0 )
        cg()->builder()->debugPopIndent();

    popState();

    return cg()->moduleBuilder()->popFunction();
}

shared_ptr<hilti::Expression> Composer::hiltiCreateComposeFunction(shared_ptr<type::Unit> unit)
{
    auto grammar = unit->grammar();
    assert(grammar);

    auto name = util::fmt("compose_%s_internal", grammar->name().c_str());

    auto n = cg()->moduleBuilder()->lookupNode("create-compose-function", name);

    if ( n )
        return ast::checkedCast<hilti::Expression>(n);

    auto func = _newComposeFunction(name, unit);
    cg()->moduleBuilder()->cacheNode("create-compose-function", name, func);

    _startingUnit();
    _hiltiCompose(grammar->root(), nullptr, nullptr);
    _finishedUnit();

    _finishComposeFunction();

    return func;
}

shared_ptr<hilti::Expression> Composer::_newComposeFunction(const string& name, shared_ptr<type::Unit> unit, shared_ptr<hilti::Type> value_type)
{
    auto utype = cg()->hiltiType(unit);

    auto rtype = hilti::builder::function::result(hilti::builder::void_::type());
    auto arg1 = hilti::builder::function::parameter("__self", utype, false, nullptr);
    auto arg2 = hilti::builder::function::parameter("__outfunc", _hiltiTypeOutputFunction(cg()), false, nullptr);
    auto arg3 = hilti::builder::function::parameter("__cookie", cg()->hiltiTypeCookie(), false, nullptr);

    hilti::builder::function::parameter_list params = { arg1, arg2, arg3 };
    auto func = cg()->moduleBuilder()->pushFunction(name, rtype, params);

    pushState(std::make_shared<ComposerState>(unit));

    return std::make_shared<hilti::expression::Function>(func->function(), func->location());
}

void Composer::_finishComposeFunction()
{
    popState();
    cg()->moduleBuilder()->popFunction();
}

void Composer::_hiltiCompose(shared_ptr<Node> node, shared_ptr<hilti::Expression> obj, shared_ptr<type::unit::item::Field> f)
{
    shared_ptr<type::unit::item::Field> field;
    shared_ptr<hilti::builder::BlockBuilder> cont;
    shared_ptr<hilti::builder::BlockBuilder> true_;
    shared_ptr<hilti::builder::BlockBuilder> sync_cont;

    auto prod = ast::tryCast<Production>(node);

    if ( prod )
        field = ast::tryCast<type::unit::item::Field>(prod->pgMeta()->field);

    if ( field && ! field->forComposing() )
        // Skip
        return;

    if ( field && field->condition() ) {
        // Evaluate if() condition.
        auto blocks = cg()->builder()->addIf(cg()->hiltiExpression(field->condition()));
        true_ = std::get<0>(blocks);
        cont = std::get<1>(blocks);
        cg()->moduleBuilder()->pushBuilder(true_);
    }

    if ( field && field->attributes()->has("try") ) {
        // TODO: If field is not set, ignore it.
        internalError("compose for &try not implemented");
    }

    if ( field )
        _hiltiRunFieldHooks(field);

    setDefaultResult(0); // Ignored.
    shared_ptr<hilti::Expression> old_object = _object;

    if ( obj )
        _object = obj;
    else if ( field )
        _object = 0;

    processOne(node, f);
    _object = old_object;

    if ( true_ ) {
        cg()->builder()->addInstruction(hilti::instruction::flow::Jump, cont->block());
        cg()->moduleBuilder()->popBuilder(true_);
        cg()->moduleBuilder()->pushBuilder(cont);
    }
}

void Composer::_hiltiComposeContainer(shared_ptr<hilti::Expression> value, shared_ptr<Production> body, shared_ptr<type::unit::item::field::Container> container)
{
    auto iterable = ast::type::checkedTrait<type::trait::Iterable>(container->fieldType());
    assert(iterable);

    auto i = cg()->builder()->addTmp("i", cg()->hiltiType(iterable->iterType()));
    auto elem = cg()->builder()->addTmp("elem", cg()->hiltiType(iterable->elementType()));
    auto end = cg()->builder()->addTmp("end", cg()->hiltiType(iterable->iterType()));
    auto atend = cg()->builder()->addTmp("atend", hilti::builder::boolean::type());

    auto cont = cg()->moduleBuilder()->newBuilder("container-done");
    auto compose_one = cg()->moduleBuilder()->newBuilder("container-compose-one");
    auto loop = cg()->moduleBuilder()->newBuilder("container-loop");

    cg()->builder()->addInstruction(i, hilti::instruction::operator_::Begin, value);
    cg()->builder()->addInstruction(end, hilti::instruction::operator_::End, value);
    cg()->builder()->addInstruction(hilti::instruction::flow::Jump, loop->block());

    cg()->moduleBuilder()->pushBuilder(loop);
    cg()->builder()->addInstruction(atend, hilti::instruction::operator_::Equal, i, end);
    cg()->builder()->addInstruction(hilti::instruction::flow::IfElse, atend, cont->block(), compose_one->block());
    cg()->moduleBuilder()->popBuilder(loop);

    cg()->moduleBuilder()->pushBuilder(compose_one);

    cg()->builder()->addInstruction(elem, hilti::instruction::operator_::Deref, i);
    compose(body, elem, container->field());

    cg()->builder()->addInstruction(i, hilti::instruction::operator_::Incr, i);
    cg()->builder()->addInstruction(hilti::instruction::flow::Jump, loop->block());

    cg()->moduleBuilder()->popBuilder(compose_one);

    cg()->moduleBuilder()->pushBuilder(cont);
}

void Composer::_hiltiCallComposeFunction(shared_ptr<binpac::type::Unit> unit, shared_ptr<hilti::Expression> func)
{
    cg()->builder()->addInstruction(hilti::instruction::flow::CallVoid, func, state()->hiltiArguments());
}

void Composer::_hiltiRunFieldHooks(shared_ptr<type::unit::item::Field> field)
{
    assert(field);
    cg()->hiltiRunFieldHooks(state()->unit, field, state()->self, true, state()->cookie);
}

void Composer::_hiltiFilterOutput()
{
    // XXX
}

void Composer::_startingProduction(shared_ptr<Production> p, shared_ptr<type::unit::item::Field> field)
{
    cg()->builder()->addComment(util::fmt("Production: %s", util::strtrim(p->render().c_str())));
    cg()->builder()->addComment("");

    _hiltiDebugVerbose(util::fmt("composing %s", util::strtrim(p->render().c_str())));
}

void Composer::_startingUnit()
{
    cg()->hiltiRunHook(state()->unit, state()->self, cg()->hookForUnit(state()->unit, "%init", true), {}, nullptr, false, nullptr, true, state()->cookie);
}

void Composer::_finishedUnit()
{
    cg()->hiltiRunHook(state()->unit, state()->self, cg()->hookForUnit(state()->unit, "%done", true), {}, nullptr, false, nullptr, true, state()->cookie);
}

void Composer::_finishedProduction(shared_ptr<Production> p)
{
    cg()->builder()->addComment("");
    _object = nullptr;
}

void Composer::hiltiPack(shared_ptr<type::unit::item::Field> field,
                         shared_ptr<hilti::Expression> op1,
                         shared_ptr<hilti::Expression> op2,
                         shared_ptr<hilti::Expression> op3)
{
    auto rbytes = hilti::builder::reference::type(hilti::builder::bytes::type());
    auto packed = cg()->builder()->addTmp("packed", rbytes);
    cg()->builder()->addInstruction(packed, hilti::instruction::operator_::Pack, op1, op2, op3);
    _hiltiDataComposed(packed, field);
}

void Composer::_hiltiDataComposed(shared_ptr<hilti::Expression> data, shared_ptr<type::unit::item::Field> field)
{
    assert(field);

    if ( cg()->options().debug > 0 && ! ast::isA<type::Unit>(field->type()))
        cg()->builder()->addDebugMsg("binpac-compose", util::fmt("%s = %%s", field->id()->name()), data);

    auto obj = (field->anonymous() ? hilti::builder::reference::createNull() : hiltiObject());

    hilti::builder::tuple::element_list args = { data, obj, state()->cookie };
    cg()->builder()->addInstruction(hilti::instruction::flow::CallVoid, state()->output_function, hilti::builder::tuple::create(args));
}

void Composer::_hiltiDebug(const string& msg)
{
    if ( cg()->options().debug > 0 )
        cg()->builder()->addDebugMsg("binpac-compose", msg);
}

void Composer::_hiltiDebugVerbose(const string& msg)
{
    if ( cg()->options().debug > 0 )
        cg()->builder()->addDebugMsg("binpac-compose-verbose", string("- ") + msg);
}

void Composer::_hiltiComposeError(shared_ptr<hilti::Expression> excpt)
{
    _hiltiDebugVerbose("retriggering compose error");
    cg()->builder()->addInstruction(hilti::instruction::exception::Throw, excpt);
}

void Composer::_hiltiComposeError(const string& msg)
{
    _hiltiDebugVerbose("triggering compose error");

    auto etype = builder::type::byName("BinPACHilti::ComposeError");
    auto excpt = cg()->builder()->addTmp("__compose_error_excpt", hilti::builder::reference::type(etype));

    cg()->builder()->addInstruction(excpt,
                                    hilti::instruction::exception::NewWithArg,
                                    hilti::builder::type::create(etype),
                                    hilti::builder::string::create(msg));

    cg()->builder()->addInstruction(hilti::instruction::exception::Throw, excpt);
}

void Composer::visit(expression::Ctor* c)
{
    auto field = arg1();
    assert(field);

    shared_ptr<hilti::Expression> value;
    processOne(c->ctor(), &value, field);
}

void Composer::visit(expression::Constant* c)
{
    auto field = arg1();
    assert(field);

    shared_ptr<hilti::Expression> value;
    processOne(c->constant(), &value, field);
}

void Composer::visit(expression::Type* t)
{
    auto field = arg1();
    assert(field);

    auto type = ast::checkedCast<type::TypeType>(t->type())->typeType();

    shared_ptr<hilti::Expression> value;
    processOne(type, &value, field);
}

void Composer::visit(constant::Integer* i)
{
    auto field = arg1();
    assert(field);

    auto t = ast::checkedCast<type::Integer>(i->type());

    cg()->builder()->addComment(util::fmt("Integer constant: %s", field->id()->name()));

    auto e = std::make_shared<expression::Constant>(i->sharedPtr<constant::Integer>());
    auto d = cg()->hiltiExpression(e);

    auto byteorder = field->inheritedProperty("byteorder");
    auto fmt = cg()->hiltiIntPackFormat(t->width(), t->signed_(), byteorder);

    hiltiPack(field, d, fmt);
}

void Composer::visit(ctor::Bytes* b)
{
    auto field = arg1();
    assert(field);

    cg()->builder()->addComment(util::fmt("Bytes ctor: %s", field->id()->name()));

    auto e = std::make_shared<expression::Ctor>(b->sharedPtr<ctor::Bytes>());
    auto d = cg()->hiltiExpression(e);
    _hiltiDataComposed(d, field);
}

void Composer::visit(ctor::RegExp* r)
{
    auto field = arg1();
    assert(field);

    cg()->builder()->addComment(util::fmt("RegExp ctor: %s", field->id()->name()));

    _hiltiDataComposed(hiltiObject(field), field);
}

void Composer::visit(production::Boolean* b)
{
    auto field = b->pgMeta()->field;

    _startingProduction(b->sharedPtr<Production>(), field);

    internalError("composing production::Boolean not implemented");

    _finishedProduction(b->sharedPtr<Production>());
}

void Composer::visit(production::ChildGrammar* c)
{
    auto field = c->pgMeta()->field;
    assert(field);

    _startingProduction(c->sharedPtr<Production>(), field);

    auto child = c->childType();
    auto child_func = cg()->hiltiComposeFunction(child);
    auto child_self = (_object ? hilti::builder::id::create("__self") : hiltiObject(field));

    auto cstate = state()->clone();
    cstate->unit = child;
    cstate->self = child_self;
    pushState(cstate);

    if ( cg()->options().debug > 0 ) {
        if ( ! field->anonymous() )
            _hiltiDebug(field->id()->name());
        else
            _hiltiDebug(field->type()->render());

        cg()->builder()->debugPushIndent();
    }

    _hiltiCallComposeFunction(child, child_func);

    if ( cg()->options().debug > 0 )
        cg()->builder()->debugPopIndent();

    popState();

    _finishedProduction(c->sharedPtr<Production>());
}

void Composer::visit(production::Enclosure* c)
{
    auto field = c->pgMeta()->field;

    _startingProduction(c->sharedPtr<Production>(), field);

    compose(c->child(), field);

    _finishedProduction(c->sharedPtr<Production>());
}

void Composer::visit(production::Counter* c)
{
    auto field = c->pgMeta()->field;

    _startingProduction(c->sharedPtr<Production>(), field);

    if ( auto cont = ast::tryCast<type::unit::item::field::Container>(field) )
        _hiltiComposeContainer(hiltiObject(field), c->body(), cont);
    else
        internalError(::util::fmt("composing counter of field %s not implemented", field->render()));

    _finishedProduction(c->sharedPtr<Production>());
}

void Composer::visit(production::ByteBlock* c)
{
    auto field = c->pgMeta()->field;

    _startingProduction(c->sharedPtr<Production>(), field);

    internalError("composing production::ByteBlock not implemented");

    _finishedProduction(c->sharedPtr<Production>());
}

void Composer::visit(production::Epsilon* e)
{
    auto field = e->pgMeta()->field;

    _startingProduction(e->sharedPtr<Production>(), field);

    _finishedProduction(e->sharedPtr<Production>());
}

void Composer::visit(production::Literal* l)
{
    auto field = l->pgMeta()->field;

    _startingProduction(l->sharedPtr<Production>(), field);

    cg()->builder()->addComment(util::fmt("Literal: %s (id %d)", field->id()->name(), l->tokenID()));

    compose(l->literal(), field);

    _finishedProduction(l->sharedPtr<Production>());
}

void Composer::visit(production::LookAhead* l)
{
    auto field = l->pgMeta()->field;

    _startingProduction(l->sharedPtr<Production>(), field);

    internalError("composing production::LookAhead not implemented");

    _finishedProduction(l->sharedPtr<Production>());
}

void Composer::visit(production::Sequence* s)
{
    auto field = s->pgMeta()->field;

    _startingProduction(s->sharedPtr<Production>(), field);

    for ( auto p : s->sequence() )
        compose(p);

    _finishedProduction(s->sharedPtr<Production>());
}

void Composer::visit(production::Switch* s)
{
    auto field = s->pgMeta()->field;
    assert(field);

    _startingProduction(s->sharedPtr<Production>(), field);

    auto cont = cg()->moduleBuilder()->newBuilder("switch-cont");

    hilti::builder::BlockBuilder::case_list cases;

    // Build the branches.
    for ( auto c : s->alternatives() ) {
        auto builder = cg()->moduleBuilder()->pushBuilder("switch-case");
        compose(c.second);
        cg()->builder()->addInstruction(hilti::instruction::flow::Jump, cont->block());
        cg()->moduleBuilder()->popBuilder(builder);

        for ( auto e : c.first ) {
            auto expr = cg()->hiltiExpression(e, s->expression()->type());
            cases.push_back(std::make_pair(expr, builder));
        }
    }

    // Build default branch, raising a parse error if none given.
    auto default_ = cg()->moduleBuilder()->pushBuilder("switch-default");

    if ( s->default_() )
        compose(s->default_());
    else
        _hiltiComposeError("no matching switch case");

    cg()->builder()->addInstruction(hilti::instruction::flow::Jump, cont->block());
    cg()->moduleBuilder()->popBuilder(default_);

    auto expr = cg()->hiltiExpression(s->expression(), s->expression()->type());
    cg()->builder()->addSwitch(expr, default_, cases);

    cg()->moduleBuilder()->pushBuilder(cont);

    _finishedProduction(s->sharedPtr<Production>());
}

void Composer::visit(production::Variable* v)
{
    auto field = v->pgMeta()->field;

    _startingProduction(v->sharedPtr<Production>(), field);

    cg()->builder()->addComment(util::fmt("Variable: %s", field->id()->name()));

    compose(v->type(), field);

    _finishedProduction(v->sharedPtr<Production>());
}

void Composer::visit(production::Loop* l)
{
    auto field = l->pgMeta()->field;

    _startingProduction(l->sharedPtr<Production>(), field);

    if ( auto cont = ast::tryCast<type::unit::item::field::Container>(field) ) {
        _hiltiComposeContainer(hiltiObject(field), l->body(), cont);

        if ( auto until = field->attributes()->lookup("until") ) {
            // TODO: What?
        }
    }

    else
        internalError(::util::fmt("composing loop of field %s not implemented", field->render()));

    _finishedProduction(l->sharedPtr<Production>());
}

void Composer::visit(type::Bytes* b)
{
    auto field = arg1();
    assert(field);

    _hiltiDataComposed(hiltiObject(), field);

    if ( auto until = field->attributes()->lookup("until") ) {
        auto eod = cg()->hiltiExpression(until->value());
        _hiltiDataComposed(eod, field);
    }
}

void Composer::visit(type::Integer* i)
{
    auto field = arg1();
    assert(field);

    auto byteorder = field->inheritedProperty("byteorder");
    auto fmt = cg()->hiltiIntPackFormat(i->width(), i->signed_(), byteorder);

    hiltiPack(field, hiltiObject(), fmt);
}

void Composer::visit(type::Address* i)
{
    auto field = arg1();
    assert(field);

    auto v4 = field->attributes()->has("ipv4");
    auto v6 = field->attributes()->has("ipv6");
    assert((v4 || v6) && ! (v4 && v6));

    auto byteorder = field->inheritedProperty("byteorder");
    auto hltbo = byteorder ? cg()->hiltiExpression(byteorder) : hilti::builder::id::create("BinPAC::ByteOrder::Big");

    string big, little, host;

    if ( v4 ) {
        host = "IPv4";
        little= "IPv4Little";
        big = "IPv4Big";
        // FIXME: We don't have HILTI enums for little endian addresses.
    }
    else {
        host = "IPv6";
        little= "IPv6Little";
        big = "IPv6Big";
    }

    auto t1 = hilti::builder::tuple::create({
        hilti::builder::id::create("BinPAC::ByteOrder::Little"),
        hilti::builder::id::create(string("Hilti::Packed::") + little) });

    auto t2 = hilti::builder::tuple::create({
        hilti::builder::id::create("BinPAC::ByteOrder::Big"),
        hilti::builder::id::create(string("Hilti::Packed::") + big) });

    auto t3 = hilti::builder::tuple::create({
        hilti::builder::id::create("BinPAC::ByteOrder::Host"),
        hilti::builder::id::create(string("Hilti::Packed::") + host) });

    auto tuple = hilti::builder::tuple::create({ t1, t2, t3 });
    auto fmt = cg()->moduleBuilder()->addTmp("fmt", hilti::builder::type::byName("Hilti::Packed"));
    cg()->builder()->addInstruction(fmt, hilti::instruction::Misc::SelectValue, hltbo, tuple);

    hiltiPack(field, hiltiObject(), fmt);
}

void Composer::visit(type::Bitfield* btype)
{
    auto field = arg1();
    assert(field);

    auto width = btype->width();
    auto byteorder = field->inheritedProperty("byteorder");

    auto bval = hiltiObject();
    auto ival = cg()->builder()->addTmp("ival", hilti::builder::integer::type(width));
    auto elem = cg()->builder()->addTmp("elem", hilti::builder::integer::type(width));

    int i = 0;

    for ( auto b : btype->bits() ) {
        cg()->builder()->addInstruction(elem, hilti::instruction::tuple::Index, bval, hilti::builder::integer::create(i++));
        auto nelem = cg()->hiltiApplyAttributesToValue(elem, btype->attributes());

        auto bits = cg()->hiltiInsertBitsIntoInteger(nelem, std::make_shared<type::Integer>(width, false),
                                                     btype->bitOrder(),
                                                     hilti::builder::integer::create(b->lower()),
                                                     hilti::builder::integer::create(b->upper()));

        cg()->builder()->addInstruction(ival, hilti::instruction::integer::Or, ival, bits);
    }

    auto fmt = cg()->hiltiIntPackFormat(width, false, byteorder);
    hiltiPack(field, ival, fmt);
}
