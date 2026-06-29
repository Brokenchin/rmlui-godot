#include "RmlDataModel.hpp"
#include "RmlContext.hpp"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/object.hpp>

namespace RmlGodot {

void RmlDataModel::setup(RmlContext* context, const std::string& model_name) {
	_context_id = (context != nullptr) ? context->get_instance_id() : 0;
	_model = model_name;
}

RmlContext* RmlDataModel::_resolve() const {
	if (_context_id == 0) return nullptr;
	godot::Object* obj = godot::ObjectDB::get_instance(_context_id);
	return godot::Object::cast_to<RmlContext>(obj);
}

bool RmlDataModel::is_valid() const {
	RmlContext* ctx = _resolve();
	return ctx != nullptr && !_model.empty() &&
		ctx->has_data_model(godot::String(_model.c_str()));
}

void RmlDataModel::set_value(const godot::String& key, const godot::Variant& value) {
	if (RmlContext* ctx = _resolve())
		ctx->dm_set_value(godot::String(_model.c_str()), key, value);
}

godot::Variant RmlDataModel::get_value(const godot::String& key) const {
	if (RmlContext* ctx = _resolve())
		return ctx->get_data_variable(godot::String(_model.c_str()), key);
	return {};
}

void RmlDataModel::update(const godot::Dictionary& values) {
	RmlContext* ctx = _resolve();
	if (ctx == nullptr) return;
	const godot::String model(_model.c_str());
	godot::Array keys = values.keys();
	for (int i = 0; i < keys.size(); i++) {
		const godot::String key = keys[i];
		ctx->dm_set_value(model, key, values[keys[i]]);
	}
}

void RmlDataModel::set_array(const godot::String& name, const godot::Array& array) {
	if (RmlContext* ctx = _resolve())
		ctx->dm_set_array(godot::String(_model.c_str()), name, array);
}

void RmlDataModel::push(const godot::String& name, const godot::Variant& value) {
	if (RmlContext* ctx = _resolve())
		ctx->dm_push(godot::String(_model.c_str()), name, value);
}

void RmlDataModel::remove_at(const godot::String& name, int index) {
	if (RmlContext* ctx = _resolve())
		ctx->remove_data_array_item(godot::String(_model.c_str()), name, index);
}

void RmlDataModel::set_item(const godot::String& name, int index, const godot::Variant& value) {
	if (RmlContext* ctx = _resolve())
		ctx->set_data_array_item(godot::String(_model.c_str()), name, index, value);
}

int RmlDataModel::array_size(const godot::String& name) const {
	if (RmlContext* ctx = _resolve())
		return ctx->get_data_array_size(godot::String(_model.c_str()), name);
	return 0;
}

void RmlDataModel::clear_array(const godot::String& name) {
	if (RmlContext* ctx = _resolve())
		ctx->clear_data_array(godot::String(_model.c_str()), name);
}

void RmlDataModel::bind_event(const godot::String& name, const godot::Callable& callable) {
	if (RmlContext* ctx = _resolve())
		ctx->bind_data_event(godot::String(_model.c_str()), name, callable);
}

void RmlDataModel::dirty(const godot::String& key) {
	if (RmlContext* ctx = _resolve())
		ctx->dirty_data_variable(godot::String(_model.c_str()), key);
}

void RmlDataModel::dirty_all() {
	if (RmlContext* ctx = _resolve())
		ctx->dirty_all_variables(godot::String(_model.c_str()));
}

} // namespace RmlGodot
