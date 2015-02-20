
#include "define-instruction.h"

// Most operators are implemented by overloading rather than by providing a
// direct instruction. For these, we still creates instructions here so that
// we have a class "operator_::XXX" to use with the builder interface.
// However, we name their mnemonics by starting them with a ".op", turning
// them into internal ones that can't be looked up. If used, the statement
// resolver will replace them with the correct overloaded version.

iBegin(operator_, Begin, ".op.begin")
    iTarget(optype::any)
    iOp1(optype::any, true)
    iValidate {}
    iDoc(R"(
        Returns an iterator pointing to the first element of an iterable sequence.
    )")
iEnd

iBegin(operator_, End, ".op.end")
    iTarget(optype::any)
    iOp1(optype::any, true)
    iValidate {}
    iDoc(R"(
        Returns an iterator pointing one beyond the last element of an iterable sequence.
    )")
iEnd

iBegin(operator_, Incr, ".op.incr")
    iTarget(optype::any)
    iOp1(optype::any, true)
    iValidate {}
    iDoc(R"(
        Increments an iterator by one element.
    )")
iEnd

iBegin(operator_, IncrBy, ".op.incr_by")
    iTarget(optype::any)
    iOp1(optype::any, true)
    iOp2(optype::int64, true)
    iValidate {}
    iDoc(R"(
        Increments an iterator by a given number of elements.
    )")
iEnd

iBegin(operator_, Deref, ".op.deref")
    iTarget(optype::any)
    iOp1(optype::any, true)
    iValidate {}
    iDoc(R"(
        Dereferences an interator.
    )")
iEnd

iBegin(operator_, Equal, ".op.equal")
    iTarget(optype::any)
    iOp1(optype::any, true)
    iOp2(optype::any, true)
    iValidate {}
    iDoc(R"(
        Compares two values of the same type, assuming the type supports comparision.
    )")
iEnd

iBegin(operator_, Unequal, "unequal")
    iTarget(optype::boolean)
    iOp1(optype::any, true)
    iOp2(optype::any, true)
    iValidate {}
    iDoc(R"(
        Compares two values of the same type, returning true if they don't match. This operator
        is defined for all types that offer an ``equal`` operator.
     )")
iEnd

iBegin(operator_, Assign, "assign")
    iTarget(optype::any)
    iOp1(optype::any, false)

    iValidate {
        if ( ! type::hasTrait<type::trait::ValueType>(target->type()) ) {
            error(target, "target must be a value type");
            return;
        }

        if ( ! type::hasTrait<type::trait::ValueType>(op1->type()) ) {
            error(op1, "operand must be a value type");
            return;
        }

        if ( ! op1->canCoerceTo(target->type()) )
            error(op1, "operand not compatible with target");
    }

    iDoc(R"(
        Assigns *op1* to the target.  There is a short-cut syntax: instead of
        using the standard form ``t = assign op``, one can just write ``t =
        op``.
    )")
iEnd

iBegin(operator_, Unpack, "unpack")
    iTarget(optype::tuple)
    iOp1(optype::tuple, true)
    iOp2(optype::enum_, true)
    iOp3(optype::optional(optype::any), true)

    iValidate {
        // TODO
    }

    iDoc(R"(
    Unpacks an instance of a particular type (as determined by *target*;
    see below) from the binary data enclosed by the iterator tuple *op1*.
    *op2* defines the binary layout as an enum of type ``Hilti::Packed`` and
    must be a constant. Depending on *op2*, *op3* is may be an additional,
    format-specific parameter with further information about the binary
    layout. The operator returns a ``tuple<T, iterator<bytes>>``, in the first
    component is the newly unpacked instance and the second component is
    locates the first bytes that has *not* been consumed anymore.

    Raises ~~WouldBlock if there are not sufficient bytes available
    for unpacking the type. Can also raise ``UnpackError` if the raw bytes
    are not as expected (and that fact can be verified).

    Note: The ``unpack`` operator uses a generic implementation able to handle all data
    types. Different from most other operators, it's implementation is not
    overloaded on a per-type based. Instead, the type specific code is implemented in the codegen::Unpacker.
    )")
iEnd

iBegin(operator_, Pack, "pack")
    iTarget(optype::refBytes)
    iOp1(optype::any, true)
    iOp2(optype::enum_, true)
    iOp3(optype::optional(optype::any), true)

    iValidate {
        // TODO
    }

    iDoc(R"(
    Packs a value *op1* of a particular type into binary data enclosed by the iterator tuple *op1*.
    *op2* defines the binary layout as an enum of type ``Hilti::Packed`` and
    must be a constant. Depending on *op2*, *op3* is may be an additional,
    format-specific parameter with further information about the binary
    layout. The operator returns a ``ref<bytes>`` with the packed data.

    Note: The ``unpack`` operator uses a generic implementation able to handle all data
    types. Different from most other operators, it's implementation is not
    overloaded on a per-type based. Instead, the type specific code is implemented in the codegen::Packer.
    )")
iEnd

iBegin(operator_, Clear, "clear")
    iOp1(optype::any, false)
    iValidate {}
    iDoc(R"(
        Resets *op1* to the default value a new variable would be set to.

        Note: This operator is automatically defined for all value types.
    )")
iEnd

iBegin(operator_, Clone, "clone")
    iTarget(optype::any)
    iOp1(optype::any, true)
    iValidate {
        equalTypes(target, op1);
    }
    iDoc(R"(
        Returns a deep copy of *op1*.
    )")
iEnd

iBegin(operator_, Hash, "hash")
    iTarget(optype::int64);
    iOp1(optype::any, true)
    iValidate {
    }
    iDoc(R"(
        Returns an integer hash of *op1*.
    )")
iEnd
