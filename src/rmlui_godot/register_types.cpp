#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/engine.hpp>

#include "RmlManager.hpp"
#include "RmlContext.hpp"
#include "RmlElementHandle.hpp"

void initialize_rmlui_godot(ModuleInitializationLevel p_level) {

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(RmlGodot::RmlManager)
		GDREGISTER_CLASS(RmlGodot::RmlContext)
		GDREGISTER_CLASS(RmlGodot::RmlElementHandle)

		auto* rml_manager = memnew(RmlGodot::RmlManager);
		godot::Engine::get_singleton()->register_singleton("RmlManager", rml_manager);
	}
}

void uninitialize_rmlui_godot(ModuleInitializationLevel p_level) {

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
		return;

	godot::Engine::get_singleton()->unregister_singleton("RmlManager");
	memdelete(RmlGodot::RmlManager::get_singleton());
}

extern "C" {

GDExtensionBool GDE_EXPORT rmlui_godot_init(
		GDExtensionInterfaceGetProcAddress p_get_proc_address,
		GDExtensionClassLibraryPtr p_library,
		GDExtensionInitialization* r_initialization) {

	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_rmlui_godot);
	init_obj.register_terminator(uninitialize_rmlui_godot);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}

}
