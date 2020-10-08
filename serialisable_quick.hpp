#include "serialisable.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-template-friend"
#pragma GCC diagnostic ignored "-Wnarrowing"
#endif

template <typename Child>
class SerialisableQuick;

namespace SerialisableQuickInternals {

// A class to make a unique type for each member of each class
template<typename T, int N> struct ObjectGetter {
	// Forward declares for ADL-indentification of functions
	friend Serialisable::JSON serialiseMember(ObjectGetter<T, N>, const T* instance, size_t offset);
	friend void deserialiseMember(ObjectGetter<T, N>, T* instance, size_t offset, const Serialisable::JSON& from);
	friend bool isSerialised(ObjectGetter<T, N>, const T* instance, size_t offset);
	friend constexpr int memberSize(ObjectGetter<T, N>);
	friend constexpr int memberAlignment(ObjectGetter<T, N>);
};
// The class that adds implementations to the functions forward declared above according to its parametres
template<typename T, size_t N, typename Stored>
struct ObjectDataStorage {
   	using msvcPleaser = int; // MSVC won't compile it without this
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
	friend constexpr int memberAlignment(ObjectGetter<T, N>) {
		return alignof(Stored);
	}
};

// The class whose conversions cause instantiations of ObjectDataStorage with the right member type
template<typename T, int N>
struct ObjectInspector {
	template <typename Inspected, typename ObjectDataStorage<T, N, Inspected>::msvcPleaser = 0>
	operator Inspected() {
		return Inspected{};
	}
};

struct FakeObjectInspector {
	template <typename Inspected>
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
struct MemberCounter<T, decltype( T { {FakeObjectInspector()}, ObjectInspector<T, indexes>()...} )*, indexes...> {
	constexpr static size_t get() {
		return MemberCounter<T, T*, indexes..., sizeof...(indexes)>::get();
	}
};

// Calculation of padding, assuming all composite types contain word-sized members
// True for std::string, smart pointers and all STL containers (NOT std::array)
// Should use something specialised for all supported types in the final version
template <size_t previous, size_t alignment>
constexpr size_t padded() {
	return previous % alignment ? previous - (previous % alignment) + alignment : previous;
}

template <typename T, size_t index, size_t offset>
constexpr size_t getPaddedOffset() {
	constexpr size_t size = memberSize(ObjectGetter<T, index>{});
	constexpr size_t alignment = memberAlignment(ObjectGetter<T, index>{});
	constexpr size_t paddedOffset = padded<offset, alignment>();
	return paddedOffset;
}


// Iteration through all elements, the first overload stops the recursion
template <typename T, size_t offset>
void serialiseMembers(const T* instance, Serialisable::JSON& output, std::index_sequence<>) { }

template <typename T, size_t offset, size_t index, size_t... otherIndexes>
void serialiseMembers(const T* instance, Serialisable::JSON& output, std::index_sequence<index, otherIndexes...>) {
	constexpr size_t paddedOffset = getPaddedOffset<T, index, offset>();
	const char* name = T::memberName(index);
	if (name)
		output[name] = serialiseMember(ObjectGetter<T, index>{}, instance, paddedOffset);
	serialiseMembers<T, paddedOffset + memberSize(ObjectGetter<T, index>{})>(instance, output, std::index_sequence<otherIndexes...>{});
}

// Same, for deserialisation

// Iteration through all elements, the first overload stops the recursion
template <typename T, size_t offset>
void deserialiseMembers(T* instance, const Serialisable::JSON& input, std::index_sequence<>) { }

template <typename T, size_t offset, size_t index, size_t... otherIndexes>
void deserialiseMembers(T* instance, const Serialisable::JSON& input, std::index_sequence<index, otherIndexes...>) {
	constexpr size_t paddedOffset = getPaddedOffset<T, index, offset>();
	const char* name = T::memberName(index);
	if (name) {
		const auto& found = input.object().find(name);
		if (found != input.object().end())
			deserialiseMember(ObjectGetter<T, index>{}, instance, paddedOffset, found->second);
	}
	deserialiseMembers<T, paddedOffset + memberSize(ObjectGetter<T, index>{})>(instance, input, std::index_sequence<otherIndexes...>{});
}

// Same, for mapping ranges where members are saved
template <typename Child>
struct MappingInfo {
	constexpr static int size = MemberCounter<Child, Child*>::get();
	std::array<int, size> elementStarts;
	std::array<int, size> elementSizes;
	std::array<int, size> lastInitialisedBefore1;
	std::array<int, size> lastInitialisedBefore2;
	std::array<const char*, size> names;
	int namedSoFar = 0;
	Child* instance;
};

template <typename T, size_t offset>
void mapLayout(MappingInfo<T>* output, std::index_sequence<>) { }

template <typename T, size_t offset, size_t index, size_t... otherIndexes>
void mapLayout(MappingInfo<T>* output, std::index_sequence<index, otherIndexes...>) {
	constexpr size_t paddedOffset = getPaddedOffset<T, index, offset>();
	output->elementStarts[index] = paddedOffset;
	output->elementSizes[index] = memberSize(ObjectGetter<T, index>{});
	mapLayout<T, paddedOffset + memberSize(ObjectGetter<T, index>{})>(output, std::index_sequence<otherIndexes...>{});	
}

} // namespace

template <typename Child>
class SerialisableQuick {
	enum class InitialisationState : uint8_t {
		UNINITIALISED,
		INITIALISING,
		INITIALISING_AGAIN,
		INITIALISED
	};
	
