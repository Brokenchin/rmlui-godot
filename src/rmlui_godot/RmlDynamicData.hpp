#pragma once

// Dynamic data-variable backing for struct/object array binding.
//
// RmlUi's data binding is normally statically typed: structs register member
// pointers at compile time. To bind a *dynamic* Godot Dictionary (whose members
// are only known at runtime) we mirror the value into a small node tree and feed
// RmlUi a trio of custom VariableDefinitions that interpret each node's pointer.
//
// Children are heap-allocated (UniquePtr) so their addresses stay stable while
// the parent's containers grow or shrink — RmlUi's data model holds raw void*
// pointers into this tree for the whole lifetime of the binding.

#include <RmlUi/Core/DataVariable.h>
#include <RmlUi/Core/Variant.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RmlGodot {

struct DynNode {
	enum class Kind { Scalar, Struct, Array };
	Kind kind = Kind::Scalar;

	// Kind::Scalar
	Rml::Variant scalar;

	// Kind::Struct — members kept in insertion order for stable reflection.
	std::vector<std::string> member_order;
	std::unordered_map<std::string, std::unique_ptr<DynNode>> members;

	// Kind::Array
	std::vector<std::unique_ptr<DynNode>> elements;
};

class DynDataRegistry;

// Scalar leaf: read/write the node's stored variant directly.
class DynScalarDef final : public Rml::VariableDefinition {
public:
	DynScalarDef() : Rml::VariableDefinition(Rml::DataVariableType::Scalar) {}

	bool Get(void* ptr, Rml::Variant& variant) override {
		variant = static_cast<DynNode*>(ptr)->scalar;
		return true;
	}
	bool Set(void* ptr, const Rml::Variant& variant) override {
		static_cast<DynNode*>(ptr)->scalar = variant;
		return true;
	}
};

// Struct node: resolve a named member to a child variable.
class DynStructDef final : public Rml::VariableDefinition {
public:
	explicit DynStructDef(DynDataRegistry* registry)
		: Rml::VariableDefinition(Rml::DataVariableType::Struct), _registry(registry) {}

	Rml::DataVariable Child(void* ptr, const Rml::DataAddressEntry& address) override;
	Rml::StringList ReflectMemberNames() override { return {}; }

private:
	DynDataRegistry* _registry;
};

// Array node: report size and resolve an indexed element to a child variable.
class DynArrayDef final : public Rml::VariableDefinition {
public:
	explicit DynArrayDef(DynDataRegistry* registry)
		: Rml::VariableDefinition(Rml::DataVariableType::Array), _registry(registry) {}

	int Size(void* ptr) override {
		return static_cast<int>(static_cast<DynNode*>(ptr)->elements.size());
	}
	Rml::DataVariable Child(void* ptr, const Rml::DataAddressEntry& address) override;

private:
	DynDataRegistry* _registry;
};

// Owns the three definitions (which the data model references for its lifetime)
// and the root node of every dynamic array bound within one data model. The
// registry is heap-allocated and never moved, so the definition pointers handed
// to RmlUi stay valid.
class DynDataRegistry {
public:
	DynDataRegistry() : _struct(this), _array(this) {}

	Rml::DataVariable make_variable(DynNode* node) {
		switch (node->kind) {
			case DynNode::Kind::Scalar: return Rml::DataVariable(&_scalar, node);
			case DynNode::Kind::Struct: return Rml::DataVariable(&_struct, node);
			case DynNode::Kind::Array:  return Rml::DataVariable(&_array, node);
		}
		return Rml::DataVariable();
	}

	std::unordered_map<std::string, std::unique_ptr<DynNode>> roots;

private:
	DynScalarDef _scalar;
	DynStructDef _struct;
	DynArrayDef _array;
};

inline Rml::DataVariable DynStructDef::Child(void* ptr, const Rml::DataAddressEntry& address) {
	DynNode* node = static_cast<DynNode*>(ptr);
	auto it = node->members.find(std::string(address.name.c_str()));
	if (it == node->members.end())
		return Rml::DataVariable();
	return _registry->make_variable(it->second.get());
}

inline Rml::DataVariable DynArrayDef::Child(void* ptr, const Rml::DataAddressEntry& address) {
	DynNode* node = static_cast<DynNode*>(ptr);
	const int size = static_cast<int>(node->elements.size());
	const int index = address.index;
	if (index < 0 || index >= size) {
		// Mirror RmlUi's built-in ArrayDefinition: expose `.size`.
		if (address.name == "size")
			return Rml::MakeLiteralIntVariable(size);
		return Rml::DataVariable();
	}
	return _registry->make_variable(node->elements[index].get());
}

} // namespace RmlGodot
