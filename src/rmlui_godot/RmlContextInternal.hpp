#pragma once

// Internal helpers shared by RmlContext's translation units. Not installed,
// not part of the public API.

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

// Thread-safety contract: every context API must run on the main thread —
// RmlUi has no internal locking, and binding callbacks fire from Update()
// during _process. A worker thread racing the data maps or DirtyVariable
// corrupts state silently, so catch it loudly (warn-once, near-zero cost).
inline void _warn_if_off_main_thread() {
	static bool warned = false;
	if (warned) return;
	auto* os = godot::OS::get_singleton();
	if (os != nullptr && os->get_thread_caller_id() != os->get_main_thread_id()) {
		warned = true;
		godot::UtilityFunctions::push_warning(
			"[RmlUi] Context API called from a non-main thread. RmlUi data binding "
			"is NOT thread-safe — marshal through call_deferred() or a main-thread "
			"queue. (Warning shown once; expect corruption or crashes if continued.)");
	}
}

} // namespace RmlGodot
