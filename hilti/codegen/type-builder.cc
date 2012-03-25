
#include "../hilti.h"

#include "type-builder.h"
#include "codegen.h"

#include "../../libhilti/rtti.h"
#include "../../libhilti/port.h"
#include "../../libhilti/enum.h"

using namespace hilti;
using namespace codegen;

PointerMap::PointerMap(CodeGen* cg, hilti::Type* type)
{
    _cg = cg;

    auto tl = type::asTrait<type::trait::TypeList>(type);

    if ( ! tl )
        cg->internalError("Type passed to PointerMap TypeList ctor is not a TypeList");

    std::vector<llvm::Type*> fields;

    if ( type::hasTrait<type::trait::GarbageCollected>(type) )
        fields.push_back(cg->llvmLibType("hlt.ref_cnt"));

    std::list<int> ptr_indices;

    for ( auto t : tl->typeList() ) {
        if ( type::hasTrait<type::trait::GarbageCollected>(t) )
            ptr_indices.push_back(fields.size());
        fields.push_back(cg->llvmType(t));
    }

    auto llvm_fields = cg->llvmTypeStruct("ptrmap", fields);

    auto null = cg->llvmConstNull(cg->llvmTypePtr(llvm_fields));
    auto zero = cg->llvmGEPIdx(0);

    for ( auto i : ptr_indices ) {
        auto offset = cg->llvmGEP(null, zero, cg->llvmGEPIdx(i));
        _offsets.push_back(llvm::ConstantExpr::getPtrToInt(offset, cg->llvmTypeInt(16)));
    }
}

llvm::Constant* PointerMap::llvmMap()
{
    _offsets.push_back(_cg->llvmConstInt(HLT_PTR_MAP_END, 16));
    auto cval = _cg->llvmConstArray(_offsets);
    return _cg->llvmAddConst("ptrmap", cval);
}

TypeBuilder::TypeBuilder(CodeGen* cg) : CGVisitor<TypeInfo *>(cg, "codegen::TypeBuilder")
{
}

TypeBuilder::~TypeBuilder()
{
}

TypeInfo* TypeBuilder::_typeInfo(shared_ptr<hilti::Type> type)
{
    TypeInfo* result;
    bool success = processOne(type, &result);
    assert(success);
    return result;
}

shared_ptr<TypeInfo> TypeBuilder::typeInfo(shared_ptr<hilti::Type> type)
{
    auto i = _ti_cache.find(type);

    if ( i != _ti_cache.end() )
        // Already computed.
        return i->second;

    setDefaultResult(nullptr);

    shared_ptr<TypeInfo> ti(_typeInfo(type));

    if ( ! ti->init_val && ti->lib_type.size() )
        ti->init_val = cg()->llvmConstNull(cg()->llvmTypePtr(cg()->llvmLibType(ti->lib_type)));

    if ( ! ti->llvm_type && ti->lib_type.size() ) {
        ti->llvm_type = cg()->llvmLibType(ti->lib_type);

        if ( ast::isA<type::Reference>(type) )
            ti->llvm_type = cg()->llvmTypePtr(ti->llvm_type);
    }

    if ( ti->llvm_type && ! ti->init_val && ast::isA<type::ValueType>(type) )
        ti->init_val = cg()->llvmConstNull(ti->llvm_type);

    if ( ! ti )
        internalError(::util::fmt("typeInfo() not available for type '%s'", type->render().c_str()));

    if ( ! ti->llvm_type && ti->init_val )
        ti->llvm_type = ti->init_val->getType();

    if ( ti->id == HLT_TYPE_ERROR )
        internalError(::util::fmt("type info for type %s does not set an RTTI id", type->render().c_str()));

    if ( ast::isA<type::HiltiType>(type) && ! ti->llvm_type )
        internalError(::util::fmt("type info for %s does not define an llvm_type", ti->name.c_str()));

    if ( ast::isA<type::ValueType>(type) && ! ti->init_val )
        internalError(::util::fmt("type info for %s does not define an init value", ti->name.c_str()));

#if 0
    if ( type::hasTrait<type::trait::GarbageCollected>(type) && ! ti->ptr_map )
        internalError(::util::fmt("type info for %s does not define a ptr map", ti->name.c_str()));

    if ( type::hasTrait<type::trait::GarbageCollected>(type) && ! ti->dtor )
        internalError(::util::fmt("type info for %s does not define a dtor function", ti->name.c_str()));
#endif

    if ( ! ti->name.size() )
        ti->name = type->render();

    _ti_cache.insert(make_tuple(type, ti));
    return ti;
}

