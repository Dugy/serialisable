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

	int addElementToOffset(unsigned int size) {
		if (size == 1)
			return _setupData->currentSize++;
		else if (size == 2) {
			if (_setupData->currentSize % 2) _setupData->currentSize++;
			int retval = _setupData->currentSize;
			_setupData->currentSize += 2;
			return retval;
		}
		else if (size == 4 || (sizeof(void*) == 4 && size >= 4)) {
			if (_setupData->currentSize % 4) _setupData->currentSize += 4 - _setupData->currentSize % 4;
			int retval = _setupData->currentSize;
			_setupData->currentSize += 4;
			return retval;
		}
		else if (size % 8 == 0) {
			if (_setupData->currentSize % 8) _setupData->currentSize += 8 - _setupData->currentSize % 8;
			int retval = _setupData->currentSize;
			_setupData->currentSize += size;
			return retval;
		}
		throw(std::logic_error("Invalid class serialised, not sure how it got into runtime"));
	}

	template <typename T>
	void addSerialiser(const std::string& name) {
		SerialisableBriefListing& instance = SerialisableBriefListing::getInstance();
		std::string& serialiserString = _setupData->serialiserString;
		std::function<void(SerialisableBrief*)>* previousSerialiser = instance.getSerialiserFor(serialiserString);

		serialiserString.append(name);
		const auto typeNum = typeid(T).hash_code();
		for (unsigned int i = 0; i < sizeof(typeNum); i++) {
			serialiserString.push_back(reinterpret_cast<const unsigned char*>(&typeNum)[i]);
		}
		int position = addElementToOffset(sizeof(T));
		if (!instance.serialiserDone(serialiserString)) {
			instance.addSerialiser(serialiserString, [previousSerialiser, position, name] (SerialisableBrief* self) {
				(*previousSerialiser)(self);
				T& reference = *reinterpret_cast<T*>(reinterpret_cast<uint64_t>(self) + position);
				self->synch(name, reference);
			});
		}
	}

	template <typename T>
	void addNonserialiser() {
		SerialisableBriefListing& instance = SerialisableBriefListing::getInstance();
		std::string& serialiserString = _setupData->serialiserString;
		std::function<void(SerialisableBrief*)>* previousSerialiser = instance.getSerialiserFor(serialiserString);

		const auto typeSize = sizeof(T);
		for (unsigned int i = 0; i < 3; i++) {
			serialiserString.push_back(reinterpret_cast<const unsigned char*>(&typeSize)[i]);
		}
		addElementToOffset(sizeof(T));
		if (!instance.serialiserDone(serialiserString))
			instance.addSerialiser(serialiserString, *previousSerialiser);
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
		template <typename T>
		operator T() {
			if (_serialised) _parent->addSerialiser<T>(_name);
			else _parent->addNonserialiser<T>();
			return makeType<T>(typename NumberGenerator<sizeof...(Args)>::Type());
		}
		friend class SerialisableBrief;
	};

	class SubSerialiser {
		SerialisableBrief* _parent;
		std::string _name;
		bool _serialised;
		SubSerialiser(SerialisableBrief* parent, const std::string& name, bool serialised) :
			_parent(parent), _name(name), _serialised(serialised) {
		}
	public:
		template <typename... Args>
		SubSerialiserInitialised<Args...> init(Args... args) {
			return SubSerialiserInitialised<Args...>(_parent, _name, _serialised, args...);
		}
		template <typename T>
		operator T() {
			if (_serialised) _parent->addSerialiser<T>(_name);
			else _parent->addNonserialiser<T>();
			return T();
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
