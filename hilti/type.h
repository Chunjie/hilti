
#ifndef HILTI_TYPE_H
#define HILTI_TYPE_H

// TODO: Much more of th should move into ast/type.h

#include <vector>

#include <ast/type.h>

#include "common.h"
#include "scope.h"
#include "attribute.h"
#include "visitor-interface.h"

using namespace hilti;

namespace hilti {

/// Base class for all AST nodes representing a type.
class Type : public ast::Type<AstInfo>, public NodeWithAttributes
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Type(const Location& l=Location::None);

   /// Returns a readable one-line representation of the type.
   string render() override;

   /// Retturns a readable represention meant to be used in the automatically
   /// generated reference documentation. By default, this return the same as
   /// render() but can be overridden by derived classed.
   virtual string docRender() { return render(); }

   /// Returns a scope of sub-identifiers that a relative to the type. If a
   /// type defines IDs in this scope and a global type declaration is added
   /// to a module, these IDs will be accessible via scoping relative to the
   /// declaration. The returned scope should not have its parent set.
   virtual shared_ptr<hilti::Scope> typeScope() { return nullptr; }

   ACCEPT_VISITOR_ROOT();
};

namespace type {

namespace trait {

// Base class for type trait. We use trait to mark types that have certain properties.
//
// Note that Traits aren't (and can't) be derived from Node and thus aren't AST nodes.
class Trait
{
public:
   Trait() {}
   virtual ~Trait();
};

namespace parameter {

/// Base class for a type parameter. This is used with trait::Parameterized.
class Base {
public:
   virtual ~Base() {}

   /// Will be called to compare this node against another of the *same*
   /// type.
   virtual bool _equal(shared_ptr<Base> other) const = 0;
};

/// A type parameter representing a Type.
class Type : public Base {
public:
   /// Constructor.
   ///
   /// type: The type the parameter specifies.
   Type(shared_ptr<hilti::Type> type) { _type = type; }

   /// Returns the type the parameter specifies.
   shared_ptr<hilti::Type> type() const { return _type; }

   bool _equal(shared_ptr<Base> other) const override {
       return _type->equal(std::dynamic_pointer_cast<type::trait::parameter::Type>(other)->_type);
   }

private:
   node_ptr<hilti::Type> _type;
};

/// A type parameter representing an integer value.
class Integer : public Base {
public:
   /// Constructor.
   ///
   /// value: The parameter's integer value.
   Integer(int64_t value) { _value = value; }

   /// Returns the parameters integer value.
   int64_t value() const { return _value; }

   bool _equal(shared_ptr<Base> other) const override {
       return _value == std::dynamic_pointer_cast<Integer>(other)->_value;
   }

private:
   int64_t _value;
};

/// A type parameter representing an enum value. This must include the scope
/// for a module-level lookup.
class Enum : public Base {
public:
   /// Constructor.
   ///
   /// label: The parameter's enum identifier.
   Enum(shared_ptr<ID> label) { _label = label; }

   /// Returns the parameters enum identifier.
   shared_ptr<ID> label() const { return _label; }

   bool _equal(shared_ptr<Base> other) const override {
       return _label->pathAsString() == std::dynamic_pointer_cast<Enum>(other)->_label->pathAsString();
   }

private:
   shared_ptr<ID> _label;
};

class Attribute : public Base {
public:
   /// Constructor.
   ///
   /// value: The parameter's attribute, including the ampersand.
   Attribute(const string& attr) { _attr = attr; }

   /// Returns the attribute.
   const string& value() const { return _attr; }

   bool _equal(shared_ptr<Base> other) const override {
       return _attr == std::dynamic_pointer_cast<Attribute>(other)->_attr;
   }

private:
   string _attr;
};

}

/// Trait class marking a type with parameters.
class Parameterized : public virtual Trait
{
public:
   typedef std::list<shared_ptr<parameter::Base>> parameter_list;

   /// Compares the type with another for equivalence, which must be an
   /// instance of the same type class.
   bool equal(shared_ptr<hilti::Type> other) const;

   /// Returns the type's parameters. Note we return this by value as the
   /// derived classes need to build the list on the fly each time so that
   /// potential updates to the members (in particular type resolving) get
   /// reflected.
   virtual parameter_list parameters() const = 0;
};

/// Trait class marking a composite type that has a series of subtypes.
class TypeList : public virtual Trait
{
public:
   typedef std::list<shared_ptr<hilti::Type>> type_list;

   /// Returns the ordered list of subtypes.
   virtual const type_list typeList() const = 0;
};

/// Trait class marking types which's instances are garbage collected by the
/// HILTI run-time.
class GarbageCollected : public virtual Trait
{
};

/// Trait class marking types which's instances can be copied bit-wise
/// without any need for further cctors/dtors.
class Atomic : public virtual Trait
{
};

/// Trait class marking a type that provides iterators.
class Iterable : public virtual Trait
{
public:
   /// Returns the type for an iterator over this type.
   virtual shared_ptr<hilti::Type> iterType() = 0;

   /// Returns the type of elements when iterating over this type.
   virtual shared_ptr<hilti::Type> elementType() = 0;
};

/// Trait class marking a type that can be hashed for storing in containers.
class Hashable : public virtual Trait
{
public:
};

/// Trait class marking a container type.
class Container : public virtual Trait, public Iterable
{
};

/// Trait class marking a type that provides yielding until a resource
/// becomes available (i.e., it can acts as argument in a \c yield instruction).
/// Theses types must provide a \a blockable function in their type information.
class Blockable : public virtual Trait
{
};

/// Trait class marking types which's instances can be unpacked from binary
/// data via the Unpacker.
class Unpackable : public virtual Trait
{
public:
   /// Description of an unpack format supported by the type.
   struct Format {
       /// The fully-qualified name of a ``Hilti::Packed`` constant (i.e.,
       /// ``Hilti::Packed::Bool``.
       string enumName;

       /// The type of the unpacked element.
       shared_ptr<hilti::Type> result_type;

       /// The type for the 2nd unpack argument, or null if not used.
       shared_ptr<hilti::Type> arg_type;

       /// True if the 2nd argument is optional and may be skipped.
       bool arg_optional;

       /// Description of the format suitable for inclusion in documentation.
       string doc;
   };