llvm::Type* TypeBuilder::llvmType(shared_ptr<Type> type)
{
    auto ti = typeInfo(type);
    assert(ti && ti->llvm_type);
    return ti->llvm_type;
}

llvm::Constant* TypeBuilder::_lookupFunction(const string& name)
{
    if ( name.size() == 0 )
        return cg()->llvmConstNull(cg()->llvmTypePtr());

    auto func = cg()->llvmFunction(name);
    return llvm::ConstantExpr::getBitCast(func, cg()->llvmTypePtr());
}

llvm::Constant* TypeBuilder::llvmRtti(shared_ptr<hilti::Type> type)
{
    std::vector<llvm::Constant*> vals;

    int gc = 0;

    auto rt = ast::as<type::Reference>(type);

    if ( type::hasTrait<type::trait::GarbageCollected>(type)
         || (rt && type::hasTrait<type::trait::GarbageCollected>(rt->argType())) )
        gc = 1;

    auto ti = typeInfo(type);

    // Ok if not the right type here.
    shared_ptr<type::trait::Parameterized> ptype = std::dynamic_pointer_cast<type::trait::Parameterized>(ti->type);

    vals.push_back(cg()->llvmConstInt(ti->id, 16));
    vals.push_back(llvm::ConstantExpr::getTrunc(cg()->llvmSizeOf(ti->init_val), cg()->llvmTypeInt(16)));
    vals.push_back(cg()->llvmConstAsciizPtr(ti->name));
    vals.push_back(cg()->llvmConstInt((ptype ? ptype->parameters().size() : 0), 16));
    vals.push_back(cg()->llvmConstInt(gc, 16));
    vals.push_back(ti->aux ? ti->aux : cg()->llvmConstNull(cg()->llvmTypePtr()));
    vals.push_back(ti->ptr_map ? ti->ptr_map : cg()->llvmConstNull(cg()->llvmTypePtr()));
    vals.push_back(_lookupFunction(ti->to_string));
    vals.push_back(_lookupFunction(ti->to_int64));
    vals.push_back(_lookupFunction(ti->to_double));
    vals.push_back(_lookupFunction(ti->hash));
    vals.push_back(_lookupFunction(ti->equal));
    vals.push_back(_lookupFunction(ti->blockable));
    vals.push_back(ti->dtor_func ? ti->dtor_func : _lookupFunction(ti->dtor));
    vals.push_back(ti->obj_dtor_func ? ti->obj_dtor_func : _lookupFunction(ti->obj_dtor));
    vals.push_back(ti->cctor_func ? ti->cctor_func : _lookupFunction(ti->cctor));

    if ( ptype ) {
        // Add type parameters.
        for ( auto p : ptype->parameters() ) {
            auto pt = dynamic_cast<type::trait::parameter::Type *>(p.get());
            auto pi = dynamic_cast<type::trait::parameter::Integer *>(p.get());
            auto pe = dynamic_cast<type::trait::parameter::Enum *>(p.get());
            auto pa = dynamic_cast<type::trait::parameter::Attribute *>(p.get());

            if ( pt )
                vals.push_back(cg()->llvmRtti(pt->type()));

            else if ( pi )
                vals.push_back(cg()->llvmConstInt(pi->value(), 64));

            else if ( pe ) {
                auto expr = cg()->hiltiModule()->body()->scope()->lookup(pe->label());
                assert(expr);
                auto val = ast::as<type::Enum>(expr->type())->labelValue(pe->label());
                vals.push_back(cg()->llvmConstInt(val, 64));
            }

            else if ( pa )
                vals.push_back(cg()->llvmConstAsciizPtr(pa->value()));

            else
                internalError("unexpected type parameter type");
        }
    }

    return cg()->llvmConstStruct(vals);
}

