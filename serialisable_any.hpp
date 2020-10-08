#include "serialisable.hpp"

namespace SerialisableAnyUtils {

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-template-friend"
#endif

// A class to make a unique type for each member of each class
template<typename T, int N> struct ObjectGetter {
	// Forward declares for ADL-indentification of functions
	friend Serialisable::JSON serialiseMember(ObjectGetter<T, N>, const T* instance, size_t offset);
	friend void deserialiseMember(ObjectGetter<T, N>, T* instance, size_t offset, const Serialisable::JSON& from);
	friend constexpr int memberSize(ObjectGetter<T, N>);
};
// The class that adds implementations to the functions forward declared above according to its parametres
template<typename T, int N, typename Stored>
struct ObjectDataStorage {
	friend Serialisable::JSON serialiseMember(ObjectGetter<T, N>, const T* instance, size_t offset) {
		static_assert(SerialisableInternals::Serialiser<Stored, void>::valid, "Invalid contained type");
		const Stored* object = reinterpret_cast<const Stored*>(reinterpret_cast<const uint8_t*>(instance) + offset);
		return SerialisableInternals::Serialiser<Stored, void>::serialise(*object);
	};
	friend void deserialiseMember(ObjectGetter<T, N>, T* instance, size_t offset, const Serialisable::JSON& from) {
		static_assert(SerialisableInternals::Serialiser<Stored, void>::valid, "Invalid contained type");
		Stored* object = reinterpret_cast<Stored*>(reinterpret_cast<uint8_t*>(instance) + offset);
		return SerialisableInternals::Serialiser<Stored, void>::deserialise(*object, from);
	}
	friend constexpr int memberSize(ObjectGetter<T, N>) {
		return sizeof(Stored);
	}
};

// The class whose conversions cause instantiations of ObjectDataStorage with the right member type
template<typename T, int N>
struct ObjectInspector {
	template <typename Inspected, std::enable_if_t<sizeof(ObjectDataStorage<T, N, Inspected>) != -1>* = nullptr>
	operator Inspected() {
		return Inspected{};
	}
};

// Partial template specialisation for recursively finding member count, initialising with more and more variables
template <typename T, typename sfinae, size_t... indexes>
struct MemberCounter {
	constexpr static size_t get() {
		return sizeof...(indexes) - 1;
	}
};

template <typename T, size_t... indexes>
struct MemberCounter<T, decltype( T {ObjectInspector<T, indexes>()...} )*, indexes...> {
	constexpr static size_t get() {
		return MemberCounter<T, T*, indexes..., sizeof...(indexes)>::get();
	}
};

// Calculation of padding, assuming all composite types contain word-sized members
// True for std::string, smart pointers and all STL containers (NOT std::array)
// Should use something specialised for all supported types in the final version
template <size_t previous, size_t size>
constexpr size_t padded() {
	constexpr size_t wordSize = sizeof(void*);
	return (size == 1 || size == 2 || size == 4 || (size == 8 && wordSize == 8)) ?
			((previous + size) % size == 0 ? previous : previous + size - (previous + size) % size) :
			((previous + size) % wordSize == 0 ? previous : previous + wordSize - (previous + size) % wordSize);
}

// Iteration through all elements, the first overload stops the recursion
template <typename T, size_t offset>
void serialiseMembers(const T* instance, Serialisable::JSON& output, std::index_sequence<>) { }

template <typename T, size_t offset, size_t index, size_t... otherIndexes>
void serialiseMembers(const T* instance, Serialisable::JSON& output, std::index_sequence<index, otherIndexes...>) {
	constexpr size_t size = memberSize(ObjectGetter<T, index>{});
	constexpr size_t paddedOffset = padded<offset, size>();
	output.array().push_back(serialiseMember(ObjectGetter<T, index>{}, instance, paddedOffset));
	serialiseMembers<T, paddedOffset + size>(instance, output, std::index_sequence<otherIndexes...>{});
}

// Same, for deserialisation

// Iteration through all elements, the first overload stops the recursion
template <typename T, size_t offset>
void deserialiseMembers(T* instance, const Serialisable::JSON& input, std::index_sequence<>) { }

template <typename T, size_t offset, size_t index, size_t... otherIndexes>
void deserialiseMembers(T* instance, const Serialisable::JSON& input, std::index_sequence<index, otherIndexes...>) {
	constexpr size_t size = memberSize(ObjectGetter<T, index>{});
	constexpr size_t paddedOffset = padded<offset, size>();
	deserialiseMember(ObjectGetter<T, index>{}, instance, paddedOffset, input[index]);
	deserialiseMembers<T, paddedOffset + size>(instance, input, std::index_sequence<otherIndexes...>{});
}

} // namespace

// The functions to be actually used
template <typename T>
Serialisable::JSON serialiseJsonObject(const T& instance) {
	Serialisable::JSON made = Serialisable::JSON::ArrayType();
	SerialisableAnyUtils::serialiseMembers<T, 0>(&instance, made,
			std::make_index_sequence<SerialisableAnyUtils::MemberCounter<T, T*>::get()>());
	return made;
}

template <typename T>
std::string writeJsonObject(const T& instance) {
	return serialiseJsonObject(instance).toString();
}

template <typename T>
T deserialiseJsonObject(const Serialisable::JSON& input) {
	T made;
	SerialisableAnyUtils::deserialiseMembers<T, 0>(&made, input,
			std::make_index_sequence<SerialisableAnyUtils::MemberCounter<T, T*>::get()>());
	return made;
}

template <typename T>
T readJsonObject(const std::string& input) {
	return deserialiseJsonObject<T>(Serialisable::JSON::fromString(input));
}

// Allow recursion, kinda limited in C++14 because std::is_aggregate is not present in C++14
namespace SerialisableInternals {
template <typename Serialised>
#if __cplusplus > 201402L
struct Serialiser<Serialised, std::enable_if_t<std::is_aggregate<Serialised>::value>>
#else
struct Serialiser<Serialised, std::enable_if_t<std::is_class<Serialised>::value && std::is_pod<Serialised>::value>>
#endif
{
	constexpr static bool valid = true;
	
	static Serialisable::JSON serialise(Serialised value) {
		return serialiseJsonObject(value);
	}

	static void deserialise(Serialised& result, Serialisable::JSON value) {
		result = deserialiseJsonObject<Serialised>(value);
	}
};
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
