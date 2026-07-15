#pragma once

#include "RmlGD.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <string>

namespace RmlGodot {

class RmlContext;

/// Ergonomic, cached handle to a single data model — the nice way to drive
/// data binding without repeating (model_name) on every call. Returned by
/// RmlContext::get_data_model_handle (any model) and get_embedded_data (an
/// embed's, auto-namespaced), and injected into a document's <script> blocks as
/// `var data` (alongside `var rml_context`). Same object whether the model
/// belongs to a root document or an embed — that's the point.
///
/// All setters are upsert: a variable/array is bound on first use and updated
/// thereafter, so authors never juggle bind-vs-set.
///
/// Lifetime: holds a weak reference to the owning RmlContext (validated each
/// call) — methods no-op once the context is freed; check is_valid().
class RM_GD_CLASS(RmlDataModel, godot::RefCounted, {

	godot::ClassDB::bind_method(godot::D_METHOD("is_valid"), &RmlDataModel::is_valid);
	godot::ClassDB::bind_method(godot::D_METHOD("get_model_name"), &RmlDataModel::get_model_name);
	godot::ClassDB::bind_method(godot::D_METHOD("set_value", "key", "value"), &RmlDataModel::set_value);
	godot::ClassDB::bind_method(godot::D_METHOD("get_value", "key"), &RmlDataModel::get_value);
	godot::ClassDB::bind_method(godot::D_METHOD("update", "values"), &RmlDataModel::update);
	godot::ClassDB::bind_method(godot::D_METHOD("set_array", "name", "array"), &RmlDataModel::set_array);
	godot::ClassDB::bind_method(godot::D_METHOD("push", "name", "value"), &RmlDataModel::push);
	godot::ClassDB::bind_method(godot::D_METHOD("remove_at", "name", "index"), &RmlDataModel::remove_at);
	godot::ClassDB::bind_method(godot::D_METHOD("set_item", "name", "index", "value"), &RmlDataModel::set_item);
	godot::ClassDB::bind_method(godot::D_METHOD("array_size", "name"), &RmlDataModel::array_size);
	godot::ClassDB::bind_method(godot::D_METHOD("clear_array", "name"), &RmlDataModel::clear_array);
	godot::ClassDB::bind_method(godot::D_METHOD("bind_event", "name", "callable"), &RmlDataModel::bind_event);
	godot::ClassDB::bind_method(godot::D_METHOD("dirty", "key"), &RmlDataModel::dirty);
	godot::ClassDB::bind_method(godot::D_METHOD("dirty_all"), &RmlDataModel::dirty_all);

});

public:
	RmlDataModel() = default;
	~RmlDataModel() override = default;

	/// Internal: bind this handle to (context, model). Called by RmlContext.
	void setup(RmlContext* context, const std::string& model_name);

	bool is_valid() const;
	godot::String get_model_name() const { return godot::String(_model.c_str()); }

	void set_value(const godot::String& key, const godot::Variant& value);
	godot::Variant get_value(const godot::String& key) const;
	void update(const godot::Dictionary& values);

	void set_array(const godot::String& name, const godot::Array& array);
	void push(const godot::String& name, const godot::Variant& value);
	void remove_at(const godot::String& name, int index);
	void set_item(const godot::String& name, int index, const godot::Variant& value);
	int array_size(const godot::String& name) const;
	void clear_array(const godot::String& name);

	void bind_event(const godot::String& name, const godot::Callable& callable);

	void dirty(const godot::String& key);
	void dirty_all();

private:
	// Weak handle to the owning context (RmlContext is a Node, not RefCounted):
	// the instance id, resolved through ObjectDB each call so a freed context is
	// detected (returns null) rather than dereferenced.
	uint64_t _context_id = 0;
	std::string _model;

	RmlContext* _resolve() const;
};

} // namespace RmlGodot