	static auto& _memberNames() {
		constexpr static auto memberCount = SerialisableQuickInternals::MemberCounter<Child, Child*>::get(); // Can't be a member because it's too early to know it
		static_assert(memberCount < 1000000, "SerialisableQuick failed to detect the number of members");
		static std::array<const char*, memberCount> names;
		return names;
	}
	inline static InitialisationState _initialisationState = InitialisationState::UNINITIALISED;
	inline static SerialisableQuickInternals::MappingInfo<Child>* _mappingInfo = nullptr;
	void* _interface; // Meant to store a vtable pointer rather than a pointer to an object
	constexpr static int8_t garbageNumber1 = 13;
	constexpr static int8_t garbageNumber2 = -13;
	
	template <bool named, typename... Args>
	struct Assigner {
		std::tuple<Args...> args;
		
		template <typename Made, size_t... indexes>
		Made convert(std::index_sequence<indexes...>) const {
			return Made{ std::get<indexes>(args)... };
		}
		
		template <typename Made>
		operator Made() const;
	};

	struct Namer : Assigner<true> {

		template <typename Assigned>
		Assigner<true, Assigned> operator=(const Assigned& assigned) {
			return Assigner<true, Assigned>{std::make_tuple(assigned)};
		}
		Assigner<true, const char*> operator=(const char* assigned) {
			return { std::make_tuple(assigned) };
		}
		
		template <typename... Args>
		Assigner<false, Args...> operator=(const Assigner<false, Args...>& assigned) {
			return assigned;
		}
	};

public:
	static const char* memberName(int index) {
		return _memberNames()[index];
	}