void TypeBuilder::visit(type::Any* a)
{
    TypeInfo* ti = new TypeInfo(a);
    ti->id = HLT_TYPE_ANY;
    ti->c_prototype = "void *";
    ti->llvm_type = cg()->llvmTypePtr();
    ti->pass_type_info = true;
    setResult(ti);
}

void TypeBuilder::visit(type::Void* a)
{
    TypeInfo* ti = new TypeInfo(a);
    ti->id = HLT_TYPE_VOID;
    ti->c_prototype = "void";
    ti->llvm_type = cg()->llvmTypeVoid();
    setResult(ti);
}

void TypeBuilder::visit(type::String* s)
{
    TypeInfo* ti = new TypeInfo(s);
    ti->id = HLT_TYPE_STRING;
    ti->cctor_func = cg()->llvmLibFunction("__hlt_object_ref");
    ti->dtor_func = cg()->llvmLibFunction("__hlt_object_unref");
    ti->c_prototype = "hlt_string";
    ti->init_val = cg()->llvmConstNull(cg()->llvmTypeString());
    ti->to_string = "hlt::string_to_string";
    // ti->hash = "hlt::string_hash";
    // ti->equal = "hlt::string_equal";
    setResult(ti);
}

void TypeBuilder::visit(type::Bool* b)
{
    TypeInfo* ti = new TypeInfo(b);
    ti->id = HLT_TYPE_BOOL;
    ti->c_prototype = "int8_t";
    ti->init_val = cg()->llvmConstInt(0, 1);
    ti->to_string = "hlt::bool_to_string";
    ti->to_int64 = "hlt::bool_to_int64";
    // ti->hash = "hlt::bool_hash";
    // ti->equal = "hlt::bool_equal";
    setResult(ti);
}

void TypeBuilder::visit(type::Exception* e)
{
    TypeInfo* ti = new TypeInfo(e);
    ti->id = HLT_TYPE_EXCEPTION;
    ti->dtor = "hlt::exception_dtor";
    ti->c_prototype = "hlt_exception*";
    ti->lib_type = "hlt.exception";
    // ti->to_string = "hlt::exception_to_string";
    // ti->hash = "hlt::bool_hash";
    // ti->equal = "hlt::bool_equal";
    setResult(ti);
}

void TypeBuilder::visit(type::iterator::Bytes* i)
{
    TypeInfo* ti = new TypeInfo(i);
    ti->id = HLT_TYPE_ITERATOR_BYTES;
    ti->dtor = "hlt::iterator_bytes_dtor";
    ti->cctor = "hlt::iterator_bytes_cctor";
    ti->c_prototype = "hlt_iterator_bytes";
    ti->init_val = cg()->llvmConstNull(cg()->llvmLibType("hlt.iterator.bytes"));
    setResult(ti);
}

void TypeBuilder::visit(type::iterator::List* i)
{
    TypeInfo* ti = new TypeInfo(i);
    ti->id = HLT_TYPE_ITERATOR_LIST;
    ti->dtor = "hlt::iterator_list_dtor";
    ti->cctor = "hlt::iterator_list_cctor";
    ti->c_prototype = "hlt_iterator_list";
    ti->init_val = cg()->llvmConstNull(cg()->llvmLibType("hlt.iterator.list"));
    setResult(ti);
}

void TypeBuilder::visit(type::iterator::Set* i)
{
    TypeInfo* ti = new TypeInfo(i);
    ti->id = HLT_TYPE_ITERATOR_SET;
    ti->dtor = "hlt::iterator_set_dtor";
    ti->cctor = "hlt::iterator_set_cctor";
    ti->c_prototype = "hlt_iterator_set";
    ti->init_val = cg()->llvmConstNull(cg()->llvmLibType("hlt.iterator.set"));
    setResult(ti);
}