   /// Returns the suportted unpack formats.
   virtual const std::vector<Format>& unpackFormats() const = 0;
};

/// Trait class marking types which can be used within a classifier rule.
/// HILTI run-time.
class Classifiable : public virtual Trait
{
public:
   typedef std::list<shared_ptr<Type>> type_list;

   /// Returns a list of types that this one can be matched against in a
   /// classifier rule \a in \a addition to the class itself. Can be
   /// overidden by derived classes, the default implementatin returns just
   /// an empty list.
   virtual type_list alsoMatchableTo() const { return type_list(); }
};

/// Trait class marking a type that is passed by reference.
class HeapType : public virtual Trait
{
};

/// Trait class marking a type that is passed by value.
class ValueType : public virtual Trait
{
};

}

/// Returns true if type \a t has trait \a T.
template<typename T>
inline bool hasTrait(const hilti::Type* t) { return dynamic_cast<const T*>(t) != 0; }

/// Returns true if type \a t has trait \a T.
template<typename T>
inline bool hasTrait(shared_ptr<hilti::Type> t) { return std::dynamic_pointer_cast<T>(t) != 0; }

/// Dynamic-casts type \a t into trait class \a T.
template<typename T>
inline T* asTrait(hilti::Type* t) { return dynamic_cast<T*>(t); }

/// Dynamic-casts type \a t into trait class \a T.
template<typename T>
inline const T* asTrait(const hilti::Type* t) { return dynamic_cast<const T*>(t); }

/// Dynamic-casts type \a t into trait class \a T.
template<typename T>
inline T* asTrait(shared_ptr<hilti::Type> t) { return std::dynamic_pointer_cast<T>(t); }

/// An interal Type-derived class that allows us to specify an optional
/// parameters in an instruction's signature.
class OptionalArgument : public hilti::Type
{
public:
   OptionalArgument(shared_ptr<Type> arg) { _arg = arg; }

   string render() override {
       return string("[ " + _arg->render() + " ]");
   }

   bool equal(shared_ptr<hilti::Type> other) const override {
       return other ? _arg->equal(other) : true;
   }

   shared_ptr<hilti::Type> argType() const { return _arg; }

private:
   shared_ptr<Type> _arg;
};

/// Base class for types that a user can instantiate.
class HiltiType : public hilti::Type
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   HiltiType(const Location& l=Location::None);
};

/// Base class for types that are stored on the stack and value-copied.
class ValueType : public HiltiType, public trait::Hashable, public trait::ValueType
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   ValueType(const Location& l=Location::None) : HiltiType(l) {}

};

/// Base class for types that stored on the heap and manipulated only by
/// reference.
class HeapType : public HiltiType, public trait::GarbageCollected, public trait::HeapType
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   HeapType(const Location& l=Location::None) : HiltiType(l) {}
};

/// Base class for a heap type parameterized with a single type parameter.
class TypedHeapType : public HeapType, public trait::Parameterized {
public:
   /// Constructor.
   ///
   /// argtype: The type parameter.
   ///
   /// l: Associated location.
   TypedHeapType(shared_ptr<Type> argtype, const Location& l=Location::None);

   /// Constructor for a wildcard type.
   ///
   /// l: Associated location.
   TypedHeapType(const Location& l=Location::None);

   shared_ptr<Type> argType() const { return _argtype; }

   parameter_list parameters() const override;

   bool _equal(shared_ptr<hilti::Type> other) const override {
       return trait::Parameterized::equal(other);
   }

private:
   node_ptr<Type> _argtype;
};

/// Base class for a value type parameterized with a single type parameter.
class TypedValueType : public ValueType, public trait::Parameterized {
public:
   /// Constructor.
   ///
   /// argtype: The type parameter.
   ///
   /// l: Associated location.
   TypedValueType(shared_ptr<Type> argtype, const Location& l=Location::None);

   /// Constructor for a wildcard type.
   ///
   /// l: Associated location.
   TypedValueType(const Location& l=Location::None);

   shared_ptr<Type> argType() const { return _argtype; }

   parameter_list parameters() const override;

   bool _equal(shared_ptr<hilti::Type> other) const override {
       return trait::Parameterized::equal(other);
   }

private:
   node_ptr<Type> _argtype;
};

/// Base type for iterators.
class Iterator : public TypedValueType
{
public:
   /// Constructor for a wildcard iterator type.
   ///
   /// l: Associated location.
   Iterator(const Location& l=Location::None) : TypedValueType(l) {}

   ACCEPT_VISITOR(Type);

protected:
   /// Constructor for an iterator over a given target tyoe.
   ///
   /// ttype: The target type. This must be a trait::Iterable.
   ///
   /// l: Associated location.
   ///
   Iterator(shared_ptr<Type> ttype, const Location& l=Location::None) : TypedValueType(ttype, l) {}

private:
   node_ptr<hilti::Type> _ttype;
};

/// A tupe matching any other type.
class Any : public hilti::Type
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Any(const Location& l=Location::None) : hilti::Type(l) { setMatchesAny(true); }
   virtual ~Any();
   ACCEPT_VISITOR(Type);
};

/// A place holder type representing a type that has not been resolved yet.
class Unknown : public hilti::Type
{
public:
   /// Constructor for a type not further specified. This must be resolved by
   /// some external means.
   ///
   /// l: Associated location.
   Unknown(const Location& l=Location::None) : hilti::Type(l) {}

   /// Constructor referencing a type by its name. This will be resolved
   /// automatically by resolver.
   Unknown(shared_ptr<ID> id, const Location& l=Location::None) : hilti::Type(l) { _id = id; addChild(_id); }

   /// If an ID is associated with the type, returns it; null otherwise.
   shared_ptr<ID> id() const { return _id; }

   virtual ~Unknown();

   ACCEPT_VISITOR(Type);

private:
   node_ptr<ID> _id = nullptr;
};

/// An internal place holder type representing a type by its name, to be
/// resolved later. This should be used only in situations where's its
/// guaranteed that the resolving will be done manually, there's no automatic
/// handling.
class TypeByName : public hilti::Type
{
public:
   /// Constructor.
   ///
   /// name: The type name.
   ///
   /// l: Associated location.
   TypeByName(const string& name, const Location& l=Location::None) : hilti::Type(l) {
       _name = name;
   }
   virtual ~TypeByName();

   const string& name() const { return _name; }

   bool _equal(shared_ptr<hilti::Type> other) const override {
       return std::dynamic_pointer_cast<TypeByName>(other)->name() == name();
   }