	SerialisableQuick() {
		using namespace SerialisableQuickInternals;
		if (_initialisationState == InitialisationState::UNINITIALISED) {
			// Prepare stuff
			_initialisationState = InitialisationState::INITIALISING;
			MappingInfo<Child> mappingInfo;
			_mappingInfo = &mappingInfo;
			constexpr int memberCount = SerialisableQuickInternals::MemberCounter<Child, Child*>::get();
			mapLayout<Child, sizeof(SerialisableQuick<Child>)>(&mappingInfo, std::make_index_sequence<memberCount>());
			memset(_memberNames().data(), sizeof(decltype(_memberNames())), 0);
			
			// Create the child class in specially prepared garbage
			constexpr int allocatedSize = sizeof(Child) / sizeof(void*) + 1;
			std::array<std::array<void*, allocatedSize>, 2> allocated; // Allocate as void* to have proper padding
			std::array<int8_t*, 2> childBytes;
			struct ChildDestroyer { // We must assure proper destruction of Child, even if an exception is called
				Child* child = nullptr;
				~ChildDestroyer() {
					if (child)
						child->~Child();
				}
			};
			std::array<ChildDestroyer, 2> destroyers;
			auto makeChild = [&] (int index, int garbageNumber) {
				memset(&allocated[index], garbageNumber, sizeof(Child)); // We need to keep track of what is allocated and what is trash
				mappingInfo.instance = reinterpret_cast<Child*>(&allocated[index]);
				destroyers[index].child = new (&allocated[index]) Child;
				childBytes[index] = reinterpret_cast<int8_t*>(destroyers[index].child);
			};
			makeChild(0, garbageNumber1);
			
			// Do it again
			_initialisationState = InitialisationState::INITIALISING_AGAIN;
			mappingInfo.namedSoFar = 0;
			makeChild(1, garbageNumber2);
			
			// Check where garbage was left
			std::array <bool, memberCount> wasInitialised;
			for (unsigned int i = 0; i < memberCount; i++) {
				wasInitialised[i] = false;
				for (int j = 0; j < mappingInfo.elementSizes[i]; j++) {
					if (childBytes[0][mappingInfo.elementStarts[i] + j] != garbageNumber1
								&& childBytes[1][mappingInfo.elementStarts[i] + j] != garbageNumber2) {
						wasInitialised[i] = true;
						break;
					}
				}
			}
			
			for (unsigned int i = 0; i < mappingInfo.namedSoFar; i++) {
				int actualNumber = std::max(mappingInfo.lastInitialisedBefore1[i], mappingInfo.lastInitialisedBefore2[i]) + 1;
				//std::cout << "Actual number is " << actualNumber << std::endl;
				do {
					for (int j = 0; j < mappingInfo.elementSizes[actualNumber]; j++) {
						if (childBytes[0][mappingInfo.elementStarts[actualNumber] + j]
									== childBytes[1][mappingInfo.elementStarts[actualNumber] + j]) {
							goto foundInitialised; // Exit double loop
						}
					}
					actualNumber++;
					if (actualNumber >= memberCount)
						throw std::logic_error("Object mapping failed");
				} while (true);
				foundInitialised:; // Exited double loop
				//std::cout << "Actual index of " << i << " is " << actualNumber << " named " << mappingInfo.names[i] << std::endl;
				_memberNames()[actualNumber] = mappingInfo.names[i];
			}
			
			_mappingInfo = nullptr;
			_initialisationState = InitialisationState::INITIALISED;
			
/*			std::cout << "Mapping:" << std::endl;
			for (auto it : _memberNames()) {
				if (it)
					std::cout << it << std::endl;
				else
					std::cout << "(not serialised)" << std::endl;
			} */
		}
		
		if (_initialisationState == InitialisationState::INITIALISED) {
			// Polymorphism prevents polymorphism, so we need to place an interface at the start of the class
			static_assert(sizeof(void*) == sizeof(ISerialisable), "Unexpected interface size");
			static_assert(offsetof(SerialisableQuick<Child>, _interface) == 0, "Unexpected interface position");
			struct Impl : ISerialisable { // Voldemort type, needs no encapsulation
				virtual JSON toJSON() const {
					return reinterpret_cast<const SerialisableQuick<Child>*>(this)->toJson();
				}
				virtual void fromJSON(const JSON& source) {
					reinterpret_cast<SerialisableQuick<Child>*>(this)->fromJson(source);
				}
			};
			new (&_interface) Impl;
		}
	}
	
