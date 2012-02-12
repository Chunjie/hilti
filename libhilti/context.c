// $Id$

#include <stdio.h>

#include "memory.h"
#include "context.h"
#include "rtti.h"

// Generated by the HILTI linker. It tells us how many bytes we need to store
// all thread-global variables.⎈
extern int64_t __hlt_globals_size;

// These functions initialize/destroy all the globals in the passed context
// to their default values. They are normally generated by the HILTI linker,
// but we provide a "weak" dummy implementation in case no module needs any of this.
__attribute__ ((weak)) void __hlt_globals_init(hlt_execution_context* ctx) {}
__attribute__ ((weak)) void __hlt_globals_dtor(hlt_execution_context* ctx) {}

static void _context_dtor(hlt_execution_context* ctx)
{
    __hlt_globals_dtor(ctx);
}

static hlt_type_info __hlt_type_info_execution_context = {
    HLT_TYPE_CONTEXT,                 // type
    sizeof(hlt_execution_context*),   // size
    "<execution context>",            // tag
    0,                                // num_params
    0,                                // aux
    (void*)-1,                        // ptr_map (shound't be used (yet?)).
    0,                                // to_string
    0,                                // to_int
    0,                                // to_double
    0,                                // hash
    0,                                // equal
    0,                                // blockable
    (void (*)(void*))_context_dtor    // dtor
};

hlt_execution_context* __hlt_execution_context_new(hlt_vthread_id vid)
{
    hlt_execution_context* ctx = (hlt_execution_context*)
        hlt_object_new(&__hlt_type_info_execution_context, sizeof(hlt_execution_context) + __hlt_globals_size);

    ctx->vid = vid;

#if 0
    ctx->yield->succ = func;
    ctx->yield->frame = 0;
    ctx->yield->eoss = 0;
#endif

    __hlt_globals_init(ctx);

    return ctx;
}

void __hlt_execution_context_ref(hlt_execution_context* ctx)
{
    hlt_object_ref(&__hlt_type_info_execution_context, ctx);
}

void __hlt_execution_context_unref(hlt_execution_context* ctx)
{
    hlt_object_unref(&__hlt_type_info_execution_context, ctx);
}