   ACCEPT_VISITOR(Type);

private:
   string _name;
};

/// A type representing an unset value.
class Unset : public ValueType
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Unset(const Location& l=Location::None) : ValueType(l) {}
   virtual ~Unset();

   ACCEPT_VISITOR(Type);
};

/// A type representing a statement::Block.
class Block : public hilti::Type
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Block(const Location& l=Location::None) : hilti::Type(l)  {}
   virtual ~Block();
   ACCEPT_VISITOR(hilti::Type);
};

/// A type representing a Module.
class Module : public hilti::Type
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Module(const Location& l=Location::None) : hilti::Type(l) {}
   virtual ~Module();
   ACCEPT_VISITOR(Module);
};

/// A type representing a non-existing return value.
class Void : public hilti::Type
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Void(const Location& l=Location::None) : hilti::Type(l) {}
   virtual ~Void();
   ACCEPT_VISITOR(Type);
};

/// A type representing the name of an instruction block.
class Label : public hilti::Type
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Label(const Location& l=Location::None) : hilti::Type(l) {}
   virtual ~Label();
   ACCEPT_VISITOR(Type);
};

/// Type for strings.
class String : public ValueType, public trait::GarbageCollected {
public:
   /// Constructor.
   ///
   /// l: Associated location.
   String(const Location& l=Location::None) : ValueType(l) {}
   virtual ~String();

   ACCEPT_VISITOR(Type);
};

/// Type for IP addresses.
class Address : public ValueType, public trait::Atomic, public trait::Unpackable, public trait::Classifiable {
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Address(const Location& l=Location::None) : ValueType(l) {}
   virtual ~Address();

   const std::vector<Format>& unpackFormats() const override;

   ACCEPT_VISITOR(Type);
};

/// Type for IP subnets.
class Network : public ValueType, public trait::Atomic, public trait::Classifiable {
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Network(const Location& l=Location::None) : ValueType(l) {}
   virtual ~Network();

   ACCEPT_VISITOR(Type);
};

/// Type for ports.
class Port : public ValueType, public trait::Atomic, public trait::Unpackable, public trait::Classifiable {
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Port(const Location& l=Location::None) : ValueType(l) {}
   virtual ~Port();

   const std::vector<Format>& unpackFormats() const override;

   ACCEPT_VISITOR(Type);
};

/// Type for bitsets.
class Bitset : public ValueType, public trait::Atomic {
public:
   typedef std::pair<shared_ptr<ID>, int> Label;
   typedef std::list<Label> label_list;

   /// Constructor.
   ///
   /// labels: List of pairs of label name and bit number. If a bit number is
   /// -1, it will be chosen automaticallty.
   ///
   /// l: Associated location.
   Bitset(const label_list& labels, const Location& l=Location::None);
   virtual ~Bitset();

   /// Constructor creating a bitset type matching any other bitset type.
   Bitset(const Location& l=Location::None) : ValueType(l) {
       setWildcard(true);
   }

   // Returns the labels with their bit numbers.
   const label_list& labels() const { return _labels; }

   // Returns the bit number for a label, which must be known.
   int labelBit(shared_ptr<ID> label) const;

   shared_ptr<hilti::Scope> typeScope() override;

   bool _equal(shared_ptr<hilti::Type> other) const override;

   ACCEPT_VISITOR(Type);

private:
   label_list _labels;
   shared_ptr<hilti::Scope> _scope = nullptr;
};

/// Type for enums.
class Enum : public ValueType, public trait::Atomic {
public:
   typedef std::pair<shared_ptr<ID>, int> Label;
   typedef std::list<Label> label_list;

   /// Constructor.
   ///
   /// labels: List of pairs of label name and value. If a bit number is -1,
   /// it will be chosen automaticallty.
   ///
   /// l: Associated location.
   Enum(const label_list& labels, const Location& l=Location::None);
   virtual ~Enum();

   /// Constructor creating an enum type matching any other enum type.
   Enum(const Location& l=Location::None) : ValueType(l) {
       setWildcard(true);
   }

   // Returns the labels with their bit numbers.
   const label_list& labels() const { return _labels; }

   // Returns the value for a label, which must be known. Returns -1 for \c Undef.
   int labelValue(shared_ptr<ID> label) const;

   shared_ptr<hilti::Scope> typeScope() override;

   bool _equal(shared_ptr<hilti::Type> other) const override;

   ACCEPT_VISITOR(Type);

private:
   label_list _labels;
   shared_ptr<hilti::Scope> _scope = nullptr;
};

/// Type for caddr values.
class CAddr : public ValueType, public trait::Atomic {
public:
   /// Constructor.
   ///
   /// l: Associated location.
   CAddr(const Location& l=Location::None) : ValueType(l) {}
   virtual ~CAddr();

   ACCEPT_VISITOR(Type);
};

/// Type for doubles.
class Double : public ValueType, public trait::Atomic {
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Double(const Location& l=Location::None) : ValueType(l) {}
   virtual ~Double();

   ACCEPT_VISITOR(Type);
};

/// Type for booleans.
class Bool : public ValueType, public trait::Atomic, public trait::Unpackable, public trait::Classifiable {
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Bool(const Location& l=Location::None) : ValueType(l) {}
   virtual ~Bool();

   const std::vector<Format>& unpackFormats() const override;

   ACCEPT_VISITOR(Type);
};

/// Type for interval values.
class Interval : public ValueType, public trait::Atomic {
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Interval(const Location& l=Location::None) : ValueType(l) {}
   virtual ~Interval();

   ACCEPT_VISITOR(Type);
};

/// Type for time values.
class Time : public ValueType, public trait::Atomic {
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Time(const Location& l=Location::None) : ValueType(l) {}
   virtual ~Time();

   ACCEPT_VISITOR(Type);
};


/// Type for integer values.
class Integer : public ValueType, public trait::Atomic, public trait::Parameterized, public trait::Unpackable, public trait::Classifiable
{
public:
   /// Constructor.
   ///
   /// width: The bit width for the type.
   ///
   /// l: Associated location.
   Integer(int width, const Location& l=Location::None) : ValueType(l) {
       _width = width;
       }

   /// Constructore creating an integer type matching any other integer type (i.e., \c int<*>).
   Integer(const Location& l=Location::None) : ValueType(l) {
       _width = 0;
       setWildcard(true);
   }

   virtual ~Integer();

