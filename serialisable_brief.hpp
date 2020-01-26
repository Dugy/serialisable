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
class SerialisableBrief;

class SerialisableBriefListing {
	std::unordered_map<std::string, std::function<void(SerialisableBrief*)>> _entries;
	SerialisableBriefListing() {
		_entries[""] = [] (SerialisableBrief*) {};
	}
	static SerialisableBriefListing& getInstance() {
		static SerialisableBriefListing instance;
		return instance;
	}
	std::function<void(SerialisableBrief*)>* getSerialiserFor(const std::string& identifier) {
		return &(_entries.find(identifier)->second);
	}
	bool serialiserDone(const std::string& identifier) {
		return (_entries.find(identifier) != _entries.end());
	}
	void addSerialiser(const std::string& identifier, const std::function<void(SerialisableBrief*)>& serialiser) {
		_entries[identifier] = serialiser;
	}
	friend class SerialisableBrief;
};

class SerialisableBrief : public Serialisable {
	struct SerialisationSetupData {
		std::string serialiserString;
		int currentSize = sizeof(SerialisableBrief);
	};
	std::unique_ptr<SerialisationSetupData> _setupData = std::make_unique<SerialisationSetupData>();
	std::function<void(SerialisableBrief*)>* _serialiser = nullptr;

	template<typename T>
	int addElementToOffset(uint64_t ellisionOffset) {
		auto check = [ellisionOffset] (int signedOffset) {
			uint64_t offset = uint64_t(signedOffset);
			if (ellisionOffset - offset < 0xffff && ellisionOffset > offset)
				throw(std::logic_error("Offset misprediction detected, very probably a call to skip() was forgotten"));
			return signedOffset;
		};
		if (sizeof(T) == 1)
			return check(_setupData->currentSize++);
		else if (sizeof(T) == 2) {
			if (_setupData->currentSize % 2) _setupData->currentSize++;
			int retval = _setupData->currentSize;
			_setupData->currentSize += 2;
			return check(retval);
		}
		else if (sizeof(T) == 4 || (sizeof(void*) == 4 && sizeof(T) >= 4 && !(std::is_fundamental<T>::value && sizeof(T) == 8))) {
			if (_setupData->currentSize % 4) _setupData->currentSize += 4 - _setupData->currentSize % 4;
			int retval = _setupData->currentSize;
			_setupData->currentSize += sizeof(T);
			return check(retval);
		}
		else if (sizeof(T) % 8 == 0) {
			if (_setupData->currentSize % 8) _setupData->currentSize += 8 - _setupData->currentSize % 8;
			int retval = _setupData->currentSize;
			_setupData->currentSize += sizeof(T);
			return check(retval);
		}
		throw(std::logic_error("Invalid class serialised, not sure how it got into runtime"));
	}

	void finishUp() const {
		if (_setupData) {
			const_cast<SerialisableBrief*>(this)->_serialiser = SerialisableBriefListing::getInstance().getSerialiserFor(_setupData->serialiserString);
			const_cast<SerialisableBrief*>(this)->_setupData.release();
		}
	}

protected:
	template <typename... Args>
	class SubSerialiserInitialised {
		SerialisableBrief* _parent;
		std::string _name;
		bool _serialised;
		std::tuple<Args...> _args;

		SubSerialiserInitialised(SerialisableBrief* parent, const std::string& name, bool serialised, Args... args) :
			_parent(parent), _name(name), _serialised(serialised), _args(std::make_tuple(args...)) {
		}

		template<int ...> struct NumberSequence {};

		template <int N, int... Numbers>
		struct NumberGenerator : NumberGenerator<N - 1, N - 1, Numbers...> { };

		template <int... Numbers>
		struct NumberGenerator<0, Numbers...> {
			typedef NumberSequence<Numbers...> Type;
		};

		template <typename T, int... Enumeration>
		T makeType(NumberSequence<Enumeration...>) {
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
			SerialisableBriefListing& instance = SerialisableBriefListing::getInstance();
			std::string& serialiserString = _parent->_setupData->serialiserString;
			std::function<void(SerialisableBrief*)>* previousSerialiser = instance.getSerialiserFor(serialiserString);
			T returned = makeType<T>(typename NumberGenerator<sizeof...(Args)>::Type());

			if (_serialised) {
				serialiserString.append(_name);
				const auto typeNum = typeid(T).hash_code();
				for (unsigned int i = 0; i < sizeof(typeNum); i++) {
					serialiserString.push_back(reinterpret_cast<const unsigned char*>(&typeNum)[i]);
				}
				int position = _parent->addElementToOffset<T>(uint64_t(&returned) - uint64_t(_parent));
				if (!instance.serialiserDone(serialiserString)) {
					instance.addSerialiser(serialiserString, [previousSerialiser, position, name = _name] (SerialisableBrief* self) {
						(*previousSerialiser)(self);
						T& reference = *reinterpret_cast<T*>(reinterpret_cast<uint64_t>(self) + position);
						self->synch(name, reference);
					});
				}
			}
			else {
				short int typeSize = sizeof(T);
				for (unsigned int i = 0; i < 2; i++) {
					serialiserString.push_back(reinterpret_cast<const unsigned char*>(&typeSize)[i]);
				}
				_parent->addElementToOffset<T>(uint64_t(&returned) - uint64_t(_parent));
				if (!instance.serialiserDone(serialiserString))
					instance.addSerialiser(serialiserString, *previousSerialiser);
			}
			return returned;
		}
		friend class SerialisableBrief;
		friend class SubSerialiser;
	};

	class SubSerialiser : public SubSerialiserInitialised<> {
		SubSerialiser(SerialisableBrief* parent, const std::string& name, bool serialised) :
			SubSerialiserInitialised<>(parent, name, serialised) {
		}
	public:
		template <typename... Args>
		SubSerialiserInitialised<Args...> init(Args... args) {
			return SubSerialiserInitialised<Args...>(_parent, _name, _serialised, args...);
		}

		friend class SerialisableBrief;
	};

	SerialisableBrief(const SerialisableBrief& other)
		: _setupData(nullptr) {
		other.finishUp();
		_serialiser = other._serialiser;
	}
	SerialisableBrief() = default;

	SerialisableBrief(SerialisableBrief&& other)
		: _setupData(nullptr) {
		other.finishUp();
		_serialiser = other._serialiser;
	}

	virtual ~SerialisableBrief() = default;

	SubSerialiser key(const std::string& name) {
		return SubSerialiser(this, name, true);
	}

	template <typename T, typename... Ts>
	SubSerialiserInitialised<T, Ts...> key(const std::string& name, T firstArg, Ts... otherArgs) {
		return SubSerialiserInitialised<T, Ts...>(this, name, true, firstArg, otherArgs...);
	}

	SubSerialiser skip() {
		return SubSerialiser(this, "", false);
	}

	template <typename T, typename... Ts>
	SubSerialiserInitialised<T, Ts...> skip(T firstArg, Ts... otherArgs) {
		return SubSerialiserInitialised<T, Ts...>(this, "", false, firstArg, otherArgs...);
	}

	void serialisation() final override {
		finishUp();
		(*_serialiser)(this);
	}
	friend class Serialisable;
};


#endif  //SERIALISABLE_BRIEF_BY_DUGI_HPP