	Serialisable::JSON toJson() const {
		Serialisable::JSON made = Serialisable::JSON::ObjectType();
		SerialisableQuickInternals::serialiseMembers<Child, sizeof(SerialisableQuick<Child>)>(static_cast<const Child*>(this), made,
				std::make_index_sequence<SerialisableQuickInternals::MemberCounter<Child, Child*>::get()>());
		return made;
	}
	void fromJson(const Serialisable::JSON& input) {
		SerialisableQuickInternals::deserialiseMembers<Child, sizeof(SerialisableQuick<Child>)>(static_cast<Child*>(this), input,
				std::make_index_sequence<SerialisableQuickInternals::MemberCounter<Child, Child*>::get()>());
	}
	
	const ISerialisable* interface() const {
		return reinterpret_cast<const ISerialisable*>(&_interface);
	}
	ISerialisable* interface() {
		return reinterpret_cast<ISerialisable*>(&_interface);
	}
	operator const ISerialisable&() const {
		return *interface();
	}
	operator ISerialisable&() {
		return *interface();
	}
	
	SerialisableQuick<Child>::Namer key(const char* name) {
		if (!_mappingInfo)
			return {};
		_mappingInfo->names[_mappingInfo->namedSoFar] = name; // namedSoFar will be incremented in the conversion operator of the returned value
		return {};
	}
	
	template <typename... Args>
	SerialisableQuick<Child>::Assigner<false, Args...> init(Args&&... args) {
		return { std::make_tuple(args...) };
	}
};

namespace SerialisableInternals {
template <typename Serialised>
struct Serialiser<Serialised, std::enable_if_t<std::is_base_of_v<SerialisableQuick<Serialised>, Serialised>>>
{
	constexpr static bool valid = true;
	
	static Serialisable::JSON serialise(const Serialised& value) {
		return value.toJson();
	}

	static void deserialise(Serialised& result, Serialisable::JSON value) {
		result.fromJson(value);
	}
};
}

template <typename Child>
template <bool named, typename... Args>
template <typename Made>
SerialisableQuick<Child>::Assigner<named, Args...>::operator Made() const {
	if constexpr(named) {
		using State = SerialisableQuick<Child>::InitialisationState;
		State state = SerialisableQuick<Child>::_initialisationState;
		if (state == State::INITIALISING || state == State::INITIALISING_AGAIN) {
			auto mappingInfo = SerialisableQuick<Child>::_mappingInfo;
			int8_t garbageNumber = (state == State::INITIALISING) ? SerialisableQuick<Child>::garbageNumber1 : SerialisableQuick<Child>::garbageNumber2;
			
			int lastUninitialised = sizeof(Child) - 1;
			//std::cout << "Byte check " << int(reinterpret_cast<int8_t*>(mappingInfo->instance)[lastUninitialised]) << " with " << int(state) << std::endl;
			
			// Valgrind will complain
			while (reinterpret_cast<int8_t*>(mappingInfo->instance)[lastUninitialised] == garbageNumber && lastUninitialised)
				lastUninitialised--;
			
			int lastInitialised = -1;
			for (int i = 0; i < mappingInfo->size; i++) {
				if (mappingInfo->elementStarts[i] > lastUninitialised - mappingInfo->elementSizes[i]) {
					lastInitialised = i;
					break;
				}
			}
			//std::cout << "Snapshot last uninit " << lastUninitialised << " last init " << lastInitialised << std::endl;
			if (lastInitialised == -1)
				throw std::logic_error("Failed to map class initialisation");
			
			if (state == State::INITIALISING)
				mappingInfo->lastInitialisedBefore1[mappingInfo->namedSoFar] = lastInitialised;
			else
				mappingInfo->lastInitialisedBefore2[mappingInfo->namedSoFar] = lastInitialised;
			mappingInfo->namedSoFar++;
		}
	}
	return convert<Made>(std::make_index_sequence<sizeof...(Args)>());
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