   /// Returns the types bit width.
   int width() const { return _width; }

   bool _equal(shared_ptr<hilti::Type> other) const override {
       return trait::Parameterized::equal(other);
   }

   parameter_list parameters() const override;

   const std::vector<Format>& unpackFormats() const override;

   ACCEPT_VISITOR(Type);

private:
   int _width;
};

/// Type for tuples.
class Tuple : public ValueType, public trait::Parameterized, public trait::TypeList {
public:
   typedef std::list<shared_ptr<hilti::Type>> type_list;
   typedef std::list<shared_ptr<hilti::ID>> id_list;

   typedef std::pair<shared_ptr<hilti::ID>, shared_ptr<hilti::Type>> element;
   typedef std::list<element> element_list;

   /// Constructor for a tuple type with anonymous elements
   ///
   /// types: The types of the tuple's elements.
   ///
   /// l: Associated location.
   Tuple(const type_list& types, const Location& l=Location::None);

   /// Constructor for a tuple type with named elements
   ///
   /// elems: The elements of the tuple with name and type.
   ///
   /// l: Associated location.
   Tuple(const element_list& elems, const Location& l=Location::None);

   /// Constructor for a wildcard tuple type matching any other.
   ///
   /// l: Associated location.
   Tuple(const Location& l=Location::None);

   /// Returns a list of names associated with the tuple elements, which will
   /// have exactly as many entries as the tuple has elements. If no names
   /// have been associated with the type, the list will contains nullptrs
   /// only.
   id_list names() const;

   /// Associated a list of names with the tuple elements.
   ///
   /// names: The names. There must be exactly one list element per type.
   void setNames(const id_list& names);

   const trait::TypeList::type_list typeList() const override;
   parameter_list parameters() const override;

   bool _equal(shared_ptr<hilti::Type> other) const override {
       return trait::Parameterized::equal(other);
   }

   ACCEPT_VISITOR(Type);

private:
   std::list<node_ptr<hilti::Type>> _types;
   std::list<node_ptr<hilti::ID>> _names;
};

/// Type for types.
class TypeType : public hilti::Type
{
public:
   /// Constructor.
   ///
   /// type; The represented type.
   ///
   /// l: Associated location.
   TypeType(shared_ptr<hilti::Type> type, const Location& l=Location::None) : hilti::Type(l) {
       _rtype = type; addChild(_rtype);
   }

   /// Constructor for wildcard type.
   ///
   /// l: Associated location.
   TypeType(const Location& l=Location::None) : hilti::Type() {
       setWildcard(true);
   }

   ~TypeType();

   shared_ptr<hilti::Type> typeType() const { return _rtype; }

   bool _equal(shared_ptr<hilti::Type> other) const override {
       return _rtype->equal(std::dynamic_pointer_cast<TypeType>(other)->_rtype);
   }

   ACCEPT_VISITOR(hilti::Type);

private:
   node_ptr<hilti::Type> _rtype;
};

/// Type for exceptions.
class Exception : public TypedHeapType
{
public:
   /// Constructor.
   ///
   /// base: Another exception type this one derived from. If null, the
   /// top-level base exception is used.
   ///
   /// arg: The type of the exceptions argument, or null if none.
   ///
   /// l: Location associated with the type.
   Exception(shared_ptr<Type> base, shared_ptr<Type> arg, const Location& l=Location::None);
   ~Exception();

   /// Constructor. This creates a wildcard type matching any other exception type.
   Exception(const Location& l=Location::None) : TypedHeapType(l) {}

   /// Returns the base exception type this one is derived from. Returns null
   /// for the top-level base exception.
   shared_ptr<Type> baseType() const { return _base; }

   /// Sets the base exception type this one is derived from.
   void setBaseType(shared_ptr<Type> base) { _base = base; }

   /// Returns true if this is the top-level root type.
   bool isRootType() const { return _libtype == "hlt_exception_unspecified"; }

   /// Returns the level of this type inside the inheritance tree from the
   /// root. Root is 0.
   int level() const;

   ACCEPT_VISITOR(Type);

private:
   node_ptr<Type> _base = nullptr;
   string _libtype;
};

/// Type for references.
class Reference : public TypedValueType
{
public:
   /// Constructor.
   ///
   /// rtype: The referenced type. This must be a HeapType.
   ///
   /// l: Associated location.
   Reference(shared_ptr<Type> rtype, const Location& l=Location::None) : TypedValueType(rtype, l) {}

   /// Constructor for wildcard reference type..
   ///
   /// l: Associated location.
   Reference(const Location& l=Location::None) : TypedValueType(l) {}

   ACCEPT_VISITOR(Type);
};

namespace function {

/// Helper type to define a function parameter or return value.
class Parameter : public ast::type::mixin::function::Parameter<AstInfo>
{
public:
   /// Constructor for function parameters.
   ///
   /// id: The name of the parameter.
   ///
   /// type: The type of the parameter.
   ///
   /// constant: A flag indicating whether the parameter is constant (i.e., a
   /// function invocation won't change its value.)
   ///
   /// default_value: An optional default value for the parameters, or null if none.
   ///
   /// l: A location associated with the expression.
   Parameter(shared_ptr<hilti::ID> id, shared_ptr<Type> type, bool constant, shared_ptr<Expression> default_value, Location l=Location::None);

   /// Constructor for a return value.
   ///
   /// type: The type of the return value.
   ///
   /// constant: A flag indicating whether the return value is constant
   /// (i.e., a caller may not change its value.)
   ///
   /// l: A location associated with the expression.
   Parameter(shared_ptr<Type> type, bool constant, Location l=Location::None);

   ACCEPT_VISITOR_ROOT();
};

/// Helper type to define a function parameter or return value.
class Result : public ast::type::mixin::function::Result<AstInfo>
{
public:
    /// Constructor for a return value.
    ///
    /// type: The type of the return value.
    ///
    /// constant: A flag indicating whether the return value is constant
    /// (i.e., a caller may not change its value.)
    ///
    /// l: A location associated with the expression.
    Result(shared_ptr<Type> type, bool constant, Location l=Location::None);

