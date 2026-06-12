// RmlContext — data model binding (see RmlContext.cpp for the TU map).
#include "RmlContext.hpp"
#include "RmlManager.hpp"
#include "RmlElementHandle.hpp"
#include "GodotEventListener.hpp"
#include "GodotFontInterface.hpp"
#include "GodotScriptDocument.hpp"

#include <algorithm>
#include <utility>
#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/StyleSheetContainer.h>
#include <RmlUi/Debugger.h>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector4.hpp>

#include "RmlContextInternal.hpp"

namespace {

Rml::Variant godot_to_rml_variant(const godot::Variant& gv) {
	switch (gv.get_type()) {
		case godot::Variant::BOOL:
			return Rml::Variant(static_cast<bool>(gv));
		case godot::Variant::INT:
			return Rml::Variant(static_cast<int>(static_cast<int64_t>(gv)));
		case godot::Variant::FLOAT:
			return Rml::Variant(static_cast<float>(static_cast<double>(gv)));
		case godot::Variant::STRING: {
			godot::String s = gv;
			return Rml::Variant(Rml::String(s.utf8().get_data()));
		}
		case godot::Variant::VECTOR2: {
			godot::Vector2 v = gv;
			return Rml::Variant(Rml::Vector2f(v.x, v.y));
		}
		default:
			if (gv.get_type() != godot::Variant::NIL) {
				godot::String s = gv.stringify();
				return Rml::Variant(Rml::String(s.utf8().get_data()));
			}
			return Rml::Variant();
	}
}

godot::Variant rml_to_godot_variant(const Rml::Variant& rv) {
	switch (rv.GetType()) {
		case Rml::Variant::BOOL:
			return godot::Variant(rv.Get<bool>());
		case Rml::Variant::INT:
			return godot::Variant(static_cast<int64_t>(rv.Get<int>()));
		case Rml::Variant::INT64:
			return godot::Variant(rv.Get<int64_t>());
		case Rml::Variant::FLOAT:
			return godot::Variant(static_cast<double>(rv.Get<float>()));
		case Rml::Variant::DOUBLE:
			return godot::Variant(rv.Get<double>());
		case Rml::Variant::STRING:
			return godot::Variant(godot::String(rv.Get<Rml::String>().c_str()));
		case Rml::Variant::VECTOR2: {
			auto v = rv.Get<Rml::Vector2f>();
			return godot::Variant(godot::Vector2(v.x, v.y));
		}
		default:
			return godot::Variant();
	}
}

Rml::String godot_variant_to_rml_string(const godot::Variant& gv) {
	godot::String s = gv.stringify();
	return Rml::String(s.utf8().get_data());
}

Rml::Vector<Rml::String> godot_array_to_rml_string_vector(const godot::Array& arr) {
	Rml::Vector<Rml::String> result;
	result.reserve(arr.size());
	for (int i = 0; i < arr.size(); i++) {
		result.push_back(godot_variant_to_rml_string(arr[i]));
	}
	return result;
}

} // anonymous namespace

namespace RmlGodot {

bool RmlContext::create_data_model(const godot::String& model_name) {
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot create data model — context not initialized");
		return false;
	}

	std::string name(model_name.utf8().get_data());
	if (_data_models.count(name)) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Data model already exists: ") + model_name);
		return false;
	}

	Rml::DataModelConstructor constructor = _rml_context->CreateDataModel(Rml::String(name));
	if (!constructor) {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] Failed to create data model: ") + model_name);
		return false;
	}

	DataModelEntry entry;
	entry.constructor = constructor;
	entry.handle = constructor.GetModelHandle();
	_data_models[name] = std::move(entry);

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] Data model created: ") + model_name);
	return true;
}

RmlContext::DataModelEntry* RmlContext::_get_data_model(const godot::String& model_name, bool warn) {
	_warn_if_off_main_thread();
	auto it = _data_models.find(std::string(model_name.utf8().get_data()));
	if (it == _data_models.end()) {
		if (warn) {
			godot::UtilityFunctions::push_warning(
				godot::String("[RmlUi] Data model not found: ") + model_name);
		}
		return nullptr;
	}
	return &it->second;
}

const RmlContext::DataModelEntry* RmlContext::_get_data_model(const godot::String& model_name, bool warn) const {
	return const_cast<RmlContext*>(this)->_get_data_model(model_name, warn);
}

