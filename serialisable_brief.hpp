/*
* \brief A performance inefficient and code efficient version of Serialisable
*
* See more at https://github.com/Dugy/serialisable
*/

#ifndef SERIALISABLE_BRIEF_BY_DUGI_HPP
#define SERIALISABLE_BRIEF_BY_DUGI_HPP
#include <typeinfo>
#include <tuple>
#include <functional>
#include "serialisable.hpp"

template <typename Child>
class SerialisableBrief : public Serialisable {
	struct MappingElement {
		int size;
		std::array<int, 2> lastInitialisedBefore;
		const char* name;
	};
	struct SerialisationSetupData {
		std::vector<MappingElement> elements;
		const Child* instance;
		int index;
		int parentOffset;
	};
	static SerialisationSetupData*& setupData() {
		static SerialisationSetupData* instance = nullptr;
		return instance;
	}
	
	enum class InitialisationState : uint8_t {
		UNINITIALISED,
		INITIALISING,
		INITIALISING_AGAIN,
		INITIALISED,
		VERIFIED
	};
	struct SubSerialiser {
		virtual void subSynch(SerialisableBrief<Child>* base) = 0;
		int offset;
		virtual ~SubSerialiser() = default;
	};
	struct SerialisationInfo {
		InitialisationState state;
		std::vector<std::unique_ptr<SubSerialiser>> subSerialisers;
		int parentOffset;
	};
	static SerialisationInfo& serialisationInfo() {
		static SerialisationInfo instance;
		return instance;
	}

	constexpr static int8_t garbageNumber1 = 13;
	constexpr static int8_t garbageNumber2 = -13;
	
	void addKey(const char* name) {
		if (SerialisableBrief::serialisationInfo().state == InitialisationState::INITIALISING) {
			setupData()->elements.emplace_back();
			setupData()->elements.back().name = name;
		}
	}

protected:
	template <typename... Args>
	class InitialiserInitialiser {
		std::tuple<Args...> _args;

		InitialiserInitialiser(Args... args) :
			_args(std::make_tuple(args...)) {
		}
		
		template <typename T, size_t... Enumeration>
		T makeType(std::index_sequence<Enumeration...>) {
			return T(std::get<Enumeration>(_args)...);
		}
	public:
#if _MSC_VER && !__INTEL_COMPILER
		template <class T>
		struct isInitializerList : std::false_type {};

		template <class T>
		struct isInitializerList<std::initializer_list<T>> : std::true_type {};

		template<typename T, std::enable_if_t<(std::is_class_v<T> && !isInitializerList<T>::value)
			|| std::is_arithmetic_v<T> || std::is_floating_point_v<T> || std::is_enum_v<T>>* = nullptr>
#else
		template <typename T>
#endif
		operator T() {
			if (SerialisableBrief::setupData()) {
				SerialisationSetupData& info = *SerialisableBrief::setupData();
				info.elements[info.index].size = sizeof(T);
				SerialisableBrief::InitialisationState state = SerialisableBrief::serialisationInfo().state;
				int8_t garbageNumber = state == InitialisationState::INITIALISING ? SerialisableBrief::garbageNumber1 : SerialisableBrief::garbageNumber2;

				int lastUninitialised = sizeof(Child) - 1;

				// Valgrind will complain
				while (lastUninitialised && reinterpret_cast<const int8_t*>(info.instance)[lastUninitialised - 1] == garbageNumber)
					lastUninitialised--;

				info.elements[info.index].lastInitialisedBefore[state == InitialisationState::INITIALISING_AGAIN] = lastUninitialised;

				if (state == InitialisationState::INITIALISING) {
					struct : SubSerialiser {
						void subSynch(SerialisableBrief<Child>* base) override {
							T* member = reinterpret_cast<T*>(reinterpret_cast<uint64_t>(base) + SubSerialiser::offset
									+ base->serialisationInfo().parentOffset);
							base->synch(name, *member);
						}
						const char* name; // The offset will be set later
					} subSerialiser;
					subSerialiser.name = info.elements[info.index].name;
					serialisationInfo().subSerialisers.emplace_back(new decltype(subSerialiser)(subSerialiser));
				}

				info.index++;
			}
			return makeType<T>(std::make_index_sequence<sizeof...(Args)>());
		}
		friend class SerialisableBrief;
		friend class Initialiser;
	};