    ACCEPT_VISITOR_ROOT();
};

typedef ast::type::mixin::Function<AstInfo>::parameter_list parameter_list;

/// HILTI's supported calling conventions.
enum CallingConvention {
    DEFAULT,    /// Place-holder for chosing a default automatically.
    HILTI,      /// Internal convention for calls between HILTI functions.
    HOOK,       /// Internal convention for hooks.
    HILTI_C,    /// C-compatible calling convention, but with extra HILTI run-time parameters.
    C,          /// C-compatable calling convention, with no messing around with parameters.
    CALLABLE    /// Internal convention for call to callable instances.
};

}

/// Base class for all function types.
class Function : public HiltiType, public ast::type::mixin::Function<AstInfo>
{
public:
   /// Returns the function's calling convention.
   hilti::type::function::CallingConvention callingConvention() const { return _cc; }

   /// Sets the function's calling convention.
   void setCallingConvention(hilti::type::function::CallingConvention cc) { _cc = cc; }

   /// Returns true if this function uses the adapted HILTI/HOOK calling
   /// convention that passes parameters at +1.
   bool ccPlusOne() const { return _plusone; }

   /// Sets the function's use of the adapted HILTI/HOOK calling convention
   /// that passes parameters as plus one.
   void setCcPlusOne(bool plusone) { _plusone = plusone; }

   /// Returns true if calling the function may trigger a safepoint.
   bool mayTriggerSafepoint() const;

   /// Returns true if calling the function may trigger a yield.
   bool mayYield() const;

   bool _equal(shared_ptr<hilti::Type> other) const override;

   ACCEPT_VISITOR(HiltiType);

protected:
   /// Constructor.
   ///
   /// result: The function return type.
   ///
   /// params: The function's parameters.
   ///
   /// cc: The function's calling convention.
   ///
   /// l: Associated location.
   Function(shared_ptr<hilti::type::function::Result> result, const function::parameter_list& args,
            hilti::type::function::CallingConvention cc, const Location& l=Location::None);

   /// Constructor for a function type that matches any other function type (i.e., a wildcard type).
   Function(const Location& l=Location::None);

private:
   hilti::type::function::CallingConvention _cc;
   bool _plusone;
};

/// Type for HILTI-level functions.
class HiltiFunction : public Function, public trait::ValueType
{
public:
   /// Constructor.
   ///
   /// result: The hook's return type.
   ///
   /// params: The hooks's parameters.
   ///
   /// l: Associated location.
   HiltiFunction(shared_ptr<hilti::type::function::Result> result, const function::parameter_list& args,
                 hilti::type::function::CallingConvention cc, const Location& l=Location::None)
       : Function(result, args, cc, l) { }

   /// Constructor for a hook type that matches any other hook type (i.e., a
   /// wildcard type).
   HiltiFunction(const Location& l=Location::None) : Function(l) {}

   virtual ~HiltiFunction();

   ACCEPT_VISITOR(hilti::type::Function);
};

/// Type for hooks.
class Hook : public Function, public trait::ValueType
{
public:
   /// Constructor.
   ///
   /// result: The hook's return type.
   ///
   /// params: The hooks's parameters.
   ///
   /// l: Associated location.
   Hook(shared_ptr<hilti::type::function::Result> result, const function::parameter_list& args,
        const Location& l=Location::None);

   /// Constructor for a hook type that matches any other hook type (i.e., a
   /// wildcard type).
   Hook(const Location& l=Location::None) : Function(l) {}

   virtual ~Hook();

   ACCEPT_VISITOR(hilti::type::Function);
};

/// Type for callable instances.
///
class Callable : public Function, public trait::HeapType, public trait::GarbageCollected, public trait::Parameterized
{
public:
   /// Constructor.
   ///
   /// result: The callables's return type.
   ///
   /// params: The callables's parameters.
   ///
   /// l: Associated location.
   Callable(shared_ptr<hilti::type::function::Result> result, const function::parameter_list& args,
            const Location& l=Location::None)
       : Function(result, args, function::CALLABLE, l) {}

   /// Constructor for wildcard callable type..
   ///
   /// l: Associated location.
   Callable(const Location& l=Location::None) : Function(l) {}

   /// Destructor.
   ~Callable();

   trait::Parameterized::parameter_list parameters() const override;

   ACCEPT_VISITOR(Function);
};

/// Type for bytes instances.
///
class Bytes : public HeapType, public trait::Iterable, public trait::Unpackable, public trait::Hashable, public trait::Classifiable, public trait::Blockable
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Bytes(const Location& l=Location::None) : HeapType(l) {}
   virtual ~Bytes();

   shared_ptr<hilti::Type> iterType() override;
   shared_ptr<hilti::Type> elementType() override;

   const std::vector<Format>& unpackFormats() const override;

   ACCEPT_VISITOR(Type);
};

namespace iterator {

class Bytes : public Iterator
{
public:
   /// Constructor for an iterator over a bytes object.
   ///
   /// l: Associated location.
   ///
   Bytes(const Location& l=Location::None) : Iterator(shared_ptr<Type>(new type::Bytes(l))) {}

   ACCEPT_VISITOR(Iterator);
};

}

/// Type for classifier instances.
///
class Classifier : public HeapType, public trait::Parameterized
{
public:
   /// Constructor.
   ///
   /// rtype: The type for the classifier rules. This must be a reference to
   /// heap type that's of trait trait::TypeList.
   ///
   /// vtype: The type for values associated with rules.
   ///
   /// l: Associated location.
   Classifier(shared_ptr<Type> rtype, shared_ptr<Type> vtype, const Location& l=Location::None);
   virtual ~Classifier();

   /// Constructor for wildcard classifier type.
   ///
   /// l: Associated location.
   Classifier(const Location& l=Location::None);

   /// Returns the type for rules.
   shared_ptr<Type> ruleType() const { return _rtype; }

   /// Returns the type for value.
   shared_ptr<Type> valueType() const { return _vtype; }

   parameter_list parameters() const override;

   bool _equal(shared_ptr<hilti::Type> other) const override;

   ACCEPT_VISITOR(Type);

private:
   node_ptr<Type> _rtype;
   node_ptr<Type> _vtype;
};

class Overlay;

namespace overlay {

/// Definition of one overlay field.
class Field : public Node
{
public:
   typedef std::list<node_ptr<overlay::Field>> field_list;

