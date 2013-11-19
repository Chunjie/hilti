//
// Runtime functions supporting the generated HILTI/BinPAC++ code.
//
// These function all assume "HILTI-C" linkage. Note that they must all be
// 'extern "C"' here.
//
// All functions with names of the form <module>_<func>_bif will be
// automatically mapped to BiFs <module>::<func>. You must also declare them
// in scripts/bif (excluding the _bif postfix).

#include "Plugin.h"
#include "Conn.h"
#include "Val.h"
#include "Event.h"
#include "Rule.h"
#include "file_analysis/Manager.h"
#include "Cookie.h"
#undef DBG_LOG

extern "C" {
#include <libhilti/libhilti.h>
}

#include "LocalReporter.h"
#include "RuntimeInterface.h"

#undef List

#include <hilti/context.h>
#include <autogen/bro.pac2.h>

extern "C"  {

// Internal LibBro::* functions.

static bro::hilti::pac2_cookie::Protocol* get_protocol_cookie(void* cookie, const char *tag)
	{
	assert(cookie);
	auto c = ((bro::hilti::Pac2Cookie *)cookie);

	if ( c->type != bro::hilti::Pac2Cookie::PROTOCOL )
		bro::hilti::reporter::fatal_error(util::fmt("BinPAC++ error: %s cannot be used outside of protocol analysis", tag));

	return &c->protocol_cookie;
	}

static bro::hilti::pac2_cookie::File* get_file_cookie(void* cookie, const char *tag)
	{
	assert(cookie);
	auto c = ((bro::hilti::Pac2Cookie *)cookie);

	if ( c->type != bro::hilti::Pac2Cookie::FILE )
		bro::hilti::reporter::fatal_error(util::fmt("BinPAC++ error: %s cannot be used outside of file analysis", tag));

	return &c->file_cookie;
	}

::Val* libbro_cookie_to_conn_val(void* cookie, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto c = get_protocol_cookie(cookie, "$conn");
	return c->analyzer->Conn()->BuildConnVal();
	}

::Val* libbro_cookie_to_file_val(void* cookie, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto c = get_file_cookie(cookie, "$conn");
	auto f = c->analyzer->GetFile()->GetVal();
	Ref(f);
	return f;
	}

::Val* libbro_cookie_to_is_orig(void* cookie, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto c = get_protocol_cookie(cookie, "$is_orig");
	return new Val(c->is_orig, TYPE_BOOL);
	}

::Val* libbro_h2b_string(hlt_string s, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	const char* str = hlt_string_to_native(s, excpt, ctx);
	Val* v = new StringVal(str); // copies data.
	hlt_free((void *)str);
	return v;
	}

::Val* libbro_h2b_integer_signed(int64_t i, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new Val(i, TYPE_INT);
	}

::Val* libbro_h2b_integer_unsigned(uint64_t i, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new Val(i, TYPE_COUNT);
	}

::Val* libbro_h2b_address(hlt_addr addr, hlt_exception** excpt, hlt_execution_context* ctx)
	{
        if ( hlt_addr_is_v6(addr, excpt, ctx) )
		{
		struct in6_addr in6 = hlt_addr_to_in6(addr, excpt, ctx);
		return new AddrVal(IPAddr(in6));
		}
	else
		{
		struct in_addr in4 = hlt_addr_to_in4(addr, excpt, ctx);
		return new AddrVal(IPAddr(in4));
		}
	}

::Val* libbro_h2b_double(double d, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new Val(d, TYPE_DOUBLE);
	}

::Val* libbro_h2b_bool(int8_t b, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new Val(b, TYPE_BOOL);
	}

::Val* libbro_h2b_count(uint64_t v, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new Val(v, TYPE_COUNT);
	}

::Val* libbro_h2b_bytes(hlt_bytes* b, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	int len = hlt_bytes_len(b, excpt, ctx);
	char data[len];
	hlt_bytes_to_raw_buffer(b, (int8_t*)data, len, excpt, ctx);
	return new StringVal(len, data); // copies data.
	}

::Val* libbro_h2b_time(hlt_time t, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new Val(hlt_time_to_timestamp(t), TYPE_TIME);
	}

::Val* libbro_h2b_enum(const hlt_type_info* type, void* obj, uint64_t type_idx, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	hlt_enum e = *((hlt_enum *) obj);

	BroType* t = lib_bro_get_indexed_type(type_idx);
	EnumType* et = static_cast<EnumType *>(t);

	return hlt_enum_has_val(e)
		? new EnumVal(hlt_enum_value(e, excpt, ctx), et) : new EnumVal(lib_bro_enum_undef_val, et);
	}

::Val* libbro_h2b_enum_type(const hlt_type_info* type, void* obj, ::EnumType* etype, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	hlt_enum e = *((hlt_enum *) obj);

	return hlt_enum_has_val(e)
		? new EnumVal(hlt_enum_value(e, excpt, ctx), etype) : new EnumVal(lib_bro_enum_undef_val, etype);
	}

::Val* libbro_h2b_network(hlt_net n, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	uint8_t len = hlt_net_length(n, excpt, ctx);

        if ( hlt_net_is_v6(n, excpt, ctx) )
		{
		struct in6_addr in6 = hlt_net_to_in6(n, excpt, ctx);
		return new SubNetVal(IPPrefix(in6, len));
		}
	else
		{
		struct in_addr in4 = hlt_net_to_in4(n, excpt, ctx);
		return new SubNetVal(IPPrefix(in4, len - 96));
		}
	}

::Val* libbro_h2b_interval(hlt_interval i, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto nsecs = hlt_interval_nsecs(i, excpt, ctx);
	return new ::IntervalVal(nsecs / 1e9, 1);
	}

::Val* libbro_h2b_regexp(hlt_regexp* re, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	bro::hilti::reporter::internal_error("libbro_h2b_regexp() not implemented");
	return nullptr;
	}

::Val* libbro_h2b_port(hlt_port p, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	switch ( p.proto ) {
	case HLT_PORT_TCP:
		return new ::PortVal(p.port, ::TRANSPORT_TCP);

	case HLT_PORT_UDP:
		return new ::PortVal(p.port, ::TRANSPORT_UDP);

	default:
		bro::hilti::reporter::internal_error("unknown protocol in libbro_h2b_port");
		return nullptr;
	}
	}

hlt_bytes* libbro_b2h_string(Val *val, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	const BroString* s = val->AsString();
	hlt_bytes* result = hlt_bytes_new_from_data_copy((const int8_t*)s->Bytes(), s->Len(), excpt, ctx);
	Unref(val);
	return result;
	}

uint64_t libbro_b2h_count(Val *val, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return val->AsCount();
	}

uint8_t libbro_b2h_bool(Val *val, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return val->AsBool();
	}

hlt_addr libbro_b2h_address(Val *val, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto addr = val->AsAddr();

	switch ( addr.GetFamily() ) {
	case ::IPv4:
		{
		struct in_addr in;
		addr.CopyIPv4(&in);
		return hlt_addr_from_in4(in, excpt, ctx);
		}

	case ::IPv6:
		{
		struct in6_addr in;
		addr.CopyIPv6(&in);
		return hlt_addr_from_in6(in, excpt, ctx);
		}

	default:
		bro::hilti::reporter::internal_error("unknown addr type in libbro_b2h_address()");
		return hlt_addr();
	}
	}

hlt_net libbro_b2h_subnet(Val *val, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto net = val->AsSubNet();

	switch ( net.Prefix().GetFamily() ) {
	case ::IPv4:
		{
		struct in_addr in;
		net.Prefix().CopyIPv4(&in);
		return hlt_net_from_in4(in, net.Length() + 96, excpt, ctx);
		}

	case ::IPv6:
		{
		struct in6_addr in;
		net.Prefix().CopyIPv6(&in);
		return hlt_net_from_in6(in, net.Length(), excpt, ctx);
		}

	default:
		bro::hilti::reporter::internal_error("unknown net type in libbro_b2h_address()");
		return hlt_net();
	}
	}


hlt_port libbro_b2h_port(Val *val, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto port = val->AsPortVal();

	switch ( port->PortType() ) {
			
	case ::TRANSPORT_TCP:
		return { (uint16_t)port->Port(), HLT_PORT_TCP };

	case ::TRANSPORT_UDP:
		return { (uint16_t)port->Port(), HLT_PORT_UDP };

	default:
		bro::hilti::reporter::internal_error("unknown prot in libbro_b2h_port()");
		return hlt_port();
	}
	}

hlt_regexp* libbro_b2h_pattern(Val *val, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	bro::hilti::reporter::internal_error("libbro_b2h_pattern() not implemented");
	return 0;
	}

static EventHandler no_handler("SENTINEL_libbro_raise_event");

void* libbro_get_event_handler(hlt_bytes* name, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	hlt_bytes_size len = hlt_bytes_len(name, excpt, ctx);
	char evname[len + 1];
	hlt_bytes_to_raw_buffer(name, (int8_t*)evname, len, excpt, ctx);
	evname[len] = '\0';

	EventHandlerPtr ev = event_registry->Lookup(evname);

	return ev && ev.Ptr() ? ev.Ptr() : &no_handler;
	}

void libbro_raise_event(void* hdl, const hlt_type_info* type, const void* tuple, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	EventHandler* ev = (EventHandler*) hdl;

	if ( ev == &no_handler )
		return;

	int16_t* offsets = (int16_t *)type->aux;

	val_list* vals = new val_list(type->num_params);

	for ( int i = 0; i < type->num_params; i++ )
		{
		Val* broval = *((Val**)(((char*)tuple) + offsets[i]));
		vals->append(broval);
		}

	mgr.QueueEvent(EventHandlerPtr(ev), vals);
	}

::Val* libbro_call_bif_result(hlt_bytes* name, const hlt_type_info* type, const void* tuple, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	hlt_bytes_size len = hlt_bytes_len(name, excpt, ctx);
	char fname[len + 1];
	hlt_bytes_to_raw_buffer(name, (int8_t*)fname, len, excpt, ctx);
	fname[len] = '\0';

	::ID* fid = global_scope()->Lookup(fname);

	if ( ! fid )
		bro::hilti::reporter::internal_error(::util::fmt("unknown bif '%s' called in libbro_call_bif_result", string(fname)));

	assert(fid->ID_Val());
	auto func = fid->ID_Val()->AsFunc();
	assert(func->GetKind() == ::Func::BUILTIN_FUNC);

	int16_t* offsets = (int16_t *)type->aux;

	val_list* vals = new val_list;

	for ( int i = 0; i < type->num_params; i++ )
		{
		Val* broval = *((Val**)(((char*)tuple) + offsets[i]));
		vals->append(broval);
		}

	auto result = func->Call(vals);
	return result;
	}

void libbro_call_bif_void(hlt_bytes* name, const hlt_type_info* type, const void* tuple, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto result = libbro_call_bif_result(name, type, tuple, excpt, ctx);
	Unref(result);
	}

int64_t libbro_bro_internal_int(Val *val, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return val->InternalInt();
	}

double libbro_bro_internal_double(Val *val, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return val->InternalDouble();
	}

struct bro_table_iterate_result {
	::IterCookie* cookie;
	::Val* kval;
	::Val* vval;
};

bro_table_iterate_result libbro_bro_table_iterate(::TableVal* val, ::IterCookie* cookie)
	{
	auto tbl = val->AsTable();

	HashKey* h;
	TableEntryVal* v;

	if ( ! cookie )
		cookie = tbl->InitForIteration();

	if ( ! (v = tbl->NextEntry(h, cookie)) )
		return { 0, 0, 0 };

	::Val* kval = 0;

	auto k = val->RecoverIndex(h);
	if ( k->Length() == 1 )
		kval = k->Index(0);
	else
		kval = k;

	delete h;

	return { cookie, kval, v->Value() };
	}

::BroType* libbro_bro_base_type(uint64_t tag)
	{
	return ::base_type((TypeTag)(tag));
	}

::TableType* libbro_bro_table_type_new(::BroType* key, ::BroType* value, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new ::TableType(key->AsTypeList(), value);
	}

::TableVal* libbro_bro_table_new(::TableType* type, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new ::TableVal(type);
	}

void libbro_bro_table_insert(::TableVal* val, ::Val* k, ::Val* v, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	val->Assign(k, v);
	}

::ListVal* libbro_bro_list_new(hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new ListVal(::TYPE_ANY);
	}

void libbro_bro_list_append(::ListVal* lval, ::Val* val, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	lval->Append(val);
	}

::Val* libbro_bro_list_index(::ListVal* lval, uint64_t idx)
	{
	return lval->Index(idx);
	}

::TypeList* libbro_bro_list_type_new(::BroType* pure_type, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new ::TypeList(pure_type);
	}

void libbro_bro_list_type_append(::TypeList* t, ::BroType* ntype, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	t->Append(ntype);
	}

::RecordVal* libbro_bro_record_new(::RecordType* rtype, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new RecordVal(rtype);
	}

void libbro_bro_record_assign(::RecordVal* rval, uint64_t idx, ::Val* val, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	if ( ! val )
			return;

	rval->Assign(idx, val);
	}

::Val* libbro_bro_record_index(::RecordVal* rval, uint64_t idx)
	{
	return rval->Lookup(idx);
	}

::RecordType* libbro_bro_record_type_new(hlt_list* fields, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto decls = new type_decl_list;

	hlt_iterator_list i = hlt_list_begin(fields, excpt, ctx);
	hlt_iterator_list end = hlt_list_end(fields, excpt, ctx);

	while ( ! hlt_iterator_list_eq(i, end, excpt, ctx) )
		{
		auto td = *(::TypeDecl**) hlt_iterator_list_deref(i, excpt, ctx);
		decls->append(td);

		hlt_iterator_list j = i;
		i = hlt_iterator_list_incr(i, excpt, ctx);
		GC_DTOR(j, hlt_iterator_list);
		}

	GC_DTOR(i, hlt_iterator_list);
	GC_DTOR(end, hlt_iterator_list);

	return new RecordType(decls);
	}

::TypeDecl* libbro_bro_record_typedecl_new(hlt_string fname, ::BroType* ftype, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto n = hlt_string_to_native(fname, excpt, ctx);
	return new TypeDecl(ftype, n, 0, true);
	}

::VectorVal* libbro_bro_vector_new(::VectorType* vtype, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new VectorVal(vtype);
	}

void libbro_bro_vector_append(::VectorVal* vval, ::Val* val, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	vval->Assign(vval->Size(), val);
	}

::VectorType* libbro_bro_vector_type_new(::BroType* ytype, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new VectorType(ytype);
	}

struct bro_vector_iterate_result {
	::IterCookie* cookie;
	::Val* val;
};

bro_vector_iterate_result libbro_bro_vector_iterate(::VectorVal* val, ::IterCookie* cookie)
	{
	intptr_t idx = (intptr_t) cookie;

	if ( ! idx )
		idx = 1;

	if ( (idx - 1) >= val->Size() )
		return { 0, 0 };

	auto v = val->Lookup(idx - 1);

	if ( v )
		Ref(v);

	return { (::IterCookie *) (idx + 1), v };
	}

::SubNetType* libbro_bro_subnet_type_new(hlt_exception** excpt, hlt_execution_context* ctx)
	{
	return new SubNetType();
	}

::EnumType* libbro_bro_enum_type_new(hlt_string module, hlt_string name, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto n = hlt_string_to_native(name, excpt, ctx);
	auto m = hlt_string_to_native(module, excpt, ctx);

	auto id = lookup_ID(n, m);

	if ( id )
		{
		auto t = id->AsType();
		assert(t);
		Ref(t);
		hlt_free(n);
		hlt_free(m);
		return t->AsEnumType();
		}

	auto etype = new EnumType(n);
	hlt_free(n);
	hlt_free(m);
	return etype;
	}

void libbro_bro_enum_type_add_name(::EnumType* etype, hlt_string module, hlt_string name, uint64_t val, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto n = hlt_string_to_native(name, excpt, ctx);
	auto m = hlt_string_to_native(module, excpt, ctx);

	auto id = ::lookup_ID(n, m);

	if ( ! id )
		etype->AddName(m, n, val);

	hlt_free(n);
	hlt_free(m);
	}

// User-visible Bro::* functions.

void bro_file_begin(void* cookie, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	// Nothing to do.
	}

void bro_file_set_size(uint64_t size, void* cookie, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto c = get_protocol_cookie(cookie, "file_set_size()");
	file_mgr->SetSize(size, c->tag, c->analyzer->Conn(), c->is_orig);
	}

void bro_file_data_in(hlt_bytes* data, void* cookie, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto c = get_protocol_cookie(cookie, "file_data_in()");

	hlt_bytes_block block;
	hlt_iterator_bytes start = hlt_bytes_begin(data, excpt, ctx);
	hlt_iterator_bytes end = hlt_bytes_end(data, excpt, ctx);

	void* bcookie = 0;

	while ( true )
		{
		bcookie = hlt_bytes_iterate_raw(&block, bcookie, start, end, excpt, ctx);

		file_mgr->DataIn((const u_char*)block.start,
				 block.end - block.start,
				 c->tag,
				 c->analyzer->Conn(),
				 c->is_orig);

		if ( ! bcookie )
			break;
		}

	GC_DTOR(start, hlt_iterator_bytes);
	GC_DTOR(end, hlt_iterator_bytes);
	}

void bro_file_data_in_at_offset(hlt_bytes* data, uint64_t offset, void* cookie, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto c = get_protocol_cookie(cookie, "file_data_in_at_offset()");

	hlt_bytes_block block;
	hlt_iterator_bytes start = hlt_bytes_begin(data, excpt, ctx);
	hlt_iterator_bytes end = hlt_bytes_end(data, excpt, ctx);

	void* bcookie = 0;

	while ( true )
		{
		bcookie = hlt_bytes_iterate_raw(&block, bcookie, start, end, excpt, ctx);

		file_mgr->DataIn((const u_char*)block.start,
				 block.end - block.start,
				 offset,
				 c->tag,
				 c->analyzer->Conn(),
				 c->is_orig);

		if ( ! bcookie )
			break;

		offset += (block.end - block.start);
		}

	GC_DTOR(start, hlt_iterator_bytes);
	GC_DTOR(end, hlt_iterator_bytes);
	}

void bro_file_gap(uint64_t offset, uint64_t len, void* cookie, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto c = get_protocol_cookie(cookie, "file_gap()");
	file_mgr->Gap(offset, len, c->tag, c->analyzer->Conn(), c->is_orig);
	}

void bro_file_end(void* cookie, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto c = get_protocol_cookie(cookie, "file_end()");
	file_mgr->EndOfFile(c->tag, c->analyzer->Conn(), c->is_orig);
	}

void bro_dpd_confirm(void* cookie, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto c = get_protocol_cookie(cookie, "dpd_confirm()");
	c->analyzer->ProtocolConfirmation(c->tag);
	}

void bro_rule_match(hlt_enum pattern_type, hlt_bytes* data, int8_t bol, int8_t eol, int8_t clear, void* cookie, hlt_exception** excpt, hlt_execution_context* ctx)
	{
	auto c = get_protocol_cookie(cookie, "rule_match()");

	Rule::PatternType bro_type = Rule::TYPES;

	if ( hlt_enum_equal(Bro_PatternType_FINGER, pattern_type, excpt, ctx) )
		bro_type = Rule::PAYLOAD;
	else if ( hlt_enum_equal(Bro_PatternType_FINGER, pattern_type, excpt, ctx) )
		bro_type = Rule::PAYLOAD;
	else if ( hlt_enum_equal(Bro_PatternType_HTTP_REQUEST, pattern_type, excpt, ctx) )
		bro_type = Rule::HTTP_REQUEST;
	else if ( hlt_enum_equal(Bro_PatternType_HTTP_REQUEST_BODY, pattern_type, excpt, ctx) )
		bro_type = Rule::HTTP_REQUEST_BODY;
	else if ( hlt_enum_equal(Bro_PatternType_HTTP_REQUEST_HEADER, pattern_type, excpt, ctx) )
		bro_type = Rule::HTTP_REQUEST_HEADER;
	else if ( hlt_enum_equal(Bro_PatternType_HTTP_REPLY_BODY, pattern_type, excpt, ctx) )
		bro_type = Rule::HTTP_REPLY_BODY;
	else if ( hlt_enum_equal(Bro_PatternType_HTTP_REPLY_HEADER, pattern_type, excpt, ctx) )
		bro_type = Rule::HTTP_REPLY_HEADER;
	else if ( hlt_enum_equal(Bro_PatternType_FTP, pattern_type, excpt, ctx) )
		bro_type = Rule::FTP;
	else if ( hlt_enum_equal(Bro_PatternType_FINGER, pattern_type, excpt, ctx) )
		bro_type = Rule::FINGER;
	else
		bro::hilti::reporter::internal_error("unknown pattern type in bro_rule_match()");

	hlt_bytes_block block;
	hlt_iterator_bytes start = hlt_bytes_begin(data, excpt, ctx);
	hlt_iterator_bytes end = hlt_bytes_end(data, excpt, ctx);

	void* bcookie = 0;

	bool first = false;

	while ( true )
		{
		bcookie = hlt_bytes_iterate_raw(&block, bcookie, start, end, excpt, ctx);

		c->analyzer->Conn()->Match(bro_type,
					   (const u_char*)block.start,
					   block.end - block.start,
					   c->is_orig,
					   first ? bol : false,
					   bcookie ? false : eol,
					   first ? clear : false);

		if ( ! bcookie )
			break;

		first = false;
		}

	GC_DTOR(start, hlt_iterator_bytes);
	GC_DTOR(end, hlt_iterator_bytes);
	}

}