	class Initialiser : public InitialiserInitialiser<> {
		Initialiser() :
			InitialiserInitialiser<>() {
		}
	public:
		template <typename... Args>
		InitialiserInitialiser<Args...> init(Args... args) {
			return InitialiserInitialiser<Args...>(args...);
		}
		template <typename Arg>
		InitialiserInitialiser<Arg> operator=(Arg arg) {
			return InitialiserInitialiser<Arg>(arg);
		}

		friend class SerialisableBrief;
	};
	
	template <typename... Args>
	SerialisableBrief(Args... args) {
		if (uint8_t(serialisationInfo().state) < uint8_t(InitialisationState::INITIALISED)) {
			if (serialisationInfo().state == InitialisationState::UNINITIALISED) {
				// Prepare stuff
				serialisationInfo().state = InitialisationState::INITIALISING;
				SerialisationSetupData info;
				setupData() = &info;

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
				auto makeChild = [&] (int index) {
					info.instance = reinterpret_cast<Child*>(&allocated[index]);
					info.index = 0;
					destroyers[index].child = new (&allocated[index]) Child(args...);
					childBytes[index] = reinterpret_cast<int8_t*>(destroyers[index].child);
				};
				makeChild(0);
				serialisationInfo().parentOffset = reinterpret_cast<uint64_t>(static_cast<const SerialisableBrief<Child>*>(info.instance)) -
						reinterpret_cast<uint64_t>(info.instance);
				// Do it again
				serialisationInfo().state = InitialisationState::INITIALISING_AGAIN;
				makeChild(1);

				// Check where garbage was left
				for (unsigned int i = 0; i < info.elements.size(); i++) {
					int start = std::max(info.elements[i].lastInitialisedBefore[0], info.elements[i].lastInitialisedBefore[1]);
					while (childBytes[0][start] == garbageNumber1 && childBytes[1][start] == garbageNumber2) {
						if (start > int(sizeof(Child))) throw std::logic_error("Reflection failed");
						start++;
					}
					serialisationInfo().subSerialisers[i]->offset = start;
				}

				setupData() = nullptr;
				serialisationInfo().state = InitialisationState::INITIALISED;
			} else {
				// We need to keep track of what is allocated and what is trash
				void* start = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(this) + sizeof(SerialisableBrief<Child>));
				uint8_t garbageNumber = (serialisationInfo().state == InitialisationState::INITIALISING) ? garbageNumber1 : garbageNumber2;
				memset(start, garbageNumber, sizeof(Child) - sizeof(SerialisableBrief<Child>));
			}
		}
	}
	SerialisableBrief(const SerialisableBrief& other) = default;
	SerialisableBrief(SerialisableBrief&& other) = default;

	virtual ~SerialisableBrief() = default;

	Initialiser key(const char* name) {
		addKey(name);
		return Initialiser();
	}

	template <typename T, typename... Ts>
	InitialiserInitialiser<T, Ts...> key(const char* name, T firstArg, Ts... otherArgs) {
		addKey(name);
		return InitialiserInitialiser<T, Ts...>(firstArg, otherArgs...);
	}

	void serialisation() final override {
		if (serialisationInfo().state != InitialisationState::VERIFIED) {
			if (serialisationInfo().state != InitialisationState::INITIALISED)
				throw std::logic_error("Cannot serialise/deserialise the class yet");
		
			if (!dynamic_cast<Child*>(this))
				throw std::logic_error("Wrong CRTP class, must inherit from SerialisableBrief templated to the derived type");
			serialisationInfo().state = InitialisationState::VERIFIED;
		}
		for (auto& it : serialisationInfo().subSerialisers) {
			it->subSynch(this);
		}
	}
	friend class Serialisable;
};


#endif //SERIALISABLE_BRIEF_BY_DUGI_HPP