   /// Constructor.
   ///
   /// name: The name of the field.
   ///
   /// start: Offset in bytes form the start of the overlay where the field's
   /// data starts.
   ///
   /// type: The type of the field.
   ///
   /// fmt: An expression refering to a ``Hilti::Packed`` label defining the
   /// format used for unpacking the field.
   ///
   /// arg: An argument expression that might be need for the given \a fmt
   /// unpack format.
   ///
   /// l: Associated location.
   Field(shared_ptr<ID> name, shared_ptr<Type> type, int start, shared_ptr<Expression> fmt, shared_ptr<Expression> arg = nullptr, const Location& l=Location::None);

   /// Constructor.
   ///
   /// name: The name of the field.
   ///
   /// type: The type of the field.
   ///
   /// start: Name of an another field which this one follows immediately in
   /// the overlay layout.
   ///
   /// fmt: An expression refering to a ``Hilti::Packed`` label defining the
   /// format used for unpacking the field.
   ///
   /// arg: An argument expression that might be need for the given \a fmt
   /// unpack format.
   ///
   /// l: Associated location.
   Field(shared_ptr<ID> name, shared_ptr<Type> type, shared_ptr<ID> start, shared_ptr<Expression> fmt, shared_ptr<Expression> arg = nullptr, const Location& l=Location::None);

   /// Destructor.
   ~Field();

   /// Returns the field's name.
   shared_ptr<ID> name() const { return _name; }

   /// Returns the field's type.
   shared_ptr<Type> type() const { return _type; }

   /// Returns the field's start offset, if set via the ctor; -1 if not.
   int startOffset() const { return _start_offset; }

   /// Returns the field's predecessor field, if set via the ctor; null if not.
   shared_ptr<ID> startField() const { return _start_field; }

   /// Returns the field's unpack format.
   shared_ptr<Expression> format() const;

   /// Returns the field's optional unpack argument, or null if not set.
   shared_ptr<Expression> formatArg() const;

   /// Returns a list of all other fields that this one depends on for
   /// determining its start offset in the overlay. The list is sorted in the
   /// order the fields are defined in the overlay. If the named field starts
   /// at a constant offset and does not depend on any other fields, an empty
   /// list is returned.
   ///
   /// Valid only after the field has been added to an Overlay.
   const field_list& dependents() const { return _deps; }

   /// Returns an index unique across all fields that are directly
   /// determining the starting position of another field with a non-constant
   /// offset. All of these fields are sequentially numbered, starting with
   /// one. Returns -1 if the does not determine another's offset directly.
   ///
   /// Valid only after the field has been added to an Overlay.
   int depIndex() const { return _idx; }

   bool equal(shared_ptr<Field> other) const;

   ACCEPT_VISITOR_ROOT();

private:
   friend class hilti::type::Overlay;

   shared_ptr<ID> _name;
   shared_ptr<Type> _type;
   int _start_offset;
   shared_ptr<ID> _start_field;
   node_ptr<Expression> _fmt;
   node_ptr<Expression> _fmt_arg;
   field_list _deps;
   int _idx;
};

}

/// Type for overlays.
class  Overlay : public ValueType {
public:
   typedef overlay::Field::field_list field_list;

   /// Constructor.
   ///
   Overlay(const field_list& fields, const Location& l=Location::None);

   /// Constructor for wildcard overlay type.
   ///
   /// l: Associated location.
   Overlay(const Location& l=Location::None);

   virtual ~Overlay();

   /// Returns the overlay's fields.
   const field_list& fields() const { return _fields; }

   /// Returns a field of a given name, or null if not known.
   shared_ptr<overlay::Field> field(const string& name) const;

   /// Returns a field of a given name, or null if not known.
   shared_ptr<overlay::Field> field(shared_ptr<ID> name) const;

   /// Returns the number of fields that other fields directly depend on for
   /// their starting position. This number is guaranteed to be not higher
   /// than the largest overlay::Field::depIndex() in this overlay.
   int numDependencies() const { return _idxcnt; }

   bool _equal(shared_ptr<hilti::Type> other) const override;

   ACCEPT_VISITOR(Type);

private:
   void Init();

   field_list _fields;
   int _idxcnt = 0;
};

/// Type for file instances.
///
class File : public HeapType
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   File(const Location& l=Location::None) : HeapType(l) {}
   virtual ~File();

   ACCEPT_VISITOR(Type);
};

/// Type for channels.
class Channel : public TypedHeapType
{
public:
   /// Constructor.
   ///
   /// type: The channel's data type. This must be a ValueType.
   ///
   /// l: Associated location.
   Channel(shared_ptr<Type> type, const Location& l=Location::None) : TypedHeapType(type, l) {}

   /// Constructor for wildcard reference type..
   ///
   /// l: Associated location.
   Channel(const Location& l=Location::None) : TypedHeapType(l) {}

   virtual ~Channel();

   ACCEPT_VISITOR(Type);
};

/// Type for IOSource instances.
class IOSource : public HeapType, public trait::Parameterized, public trait::Iterable
{
public:
   /// Constructor.
   ///
   /// kind: An enum identifier referencing the type of IOSource (``Hilti::IOSrc::*``).
   ///
   /// l: Associated location.
   IOSource(shared_ptr<ID> kind, const Location& l=Location::None) : HeapType(l) {
       _kind = kind;
       addChild(_kind);
   }

   /// Constructore creating an IOSrc type matching any other (i.e., \c iosrc<*>).
   IOSource(const Location& l=Location::None) : HeapType(l) {
       setWildcard(true);
   }

   virtual ~IOSource();

   /// Returns the enum label for the type of IOSource.
   shared_ptr<ID> kind() const { return _kind; }

   shared_ptr<hilti::Type> iterType() override;
   shared_ptr<hilti::Type> elementType() override;

   bool _equal(shared_ptr<hilti::Type> other) const override {
       return trait::Parameterized::equal(other);
   }

   parameter_list parameters() const override;

   ACCEPT_VISITOR(Type);

private:
   node_ptr<ID> _kind;
};

namespace iterator {

class IOSource : public Iterator
{
public:
   /// Constructor for an iterator over an IOSource object.
   ///
   /// ty: The IOSource type to iterator over.
   ///
   /// l: Associated location.
   ///
   IOSource(shared_ptr<Type> ty, const Location& l=Location::None)
       : Iterator(ty, l) {}

   /// Constructor for an iterator over a widlcard IOSource object.
   ///
   /// l: Associated location.
   ///
   IOSource(const Location& l=Location::None)
       : IOSource { std::make_shared<type::IOSource>(l) } {}

   ACCEPT_VISITOR(Iterator);
};

}