void TypeBuilder::visit(type::iterator::Map* i)
{
    TypeInfo* ti = new TypeInfo(i);
    ti->id = HLT_TYPE_ITERATOR_MAP;
    ti->dtor = "hlt::iterator_map_dtor";
    ti->cctor = "hlt::iterator_map_cctor";
    ti->c_prototype = "hlt_iterator_map";
    ti->init_val = cg()->llvmConstNull(cg()->llvmLibType("hlt.iterator.map"));
    setResult(ti);
}

void TypeBuilder::visit(type::iterator::Vector* i)
{
    TypeInfo* ti = new TypeInfo(i);
    ti->id = HLT_TYPE_ITERATOR_VECTOR;
    ti->dtor = "hlt::iterator_vector_dtor";
    ti->cctor = "hlt::iterator_vector_cctor";
    ti->c_prototype = "hlt_iterator_vector";
    ti->init_val = cg()->llvmConstNull(cg()->llvmLibType("hlt.iterator.vector"));
    setResult(ti);
}

void TypeBuilder::visit(type::Integer* i)
{
    TypeInfo* ti = new TypeInfo(i);
    ti->id = HLT_TYPE_INTEGER;
    ti->init_val = cg()->llvmConstInt(0, i->width());
    ti->to_string = "hlt::int_to_string";
    ti->to_int64 = "hlt::int_to_int64";
    // ti->hash = "hlt::default_hash";
    // ti->equal = "hlt::default_equal";

    if ( i->width() <= 8 )
        ti->c_prototype = "int8_t";
    else if ( i->width() <= 16 )
        ti->c_prototype = "int16_t";
    else if ( i->width() <= 32 )
        ti->c_prototype = "int16_t";
    else if ( i->width() <= 64 )
        ti->c_prototype = "int16_t";
    else
        assert(false);

    setResult(ti);
}

llvm::Function* TypeBuilder::_makeTupleDtor(CodeGen* cg, type::Tuple* t)
{
    // Creates the destructor for tuples.
    return _makeTupleFuncHelper(cg, t, true);
}

llvm::Function* TypeBuilder::_makeTupleCctor(CodeGen* cg, type::Tuple* t)
{
    // Creates the cctor for tuples.
    return _makeTupleFuncHelper(cg, t, false);
}

llvm::Function* TypeBuilder::_makeTupleFuncHelper(CodeGen* cg, type::Tuple* t, bool dtor)
{
    auto type = t->sharedPtr<Type>();

    string prefix = dtor ? "dtor_" : "cctor_";
    string name = prefix + type->render();

    llvm::Value* cached = cg->lookupCachedValue(prefix + "-tuple", name);

    if ( cached )
        return llvm::cast<llvm::Function>(cached);

    // Build tuple type. Can't use llvmType() as that would recurse.
    std::vector<llvm::Type*> fields;
    for ( auto t : t->typeList() )
        fields.push_back(cg->llvmType(t));

    auto llvm_type = cg->llvmTypeStruct("", fields);

    CodeGen::llvm_parameter_list params;
    params.push_back(std::make_pair("type", cg->llvmTypePtr(cg->llvmTypeRtti())));
    params.push_back(std::make_pair("tuple", cg->llvmTypePtr(llvm_type)));

    auto func = cg->llvmAddFunction(name, cg->llvmTypeVoid(), params, false);

    cg->pushFunction(func);

    auto a = func->arg_begin();
    ++a;
    auto tval = cg->builder()->CreateLoad(a);

    int idx = 0;

    for ( auto et : t->typeList() ) {
        auto elem = cg->llvmExtractValue(tval, idx++);

        if ( dtor )
            cg->llvmDtor(elem, et, false);
        else
            cg->llvmCctor(elem, et, false);
    }

    cg->popFunction();

    cg->cacheValue(prefix + "-tuple", name, func);

    return func;
}