Rml::Vector<Rml::String>* RmlContext::_get_data_array(DataModelEntry& model,
	const godot::String& array_name, bool warn) {

	auto it = model.arrays.find(std::string(array_name.utf8().get_data()));
	if (it == model.arrays.end()) {
		if (warn) {
			godot::UtilityFunctions::push_warning(
				godot::String("[RmlUi] Array not bound: ") + array_name);
		}
		return nullptr;
	}
	return &it->second;
}

const Rml::Vector<Rml::String>* RmlContext::_get_data_array(const DataModelEntry& model,
	const godot::String& array_name, bool warn) {
	return _get_data_array(const_cast<DataModelEntry&>(model), array_name, warn);
}

bool RmlContext::bind_data_variable(const godot::String& model_name,
	const godot::String& variable_name, const godot::Variant& initial_value) {

	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot bind variable — context not initialized");
		return false;
	}

	DataModelEntry* model = _get_data_model(model_name);
	if (model == nullptr) return false;

	std::string vname(variable_name.utf8().get_data());
	model->variables[vname] = godot_to_rml_variant(initial_value);

	auto* vars = &model->variables;
	std::string captured_vname = vname;

	model->constructor.BindFunc(
		Rml::String(vname),
		[vars, captured_vname](Rml::Variant& variant) {
			auto found = vars->find(captured_vname);
			if (found != vars->end())
				variant = found->second;
		},
		[vars, captured_vname](const Rml::Variant& variant) {
			(*vars)[captured_vname] = variant;
		}
	);

	return true;
}

void RmlContext::set_data_variable(const godot::String& model_name,
	const godot::String& variable_name, const godot::Variant& value) {

	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot set data variable — context not initialized");
		return;
	}

	DataModelEntry* model = _get_data_model(model_name);
	if (model == nullptr) return;

	std::string vname(variable_name.utf8().get_data());
	model->variables[vname] = godot_to_rml_variant(value);
	model->handle.DirtyVariable(Rml::String(vname));
	_render_dirty = true;
}

godot::Variant RmlContext::get_data_variable(const godot::String& model_name,
	const godot::String& variable_name) const {

	const DataModelEntry* model = _get_data_model(model_name, false);
	if (model == nullptr) return godot::Variant();

	std::string vname(variable_name.utf8().get_data());
	auto vit = model->variables.find(vname);
	if (vit == model->variables.end()) return godot::Variant();

	return rml_to_godot_variant(vit->second);
}

bool RmlContext::bind_data_event(const godot::String& model_name,
	const godot::String& event_name, const godot::Callable& callable) {

	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot bind event — context not initialized");
		return false;
	}

	DataModelEntry* model = _get_data_model(model_name);
	if (model == nullptr) return false;

	std::string ename(event_name.utf8().get_data());
	model->event_callbacks[ename] = callable;

	auto* callbacks = &model->event_callbacks;
	std::string captured_ename = ename;

	model->constructor.BindEventCallback(
		Rml::String(ename),
		[callbacks, captured_ename](Rml::DataModelHandle /*handle*/, Rml::Event& /*event*/,
			const Rml::VariantList& arguments) {
			auto found = callbacks->find(captured_ename);
			if (found == callbacks->end()) return;

			godot::Array args;
			for (const auto& arg : arguments) {
				args.append(rml_to_godot_variant(arg));
			}
			found->second.callv(args);
		}
	);

	return true;
}

void RmlContext::dirty_data_variable(const godot::String& model_name,
	const godot::String& variable_name) {

	if (_rml_context == nullptr) return;

	DataModelEntry* model = _get_data_model(model_name, false);
	if (model == nullptr) return;

	model->handle.DirtyVariable(Rml::String(std::string(variable_name.utf8().get_data())));
	_render_dirty = true;
}

void RmlContext::dirty_all_variables(const godot::String& model_name) {
	if (_rml_context == nullptr) return;

	DataModelEntry* model = _get_data_model(model_name, false);
	if (model == nullptr) return;

	model->handle.DirtyAllVariables();
	_render_dirty = true;
}

bool RmlContext::create_data_model_from_dict(const godot::String& model_name,
	const godot::Dictionary& variables) {

	if (!create_data_model(model_name)) return false;

	godot::Array keys = variables.keys();
	for (int i = 0; i < keys.size(); i++) {
		godot::String key = keys[i];
		bind_data_variable(model_name, key, variables[key]);
	}

	return true;
}