namespace iterator {

/// A template class for defining container iterators.
template<typename T>
class ContainerIterator : public Iterator
{
protected:
   /// Constructor for an iterator over a container type.
   ///
   /// ctype: The container type this iterator iterates over.
   ///
   /// l: Associated location.
   ///
   ContainerIterator(shared_ptr<Type> ctype, const Location& l=Location::None) : Iterator(ctype) {}
};

class Map : public ContainerIterator<Map>
{
public:
   /// FIXMEL This should be "using
   /// ContainerIterator<Map>::ContainterIterator; ...", but clang doesn't
   /// like it yet.
   Map(shared_ptr<Type> ctype, const Location& l=Location::None) : ContainerIterator<Map>(ctype, l) {}
   ACCEPT_VISITOR(Iterator);
};

class Set : public ContainerIterator<Set>
{
public:
   /// FIXMEL This should be "using
   /// ContainerIterator<Set>::ContainterIterator; ...", but clang doesn't
   /// like it yet.
   Set(shared_ptr<Type> ctype, const Location& l=Location::None) : ContainerIterator<Set>(ctype, l) {}
   ACCEPT_VISITOR(Iterator);
};

class Vector : public ContainerIterator<Vector>
{
public:
   /// FIXMEL This should be "using
   /// ContainerIterator<Vector>::ContainterIterator; ...", but clang doesn't
   /// like it yet.
   Vector(shared_ptr<Type> ctype, const Location& l=Location::None) : ContainerIterator<Vector>(ctype, l) {}
   ACCEPT_VISITOR(Iterator);
};

class List : public ContainerIterator<List>
{
public:
   /// FIXMEL This should be "using
   /// ContainerIterator<List>::ContainterIterator; ...", but clang doesn't
   /// like it yet.
   List(shared_ptr<Type> ctype, const Location& l=Location::None) : ContainerIterator<List>(ctype, l) {}
   ACCEPT_VISITOR(Iterator);
};


}

/// Type for list objects.
class List : public TypedHeapType, public trait::Container
{
public:
   /// Constructor.
   ///
   /// etype: The type of the container's elements.
   ///
   /// l: Associated location.
   List(shared_ptr<Type> etype, const Location& l=Location::None)
       : TypedHeapType(etype, l) {}

   /// Constructor for a wildcard container type.
   List(const Location& l=Location::None) : TypedHeapType(l) {}

   virtual ~List();

   shared_ptr<hilti::Type> iterType() override {
       return std::make_shared<iterator::List>(this->sharedPtr<Type>(), location());
   }

   shared_ptr<hilti::Type> elementType() override {
       return argType();
   }

   ACCEPT_VISITOR(Type);
};

/// Type for vector objects.
class Vector : public TypedHeapType, public trait::Container
{
public:
   /// Constructor.
   ///
   /// etype: The type of the container's elements.
   ///
   /// l: Associated location.
   Vector(shared_ptr<Type> etype, const Location& l=Location::None)
       : TypedHeapType(etype, l) {}

   /// Constructor for a wildcard container type.
   Vector(const Location& l=Location::None) : TypedHeapType(l) {}

   virtual ~Vector();

   shared_ptr<hilti::Type> iterType() override {
       return std::make_shared<iterator::Vector>(this->sharedPtr<Type>(), location());
   }

   shared_ptr<hilti::Type> elementType() override {
       return argType();
   }

   ACCEPT_VISITOR(Type);
};

/// Type for vector objects.
class Set : public TypedHeapType, public trait::Container
{
public:
   /// Constructor.
   ///
   /// etype: The type of the container's elements.
   ///
   /// l: Associated location.
   Set(shared_ptr<Type> etype, const Location& l=Location::None)
       : TypedHeapType(etype, l) {}

   /// Constructor for a wildcard container type.
   Set(const Location& l=Location::None) : TypedHeapType(l) {}

   virtual ~Set();

   shared_ptr<hilti::Type> iterType() override {
       return std::make_shared<iterator::Set>(this->sharedPtr<Type>(), location());
   }

   shared_ptr<hilti::Type> elementType() override {
       return argType();
   }

   ACCEPT_VISITOR(Type);
};

/// Type for map objects.
class Map : public HeapType, public trait::Parameterized, public trait::Container
{
public:
   /// Constructor.
   ///
   /// key: The type of the container's index.
   ///
   /// value: The type of the container's values.
   ///
   /// l: Associated location.
   Map(shared_ptr<Type> key, shared_ptr<Type> value, const Location& l=Location::None)
       : HeapType(l) { _key = key; _value = value; addChild(_key); addChild(_value); }

   /// Constructor for a wildcard map type.
   Map(const Location& l=Location::None) : HeapType(l) { setWildcard(true); }

   virtual ~Map();

   shared_ptr<hilti::Type> iterType() override {
       return std::make_shared<iterator::Map>(this->sharedPtr<Type>(), location());
   }

   shared_ptr<hilti::Type> elementType() override;

   shared_ptr<Type> keyType() const { return _key; }
   shared_ptr<Type> valueType() const { return _value; }

   parameter_list parameters() const override;

   bool _equal(shared_ptr<hilti::Type> other) const override {
       return trait::Parameterized::equal(other);
   }

   ACCEPT_VISITOR(Type);

private:
   node_ptr<hilti::Type> _key = nullptr;
   node_ptr<hilti::Type> _value = nullptr;
};

/// Type for regexp instances.
class RegExp : public HeapType {
public:
   /// Constructor.
   ///
   /// l: Associated location.
   RegExp(const Location& l=Location::None) : HeapType(l) { }
   virtual ~RegExp();

   ACCEPT_VISITOR(Type);
};

/// Type for \c match_token_state
///
class MatchTokenState : public HeapType
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   MatchTokenState(const Location& l=Location::None) : HeapType(l) {}
   virtual ~MatchTokenState();

   ACCEPT_VISITOR(Type);
};

/// Type for a timer object.
///
class Timer : public HeapType
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   Timer(const Location& l=Location::None) : HeapType(l) {}
   virtual ~Timer();

   ACCEPT_VISITOR(Type);
};

/// Type for a timer_mgr object.
///
class TimerMgr : public HeapType
{
public:
   /// Constructor.
   ///
   /// l: Associated location.
   TimerMgr(const Location& l=Location::None) : HeapType(l) {}
   virtual ~TimerMgr();

   ACCEPT_VISITOR(Type);
};