void TypeBuilder::visit(type::Tuple* t)
{
    // The default value for tuples sets all elements to their default.
    std::vector<llvm::Constant*> elems;

    for ( auto t : t->typeList() )
        elems.push_back(cg()->llvmInitVal(t));

    auto init_val = cg()->llvmConstStruct(elems);

    // Aux information is an array with the fields' offsets. We calculate the
    // offsets via a sizeof-style hack ...
    std::vector<llvm::Constant*> offsets;
    auto null = cg()->llvmConstNull(cg()->llvmTypePtr(init_val->getType()));
    auto zero = cg()->llvmGEPIdx(0);

    for ( int i = 0; i < t->typeList().size(); ++i ) {
        auto offset = cg()->llvmGEP(null, zero, cg()->llvmGEPIdx(i));
        offsets.push_back(llvm::ConstantExpr::getPtrToInt(offset, cg()->llvmTypeInt(16)));
    }

    llvm::Constant* aux;
    if ( offsets.size() ) {
        auto cval = cg()->llvmConstArray(offsets);
        aux = cg()->llvmAddConst("offsets", cval);
    }

    else
        aux = cg()->llvmConstNull();

    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_TUPLE;
    ti->ptr_map = PointerMap(cg(), t).llvmMap();
    ti->c_prototype = "hlt_tuple";
    ti->init_val = init_val;
    ti->pass_type_info = t->wildcard();
    ti->to_string = "hlt::tuple_to_string";
    // ti->hash = "hlt::tuple_hash";
    // ti->equal = "hlt::tuple_equal";
    ti->aux = aux;
    // ti->cctor_func = make_tuple_cctor(cg(), t);

    if ( ! t->wildcard() ) {
        ti->dtor_func = _makeTupleDtor(cg(), t);
        ti->cctor_func = _makeTupleCctor(cg(), t);
    }
    else {
        // Generic versions working with all tuples.
        ti->dtor = "hlt::tuple_dtor";
        ti->cctor = "hlt::tuple_cctor";
    }

    setResult(ti);
}

void TypeBuilder::visit(type::Reference* b)
{
    shared_ptr<hilti::Type> t = nullptr;

    if ( ! b->wildcard() ) {
        auto ti = _typeInfo(b->argType());

        ti->obj_dtor = ti->dtor;
        ti->obj_dtor_func = ti->dtor_func;

        ti->cctor_func = cg()->llvmLibFunction("__hlt_object_ref");
        ti->dtor_func = cg()->llvmLibFunction("__hlt_object_unref");

        setResult(ti);
        return;
    }

    // ref<*>
    TypeInfo* ti = new TypeInfo(b);
    ti->id = HLT_TYPE_ANY;
    ti->init_val = cg()->llvmConstNull();
    ti->c_prototype = "void*";
    setResult(ti);
}

void TypeBuilder::visit(type::Address* t)
{
    CodeGen::constant_list default_;
    default_.push_back(cg()->llvmConstInt(0, 64));
    default_.push_back(cg()->llvmConstInt(0, 64));

    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_ADDR;
    ti->c_prototype = "hlt_addr";
    ti->init_val = cg()->llvmConstStruct(default_);
    ti->to_string = "hlt::addr_to_string";
    // ti->hash = "hlt::default_hash";
    // ti->equal = "hlt::default_equal";
    setResult(ti);
}

void TypeBuilder::visit(type::Network* t)
{
    CodeGen::constant_list default_;
    default_.push_back(cg()->llvmConstInt(0, 64));
    default_.push_back(cg()->llvmConstInt(0, 64));
    default_.push_back(cg()->llvmConstInt(0, 8));

    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_NET;
    ti->c_prototype = "hlt_net";
    ti->init_val = cg()->llvmConstStruct(default_);
    ti->to_string = "hlt::net_to_string";
    // ti->hash = "hlt::default_hash";
    // ti->equal = "hlt::default_equal";
    setResult(ti);
}