void RmlContext::update_data_model(const godot::String& model_name,
	const godot::Dictionary& variables) {

	DataModelEntry* model = _get_data_model(model_name);
	if (model == nullptr) return;

	godot::Array keys = variables.keys();
	for (int i = 0; i < keys.size(); i++) {
		godot::String key = keys[i];
		std::string vname(key.utf8().get_data());
		auto var_it = model->variables.find(vname);
		if (var_it != model->variables.end()) {
			var_it->second = godot_to_rml_variant(variables[key]);
			model->handle.DirtyVariable(Rml::String(vname));
			_render_dirty = true;
		}
	}
}

// --- Phase 3: Array data binding ---

bool RmlContext::bind_data_array(const godot::String& model_name,
	const godot::String& array_name, const godot::Array& initial_array) {

	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot bind array — context not initialized");
		return false;
	}

	DataModelEntry* model = _get_data_model(model_name);
	if (model == nullptr) return false;

	std::string aname(array_name.utf8().get_data());
	if (model->arrays.count(aname)) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Array already bound: ") + array_name);
		return false;
	}

	// RmlUi's data type register is per-context, so this must be tracked
	// per RmlContext instance — a global flag breaks every context after
	// the first one (including the editor preview context).
	if (!_array_type_registered) {
		model->constructor.RegisterArray<Rml::Vector<Rml::String>>();
		_array_type_registered = true;
	}

	model->arrays[aname] = godot_array_to_rml_string_vector(initial_array);

	auto* array_ptr = &model->arrays[aname];
	model->constructor.Bind(Rml::String(aname), array_ptr);

	return true;
}

void RmlContext::set_data_array(const godot::String& model_name,
	const godot::String& array_name, const godot::Array& array) {

	DataModelEntry* model = _get_data_model(model_name);
	if (model == nullptr) return;

	Rml::Vector<Rml::String>* arr = _get_data_array(*model, array_name);
	if (arr == nullptr) return;

	*arr = godot_array_to_rml_string_vector(array);
	model->handle.DirtyVariable(Rml::String(array_name.utf8().get_data()));
	_render_dirty = true;
}

void RmlContext::push_data_array_item(const godot::String& model_name,
	const godot::String& array_name, const godot::Variant& value) {

	DataModelEntry* model = _get_data_model(model_name);
	if (model == nullptr) return;

	Rml::Vector<Rml::String>* arr = _get_data_array(*model, array_name);
	if (arr == nullptr) return;

	arr->push_back(godot_variant_to_rml_string(value));
	model->handle.DirtyVariable(Rml::String(array_name.utf8().get_data()));
	_render_dirty = true;
}

void RmlContext::remove_data_array_item(const godot::String& model_name,
	const godot::String& array_name, int index) {

	DataModelEntry* model = _get_data_model(model_name);
	if (model == nullptr) return;

	Rml::Vector<Rml::String>* arr = _get_data_array(*model, array_name);
	if (arr == nullptr) return;

	if (index < 0 || index >= static_cast<int>(arr->size())) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Array index out of bounds: ") + godot::String::num_int64(index));
		return;
	}

	arr->erase(arr->begin() + index);
	model->handle.DirtyVariable(Rml::String(array_name.utf8().get_data()));
	_render_dirty = true;
}

void RmlContext::set_data_array_item(const godot::String& model_name,
	const godot::String& array_name, int index, const godot::Variant& value) {

	DataModelEntry* model = _get_data_model(model_name);
	if (model == nullptr) return;

	Rml::Vector<Rml::String>* arr = _get_data_array(*model, array_name);
	if (arr == nullptr) return;

	if (index < 0 || index >= static_cast<int>(arr->size())) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Array index out of bounds: ") + godot::String::num_int64(index));
		return;
	}

	(*arr)[index] = godot_variant_to_rml_string(value);
	model->handle.DirtyVariable(Rml::String(array_name.utf8().get_data()));
	_render_dirty = true;
}

int RmlContext::get_data_array_size(const godot::String& model_name,
	const godot::String& array_name) const {

	const DataModelEntry* model = _get_data_model(model_name, false);
	if (model == nullptr) return 0;

	const Rml::Vector<Rml::String>* arr = _get_data_array(*model, array_name, false);
	if (arr == nullptr) return 0;

	return static_cast<int>(arr->size());
}

void RmlContext::clear_data_array(const godot::String& model_name,
	const godot::String& array_name) {

	DataModelEntry* model = _get_data_model(model_name);
	if (model == nullptr) return;

	Rml::Vector<Rml::String>* arr = _get_data_array(*model, array_name);
	if (arr == nullptr) return;

	arr->clear();
	model->handle.DirtyVariable(Rml::String(array_name.utf8().get_data()));
	_render_dirty = true;
}

// --- Phase 5: Custom element instancers ---

} // namespace RmlGodot
