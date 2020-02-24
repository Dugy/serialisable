#ifndef SERIALISABLE_POLYMORPHIC_HPP
#define SERIALISABLE_POLYMORPHIC_HPP

#include "serialisable.hpp"
#include "generic_factory/generic_factory.hpp"

struct SerialisablePolymorphic : public Serialisable, public SerialisableInternals::UnusualSerialisable {
	constexpr static char typeMember[] = "type";
protected:
	void subclass(const std::string& newName) {
		if (saving())
			synch(typeMember, const_cast<std::string&>(newName));
	}
#if __cplusplus > 201402L
	template <typename Parent, typename Child>
	struct polymorphism {
		polymorphism(const std::string& childName) {
			GenericFactory<Parent>::template registerChild<Child>(childName);
		}
		polymorphism(const char* childName) {
			GenericFactory<Parent>::template registerChild<Child>(childName);
		}
	};
#define SERIALISABLE_REGISTER_POLYMORPHIC(PARENT, CHILD, CHILD_NAME) \
	inline const static polymorphism<PARENT, CHILD> __polymorphism = CHILD_NAME;
#endif
};

namespace SerialisableInternals {

template <typename Serialised>
struct Serialiser<std::shared_ptr<Serialised>, std::enable_if_t<std::is_base_of<SerialisablePolymorphic, Serialised>::value>> {
	constexpr static bool valid = true;

	static Serialisable::JSON serialise(const std::shared_ptr<Serialised>& value) {
		if (value)
			return value->toJSON();
		else
			return Serialisable::JSON(); // null
	}

	static void deserialise(std::shared_ptr<Serialised>& result, Serialisable::JSON value) {
		auto type = value.object().find(SerialisablePolymorphic::typeMember);
		if (type == value.object().end())
			throw Serialisable::SerialisationError("Missing type information of polymorphic type");
		result = GenericFactory<Serialised>::createChild(type->second.string());
		result->fromJSON(value);
	}
};

template <typename Serialised>
struct Serialiser<std::unique_ptr<Serialised>, std::enable_if_t<std::is_base_of<SerialisablePolymorphic, Serialised>::value>> {
	constexpr static bool valid = true;

	static Serialisable::JSON serialise(const std::unique_ptr<Serialised>& value) {
		if (value)
			return value->toJSON();
		else
			return Serialisable::JSON(); // null
	}

	static void deserialise(std::unique_ptr<Serialised>& result, Serialisable::JSON value) {
		auto type = value.object().find(SerialisablePolymorphic::typeMember);
		if (type == value.object().end())
			throw Serialisable::SerialisationError("Missing type information of polymorphic type");
		result = GenericFactory<Serialised>::createChild(type->second.string());
		result->fromJSON(value);
	}
};

}

#endif // SERIALISABLE_POLYMORPHIC_HPP