void TypeBuilder::visit(type::CAddr* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_CADDR;
    ti->c_prototype = "void *";
    ti->to_string = "hlt::caddr_to_string";
    setResult(ti);
}

void TypeBuilder::visit(type::Double* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_DOUBLE;
    ti->c_prototype = "hlt_double";
    ti->init_val = cg()->llvmConstDouble(0);
    ti->to_string = "hlt::double_to_string";
    ti->to_double = "hlt::double_to_double";
    // ti->hash = "hlt::default_hash";
    // ti->equal = "hlt::default_equal";
    setResult(ti);
}

void TypeBuilder::visit(type::Bitset* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_BITSET;
    ti->c_prototype = "hlt_bitset";
    ti->init_val = cg()->llvmConstInt(0, 64);
    ti->to_string = "hlt::bitset_to_string";
    ti->to_int64 = "hlt::bitset_to_int64";
    // ti->hash = "hlt::default_hash";
    // ti->equal = "hlt::default_equal";

    // Aux information is a list of tuple <uint8_t, const char*>.
    CodeGen::constant_list tuples;

    for ( auto l : t->labels() ) {
        auto label = cg()->llvmConstAsciizPtr(l.first->pathAsString());
        auto bit = cg()->llvmConstInt(l.second, 8);

        CodeGen::constant_list elems;
        elems.push_back(bit);
        elems.push_back(label);
        tuples.push_back(cg()->llvmConstStruct(elems));
    }

    // End marker.
    CodeGen::constant_list elems;
    elems.push_back(cg()->llvmConstInt(0, 8));
    elems.push_back(cg()->llvmConstNull());
    tuples.push_back(cg()->llvmConstStruct(elems));

    auto aux = cg()->llvmConstArray(tuples);
    auto glob = cg()->llvmAddGlobal("bitset_labels", aux->getType(), aux);
    glob->setLinkage(llvm::GlobalValue::LinkOnceAnyLinkage);

    ti->aux = glob;

    setResult(ti);
}

void TypeBuilder::visit(type::Enum* t)
{
    CodeGen::constant_list default_;
    default_.push_back(cg()->llvmConstInt(HLT_ENUM_UNDEF, 8));
    default_.push_back(cg()->llvmConstInt(0, 64));

    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_ENUM;
    ti->c_prototype = "hlt_enum";
    ti->init_val = cg()->llvmConstStruct(default_);
    ti->to_string = "hlt::enum_to_string";
    ti->to_int64 = "hlt::enum_to_int64";
    // ti->hash = "hlt::default_hash";
    // ti->equal = "hlt::default_equal";

    // Aux information is a list of tuple <uint64_t, const char*>.
    CodeGen::constant_list tuples;

    for ( auto l : t->labels() ) {
        auto label = cg()->llvmConstAsciizPtr(l.first->pathAsString());
        auto bit = cg()->llvmConstInt(l.second, 64);

        CodeGen::constant_list elems;
        elems.push_back(bit);
        elems.push_back(label);
        tuples.push_back(cg()->llvmConstStruct(elems));
    }

    // End marker.
    CodeGen::constant_list elems;
    elems.push_back(cg()->llvmConstInt(0, 64));
    elems.push_back(cg()->llvmConstNull());
    tuples.push_back(cg()->llvmConstStruct(elems));

    auto aux = cg()->llvmConstArray(tuples);
    auto glob = cg()->llvmAddGlobal("enum_labels", aux->getType(), aux);
    glob->setLinkage(llvm::GlobalValue::LinkOnceAnyLinkage);

    ti->aux = glob;

    setResult(ti);
}

void TypeBuilder::visit(type::Interval* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_INTERVAL;
    ti->c_prototype = "hlt_interval";
    ti->init_val = cg()->llvmConstInt(0, 64);
    ti->to_string = "hlt::interval_to_string";
    // ti->hash = "hlt::default_hash";
    // ti->equal = "hlt::default_equal";
    setResult(ti);}

