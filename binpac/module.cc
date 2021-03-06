
#include "module.h"
#include "statement.h"
#include "type.h"
#include "attribute.h"

using namespace binpac;

Module::Module(CompilerContext* ctx, shared_ptr<ID> id, const string& path, const Location& l)
    : ast::Module<AstInfo>(id, path, l)
{
    _context = ctx;
    auto body = std::make_shared<binpac::statement::Block>(nullptr, l);
    setBody(body);

    // Implicitly always import the BinPAC module.
    import("binpac");
}

CompilerContext* Module::context() const
{
    return _context;
}

void Module::addProperty(shared_ptr<Attribute> prop)
{
    _properties.push_back(prop);
    addChild(_properties.back());
}

std::list<shared_ptr<Attribute>> Module::properties() const
{
    std::list<shared_ptr<Attribute>> props;

    for ( auto p : _properties )
        props.push_back(p);

    return props;
}

shared_ptr<Attribute> Module::property(const string& prop) const
{
    Attribute pp(prop);

    shared_ptr<Attribute> result = nullptr;

    for( auto p : _properties ) {
        if ( pp == *p )
            result = p;
    }

    return result;
}