namespace struct_ {

/// Definition of one struct field.
class Field : public Node, public NodeWithAttributes
{
public:
   /// id:  The name of the field.
   ///
   /// type: The type of the field.
   ///
   /// internal: If true, the field will not be printed when the struct
   /// type is rendered as a string. Internal IDS are also skipped from
   /// ctor expressions and list conversions.
   ///
   /// l: Location associated with the field.
   Field(shared_ptr<ID> id, shared_ptr<hilti::Type> type, bool internal=false, const Location& l=Location::None);

   shared_ptr<ID> id() const { return _id; }
   shared_ptr<hilti::Type> type() const { return _type; }

   /// This is a shortcut to querying the corresponding type attribute.
   shared_ptr<Expression> default_() const;
   bool internal() const { return _internal; }

   /// Returns true if the field is anonymous. Anonymous have their name
   /// skipped when printed at runtime. 
   bool anonymous() const { return _anonymous; }

   /// Marks the field as internal.
   void setInternal() { _internal = true; }

   /// Marks the field as internal.
   void setAnonymous() { _anonymous = true; }

   ACCEPT_VISITOR_ROOT();

private:
   node_ptr<ID> _id;
   node_ptr<hilti::Type> _type;
   bool _internal = false;
   bool _anonymous = false;
};


}

/// Type for structs.
class Struct : public HeapType, public trait::TypeList, public trait::Hashable, public trait::Parameterized
{
public:
   typedef std::list<node_ptr<struct_::Field>> field_list;

   /// Constructor.
   ///
   /// fields: The struct's fields.
   ///
   /// l: Associated location.
   Struct(const field_list& fields, const Location& l=Location::None);

   /// Constructor for a wildcard struct type matching any other.
   ///
   /// l: Associated location.
   Struct(const Location& l=Location::None);

   virtual ~Struct();

   /// Returns the list of fields.
   const field_list& fields() const { return _fields; }

   /// Adds a field.
   void addField(shared_ptr<struct_::Field> field);

   /// Returns the field of a given name, or null if no such field.
   shared_ptr<struct_::Field> lookup(shared_ptr<ID> id) const;

   /// Returns the field of a given name, or null if no such field.
   shared_ptr<struct_::Field> lookup(const std::string& name) const;

   /// Returns the 0-based index of the field of a given name, or -1 if no
   /// such field.
   int index(const std::string& name) const;

   const trait::TypeList::type_list typeList() const override;
   parameter_list parameters() const override;

   bool _equal(shared_ptr<hilti::Type> other) const override;

   ACCEPT_VISITOR(Type);

protected:
   // Returns a sorted list of field so that we have a well-defined order.
   field_list sortedFields();

private:
   std::list<node_ptr<struct_::Field>> _fields;
   string _dtor;
};


/// Type for a threading ``context``.
class Context : public Struct
{
public:
   /// Constructor.
   ///
   /// fields: The contexts's fields.
   ///
   /// l: Associated location.
   Context(const field_list& fields, const Location& l=Location::None);

   /// Constructor for a wildcard context type matching any other.
   ///
   /// l: Associated location.
   Context(const Location& l=Location::None)
       : Struct(l) {}

   virtual ~Context();

   ACCEPT_VISITOR(Struct);
};

namespace union_ {
    typedef struct_::Field Field;
}

/// Type for unions.
class Union : public ValueType, public trait::TypeList, public trait::Hashable, public trait::Parameterized
{
public:
   typedef std::list<node_ptr<union_::Field>> field_list;

   /// Constructor with names fields.
   ///
   /// fields: The union's fields.
   ///
   /// l: Associated location.
   Union(const field_list& fields, const Location& l=Location::None);

   /// Constructor with anonymous fields, resemling a variant.
   ///
   /// fields: The union's fields.
   ///
   /// l: Associated location.
   Union(const type_list& types, const Location& l=Location::None);

   /// Constructor for a wildcard union type matching any other.
   ///
   /// l: Associated location.
   Union(const Location& l=Location::None);

   virtual ~Union();

   /// Returns the list of fields. If this is union with anonymous fields,
   /// placeholder names will be filled in.
   const field_list& fields() const { return _fields; }

   /// Returns all fields of a givem type.
   ///
   /// type: The type to search for. It must match the fields directly, no
   /// coercion.
   field_list fields(shared_ptr<Type> type) const;

   /// Returns if this a union with anonymous fields.
   bool anonymousFields() const { return _anonymous; }

   /// Adds a field.
   void addField(shared_ptr<union_::Field> field);

   /// Returns the field of a given name. If there's no such field, including
   /// if this is a union with anonymous fields, returns null.
   shared_ptr<union_::Field> lookup(const string& name) const;

   /// Returns the field of a given name. If there's no such field, including
   /// if this is a union with anonymous fields, returns null.
   shared_ptr<union_::Field> lookup(shared_ptr<ID> id) const;

   /// Returns the 0-based index of the field of a given name, or -1 if no
   /// such field.
   int index(const std::string& name) const;

   const trait::TypeList::type_list typeList() const override;
   parameter_list parameters() const override;

   bool _equal(shared_ptr<hilti::Type> other) const override;

   ACCEPT_VISITOR(Type);

protected:
   // Returns a sorted list of field so that we have a well-defined order.
   field_list sortedFields();

private:
   std::list<node_ptr<union_::Field>> _fields;
   bool _anonymous;
   string _dtor;
};

/// Type for a threading ``scope``.
class Scope : public HiltiType {
public:
   typedef std::list<node_ptr<ID>> field_list;

   /// Constructor.
   ///
   /// fields: list of string - The field names of the context that are part of
   /// this scope.
   ///
   /// l: Associated location.
   Scope(const field_list& fields, const Location& l=Location::None) : HiltiType(l) {
       _fields = fields;
   }

   virtual ~Scope();

   /// Constructor creating a scope type matching any other scope type.
   Scope(const Location& l=Location::None) : HiltiType(l) {
       setWildcard(true);
   }

   // Returns the fields.
   const field_list& fields() const { return _fields; }

   /// Returns true if the scope uses a field of that name.
   bool hasField(shared_ptr<ID> id) const;

   bool _equal(shared_ptr<hilti::Type> other) const override;

   ACCEPT_VISITOR(Type);

private:
   field_list _fields;
   shared_ptr<hilti::Scope> _scope = nullptr;
};


////

}

}

#endif