void TypeBuilder::visit(type::Time* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_TIME;
    ti->c_prototype = "hlt_time";
    ti->init_val = cg()->llvmConstInt(0, 64);
    ti->to_string = "hlt::time_to_string";
    // ti->hash = "hlt::default_hash";
    // ti->equal = "hlt::default_equal";
    setResult(ti);
}

void TypeBuilder::visit(type::Port* t)
{
    CodeGen::constant_list default_;
    default_.push_back(cg()->llvmConstInt(0, 16));
    default_.push_back(cg()->llvmConstInt(HLT_PORT_TCP, 8));

    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_PORT;
    ti->c_prototype = "hlt_port";
    ti->init_val = cg()->llvmConstStruct(default_, true);
    ti->to_string = "hlt::port_to_string";
    ti->to_int64 = "hlt::port_to_int64";
    // ti->hash = "hlt::default_hash";
    // ti->equal = "hlt::default_equal";

    setResult(ti);
}

void TypeBuilder::visit(type::Bytes* b)
{
    TypeInfo* ti = new TypeInfo(b);
    ti->id = HLT_TYPE_BYTES;
    ti->dtor = "hlt::bytes_dtor";
    ti->c_prototype = "hlt_bytes*";
    ti->lib_type = "hlt.bytes";
    ti->to_string = "hlt::bytes_to_string";
    // ti->hash = "hlt::bool_hash";
    // ti->equal = "hlt::bool_equal";
    setResult(ti);
}

void TypeBuilder::visit(type::Callable* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_CALLABLE;
    ti->dtor = "hlt::callable_dtor";
    ti->c_prototype = "hlt_callable*";
    ti->lib_type = "hlt.callable";
    setResult(ti);
}

void TypeBuilder::visit(type::Channel* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_CHANNEL;
    ti->dtor = "hlt::channel_dtor";
    ti->c_prototype = "hlt_channel*";
    ti->lib_type = "hlt.channel";
    ti->to_string = "hlt::channel_to_string";
    setResult(ti);
}

void TypeBuilder::visit(type::Classifier* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_CLASSIFIER;
    ti->dtor = "hlt::classifier_dtor";
    ti->c_prototype = "hlt_classifier*";
    ti->lib_type = "hlt.classifier";
    setResult(ti);
}

void TypeBuilder::visit(type::File* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_FILE;
    ti->dtor = "hlt::file_dtor";
    ti->c_prototype = "hlt_file*";
    ti->lib_type = "hlt.file";
    ti->to_string = "hlt::file_to_string";
    setResult(ti);
}

void TypeBuilder::visit(type::IOSource* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_IOSOURCE;
    ti->dtor = "hlt::iosrc_dtor";
    ti->c_prototype = "hlt_iosrc*";
    ti->lib_type = "hlt.iosrc";
    setResult(ti);
}

void TypeBuilder::visit(type::List* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_LIST;
    ti->dtor = "hlt::list_dtor";
    ti->c_prototype = "hlt_list*";
    ti->lib_type = "hlt.list";
    ti->to_string = "hlt::list_to_string";
    setResult(ti);
}

void TypeBuilder::visit(type::Map* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_MAP;
    ti->dtor = "hlt::map_dtor";
    ti->c_prototype = "hlt_map*";
    ti->lib_type = "hlt.map";
    ti->to_string = "hlt::map_to_string";
    setResult(ti);
}

void TypeBuilder::visit(type::Vector* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_VECTOR;
    ti->dtor = "hlt::vector_dtor";
    ti->c_prototype = "hlt_vector*";
    ti->lib_type = "hlt.vector";
    ti->to_string = "hlt::vector_to_string";
    setResult(ti);
}

void TypeBuilder::visit(type::Set* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_SET;
    ti->dtor = "hlt::set_dtor";
    ti->c_prototype = "hlt_set*";
    ti->lib_type = "hlt.set";
    ti->to_string = "hlt::set_to_string";
    setResult(ti);
}

