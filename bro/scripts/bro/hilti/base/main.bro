
module Hilti;

export {
	## Dump debug information about analyzers to stderr (for debugging only). 
	const dump_debug = F &redef;

	## Dump generated HILTI/BinPAC++ code to stderr (for debugging only).
	const dump_code = F &redef;

	## Dump generated HILTI/BinPAC++ code to stderr before finalizing the modules. (for
	## debugging only). 
	const dump_code_pre_finalize = F &redef;

	## Dump all HILTI/BinPAC++ code to stderr (for debugging only).
	const dump_code_all = F &redef;

	## Disable code verification (for debugging only).
	const no_verify = F &redef;

	## Generate code for all events no matter if they have a handler
	## defined or not.
	const compile_all = F &redef;

	## Enable debug mode for code generation.
	const debug = F &redef;

	## Enable optimization for code generation.
	const optimize = F &redef;

	## Profiling level for code generation.
	const profile = 0 &redef;

	## Tags for codegen debug output as colon-separated string.
	const cg_debug = "" &redef;

	## Save all generated BinPAC++ modules into "bro.<X>.pac2"
	const save_pac2 = F &redef;

	## Save all HILTI modules into "bro.<X>.hlt"
	const save_hilti = F &redef;

	## Save final linked LLVM assembly into "bro.ll"
	const save_llvm = F &redef;

	## Use on-disk cache for compiled modules.
	const use_cache = F &redef;

	## Activate the Bro script compiler.
	const compile_scripts = F &redef;

	## If we're compiling scripts, interface the BinPAC++ directly 
	## with the compiled events (rather than queuening them through Bro's
	## core.) 
	const pac2_to_compiler = T &redef;

	## Number of HILTI worker threads to spawn.
	const hilti_workers = 2 &redef;
}

event pac2_analyzer_for_port(a: Analyzer::Tag, p: port)
	{
	Analyzer::register_for_port(a, p);
	}

event pac2_analyzer_for_mime_type(a: Files::Tag, mt: string)
	{
	Files::register_for_mime_type(a, mt);
	}