void TypeBuilder::visit(type::Overlay* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_OVERLAY;
    ti->dtor = "hlt::overlay_dtor";
    ti->c_prototype = "hlt_overlay";
    ti->lib_type = "hlt.overlay";
    ti->to_string = "hlt::overlay_to_string";
    setResult(ti);
}

void TypeBuilder::visit(type::RegExp* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_REGEXP;
    ti->dtor = "hlt::regexp_dtor";
    ti->c_prototype = "hlt_regexp*";
    ti->lib_type = "hlt.regexp";
    ti->to_string = "hlt::regexp_to_string";
    setResult(ti);
}

void TypeBuilder::visit(type::MatchTokenState* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_MATCH_TOKEN_STATE;
    ti->dtor = "hlt::match_token_state_dtor";
    ti->c_prototype = "hlt_match_token_state*";
    ti->lib_type = "hlt.match_token_state";
    setResult(ti);
}

void TypeBuilder::visit(type::Struct* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_STRUCT;
    ti->dtor = "hlt::struct_dtor";
    ti->c_prototype = "void*";
    ti->to_string = "hlt::struct_to_string";
    // ti->hash = "hlt::struct_hash";
    // ti->equal = "hlt::struct_equal";

    /// Create the struct type.
    string sname = t->id() ? t->id()->pathAsString() : string("struct");
    CodeGen::type_list fields { cg()->llvmLibType("hlt.gchdr"), cg()->llvmTypeInt(32) };

    for ( auto f : t->fields() )
        fields.push_back(cg()->llvmType(f->type()));

    auto stype = cg()->llvmTypeStruct(sname, fields);

    ti->init_val = cg()->llvmConstNull(cg()->llvmTypePtr(stype));

    /// Type information for a ``struct`` includes the fields' offsets in the
    /// ``aux`` entry as a concatenation of pairs (ASCIIZ*, offset), where
    /// ASCIIZ is a field's name, and offset its offset in the value. Aux
    /// information is a list of tuple <const char*, int16_t>.

    auto zero = cg()->llvmGEPIdx(0);
    auto null = cg()->llvmConstNull(cg()->llvmTypePtr());

    CodeGen::constant_list array;

    int i = 0;

    for ( auto f : t->fields() ) {
        auto name = cg()->llvmConstAsciizPtr(f->id()->name());

        // Calculate the offset.
        auto idx = cg()->llvmGEPIdx(i + 2); // skip the gchdr and bitmask.
        auto offset = llvm::ConstantExpr::getGetElementPtr(null, idx);
        offset = llvm::ConstantExpr::getPtrToInt(offset, cg()->llvmTypeInt(16));

        CodeGen::constant_list pair { name, offset };
        array.push_back(cg()->llvmConstStruct(pair));
    }

    if ( array.size() ) {
        auto aval = cg()->llvmConstArray(array);
        auto glob = cg()->llvmAddConst("struct-fields", aval);
        glob->setLinkage(llvm::GlobalValue::LinkOnceAnyLinkage);
        ti->aux = glob;
    }

    setResult(ti);
}

void TypeBuilder::visit(type::Timer* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_TIMER;
    ti->dtor = "hlt::timer_dtor";
    ti->c_prototype = "hlt_timer*";
    ti->lib_type = "hlt.timer";
    ti->to_string = "hlt::timer_to_string";
    ti->to_int64 = "hlt::timer_to_int64";
    ti->to_double = "hlt::timer_to_double";
    setResult(ti);
}

void TypeBuilder::visit(type::TimerMgr* t)
{
    TypeInfo* ti = new TypeInfo(t);
    ti->id = HLT_TYPE_TIMER_MGR;
    ti->dtor = "hlt::timer_mgr_dtor";
    ti->c_prototype = "hlt_timer_mgr*";
    ti->lib_type = "hlt.timer_mgr";
    ti->to_string = "hlt::timer_mgr_to_string";
    setResult(ti);
}
