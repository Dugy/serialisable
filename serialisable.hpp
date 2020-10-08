/*
* \brief Class convenient and duplication-free serialisation and deserialisation of its descendants into JSON, with one function to handle both loading and saving
*
* See more at https://github.com/Dugy/serialisable
*/

#ifndef SERIALISABLE_BY_DUGI_HPP
#define SERIALISABLE_BY_DUGI_HPP

#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <memory>
#include <array>
#include <fstream>
#include <exception>
#include <sstream>
#include <type_traits>
#include <cmath>
#include <algorithm>
#include <cstring>
#if __cplusplus > 201402L
#include <optional>
#endif

class Serialisable;

namespace SerialisableInternals {

template <typename Serialised, typename SFINAE>
struct Serialiser {
	constexpr static bool valid = false;
};

template <typename Returned, typename ArgType>
auto getArgType(Returned (*)(ArgType)) { return *reinterpret_cast<std::decay_t<ArgType>*>(1); }

template <typename Format, typename SFINAE>
struct DiskAccessor {
	template <typename Internal>
	static void save(const std::string& fileName, const Internal& source) {
		auto serialised = Format::serialise(source);
		std::ofstream stream(fileName, std::ios::binary);
		for (auto& it : serialised)
			stream << it;
	}

	template <typename Internal>
	static Internal load(const std::string& fileName) {
		std::decay_t<decltype(getArgType(&Format::deserialise))> making;
		Internal made;
		std::ifstream stream(fileName, std::ios::binary);
		stream >> std::noskipws;
		while (stream.good()) {
			std::decay_t<decltype(making.front())> reading;
			stream >> reading;
			making.push_back(reading);
		}
		return Format::deserialise(making);
	}
};
} // namespace

struct ISerialisable {

	struct SerialisationError : std::runtime_error {
		using std::runtime_error::runtime_error;
	};

	enum class JSONtype : uint8_t {
		NIL, // Can't be 'NULL', it's a macro in C
		STRING,
		DOUBLE,
		INTEGER,
		BOOL,
		ARRAY,
		OBJECT
	};

	union JSON {
	public:
		struct JSONexception : std::runtime_error {
			using std::runtime_error::runtime_error;
		};

		union String {
		private:
			constexpr static int BREAKPOINT = 8;
			using RefcountType = int32_t;
			using CharType = int8_t;
			constexpr static int INTERNAL_OFFSET = sizeof(RefcountType);

			uint64_t _contents;
			CharType* longContents;
			std::array<CharType, sizeof(uint64_t)> shortContents;

			static int offsetOfCharacter(int index) {
				return 56 - (index << 3);
			}
			static uint64_t maskOfCharacter(int index) {
				return uint64_t(0xff) << offsetOfCharacter(index);
			}
			bool isLocal() const {
				return (_contents == 0 || ((_contents & 0xffull << 56)));
			}
			template <typename T>
			T* memory(int offset) const {
				if (!(_contents & 0x0000800000000000))
					return reinterpret_cast<T*>(_contents + uint64_t(offset));
				else
					return reinterpret_cast<T*>((_contents | 0xffff000000000000) + uint64_t(offset));
			}
			RefcountType& refcount() {
				return *memory<RefcountType>(-INTERNAL_OFFSET);
			}
			void unref() {
				if (isLocal())
					return;
				refcount()--;
				if (!refcount())
					delete[] memory<CharType>(-INTERNAL_OFFSET);
			}
			void makeHeap(size_t size, const char* data) {
				CharType* remote = new CharType[INTERNAL_OFFSET + size + 1];
				*reinterpret_cast<RefcountType*>(remote) = 1;
				memcpy(INTERNAL_OFFSET + remote, data, size + 1);
				_contents = reinterpret_cast<uint64_t>(remote + INTERNAL_OFFSET) & 0x0000ffffffffffff;
			}
		public:
			String() : _contents(0) { }
			String(const String& from) {
				_contents = from._contents;
				if (!from.isLocal()) {
					refcount()++;
				}
			}
			String(String&& from) : _contents(from._contents) {
				from._contents = 0;
			}
			String(const std::string& from) {
				if (from.length() <= BREAKPOINT) {
					_contents = 0;
					for (int i = 0; i <int(from.length()); i++)
						_contents |= uint64_t(from[size_t(i)]) << offsetOfCharacter(i);
				} else {
					makeHeap(from.length(), from.data());
				}
			}
			String(const char* from) {
				int length = int(strlen(from));
				if (length <= BREAKPOINT) {
					_contents = 0;
					for (int i = 0; i < length; i++)
						_contents |= uint64_t(from[i]) << offsetOfCharacter(i);
				} else {
					makeHeap(size_t(length), from);
				}
			}
			~String() {
				unref();
			}
			operator std::string() const {
				std::string result;
				if (isLocal()) {
					for (int i = 56; i >= 0; i -= 8) {
						CharType at = CharType(_contents >> i);
						if (!at) break;
						result.push_back(at);
					}
				} else {
					for (CharType* data = memory<CharType>(0); *data; data++)
						result.push_back(*data);
				}
				return result;
			}
			String& operator=(const String& from) {
				unref();
				_contents = from._contents;
				if (!isLocal()) {
					refcount()++;
				}
				return *this;
			}
			String& operator=(String&& from) {
				unref();
				_contents = from._contents;
				from._contents = 0;
				return *this;
			}

			bool operator==(const String& other) const {
				if (_contents == other._contents) return true;
				if (!isLocal() && !other.isLocal()) {
					return !strcmp(memory<char>(0), other.memory<char>(0));
				}
				return false;
			}
			bool operator==(const char* other) const {
				if (isLocal()) {
					for (int i = 0; i < int(sizeof(uint64_t)); i++) {
						if (CharType((_contents & maskOfCharacter(i)) >> (i << 3)) != other[i])
							return false;
					}
					return true;
				} else {
					return !strcmp(memory<char>(0), other);
				}
			}
			bool operator==(const std::string& other) const {
				return operator==(other.c_str());
			}

			size_t hash() const {
				if (isLocal()) {
					std::hash<uint64_t> hasher;
					return hasher(_contents);
				} else {
					// FNV hash
					char* data = memory<char>(0);
					const static unsigned int startValue = 2166136261 ^ time(nullptr);
					unsigned int value = startValue;
					for (int i = 0; data[i]; i++)
						value = (value * 16777619) ^ data[i];
					return value;
				}
			}

			friend std::ostream& operator<<(std::ostream& stream , const String& str);
		};

		struct StringHasher {
			std::size_t operator()(const ISerialisable::JSON::String& key) const
			{
				return key.hash();
			}
		};


		using ObjectType = std::unordered_map<String, JSON, StringHasher>;
		using ArrayType = std::vector<JSON>;

	private:
		static constexpr uint64_t TYPE_MASK = 0xffff000000000000;
		static constexpr uint64_t BOOLEAN_MASK = 0x1;
		static constexpr uint64_t INVALID_NUMBER_IDENTIFIER = 0xfff8000000000000;
		static constexpr uint64_t NAN_VALUE = 0x7fffffffffffffff;
		using RefcountType = int;
		static constexpr uint64_t POINTER_MASK = 0x0000ffffffffffff;
		static constexpr int STRING_BREAKPOINT = 6;
		struct InternalType {
			enum Type : uint64_t {
				NIL = 0xfff8000000000000, // NULL is a C macro already
				BOOLEAN = 0xfff9000000000000,
				SHORT_STRING = 0xfffa000000000000,
				LONG_STRING = 0xfffb000000000000,
				OBJECT = 0xfffc000000000000,
				ARRAY = 0xfffd000000000000
			};
		};

		double _number; // Some NaNs can mean it's a different type
		uint64_t _contents;
		std::array<int8_t, sizeof(uint64_t)> shortString;

		inline bool usesHeap() const {
			uint64_t prefix = _contents & TYPE_MASK;
			return (prefix == InternalType::LONG_STRING || prefix == InternalType::OBJECT || prefix == InternalType::ARRAY);
		}
		inline uint64_t internalAddress() const {
			uint64_t suffix = _contents & POINTER_MASK;
			if (suffix & 0x0000800000000000)
				suffix |= 0xffff000000000000;
			return suffix;
		}
		inline RefcountType& refcount() {
			uint64_t suffix = internalAddress();
			return *reinterpret_cast<RefcountType*>(suffix - sizeof(RefcountType));
		}
		template<typename T>
		T* getHeap() {
			return reinterpret_cast<T*>(internalAddress());
		}
		template<typename T>
		T const* getHeap() const {
			return reinterpret_cast<T const*>(internalAddress());
		}
		template<typename T>
		T* allocate(int size) {
			uint8_t* allocated = new uint8_t[sizeof(RefcountType) + unsigned(size)];
			*reinterpret_cast<RefcountType*>(allocated) = 1;
			_contents = reinterpret_cast<uint64_t>(allocated + sizeof(RefcountType));
			return reinterpret_cast<T*>(_contents);
		}
		inline void cleanup() {
			if (!usesHeap()) return;
			RefcountType& refs = refcount();
			refs--;
			if (!refs) {
				uint64_t prefix = _contents & TYPE_MASK;
				uint64_t suffix = internalAddress();
				if (prefix == InternalType::OBJECT)
					reinterpret_cast<ObjectType*>(suffix)->~unordered_map();
				else if (prefix == InternalType::ARRAY)
					reinterpret_cast<ArrayType*>(suffix)->~vector();
				delete[] reinterpret_cast<char*>(suffix - sizeof(RefcountType));
			}
		}

	public:
		enum class Type {
			NIL,
			BOOL,
			NUMBER,
			STRING,
			OBJECT,
			ARRAY
		};
		Type type() const {
			switch (_contents & TYPE_MASK) {
			case InternalType::NIL:
				return Type::NIL;
			case InternalType::BOOLEAN:
				return Type::BOOL;
			case InternalType::SHORT_STRING:
			case InternalType::LONG_STRING:
				return Type::STRING;
			case InternalType::OBJECT:
				return Type::OBJECT;
			case InternalType::ARRAY:
				return Type::ARRAY;
			default:
				return Type::NUMBER;
			}
		}

		JSON() : _contents(InternalType::NIL) {}
		JSON(const JSON& other) {
			_contents = other._contents;
			if (usesHeap())
				refcount()++;
		}
		JSON(JSON&& other) {
			_contents = other._contents;
			other._contents = InternalType::NIL;
		}
		~JSON() {
			cleanup();
		}

		JSON(bool value) {
			setBoolean(value);
		}
		inline void setBoolean(bool value) {
			_contents = InternalType::BOOLEAN | value;
		}
		bool isBool() const {
			return ((_contents & TYPE_MASK) == InternalType::BOOLEAN);
		}
		bool boolean() const {
			if ((_contents & TYPE_MASK) != InternalType::BOOLEAN)
				throw JSONexception("Value is not really boolean");
			return _contents & BOOLEAN_MASK;
		}
		inline void boolean(bool value) {
			cleanup();
			setBoolean(value);
		}
		bool operator=(bool& value) { // Reference prevents implicit conversions from int
			boolean(value);
			return value;
		}
		operator bool() const {
			if ((_contents & TYPE_MASK) == InternalType::BOOLEAN)
				return _contents & BOOLEAN_MASK;
			if ((_contents & TYPE_MASK) == InternalType::NIL)
				return false;
			return true;
		}

		template <typename T, std::enable_if_t<std::is_arithmetic<T>::value && !std::is_same<std::decay_t<T>, bool>::value>* = nullptr>
		JSON(T value) { // Anything numeric except bool
			setNumber(value);
		}
		inline void setNumber(double value) {
			if (value == value)
				_number = value;
			else
				_contents = NAN_VALUE;
		}
		bool isNumber() const {
			return type() == Type::NUMBER;
		}
		double number() const {
			if ((_contents & INVALID_NUMBER_IDENTIFIER) != INVALID_NUMBER_IDENTIFIER) // Not NaN
				return _number;
			if (type() != Type::NUMBER)
				throw JSONexception("Value is not really a number");
			return _number;
		}
		void number(double value) {
			cleanup();
			setNumber(value);
		}
		double operator=(double value) {
			setNumber(value);
			return value;
		}
		operator double() const {
			return number();
		}

		JSON(std::nullptr_t) {
			setNull();
		}
		inline void setNull() {
			_contents = InternalType::NIL;
		}
		bool isNull() const {
			return ((_contents & TYPE_MASK) == InternalType::NIL);
		}
		void null() {
			cleanup();
			setNull();
		}
		std::nullptr_t operator=(std::nullptr_t) {
			null();
			return nullptr;
		}

		JSON(const std::string& value) {
			setString(value);
		}
		inline void setString(const std::string& value) {
			if (value.size() <= STRING_BREAKPOINT) {
				_contents = InternalType::SHORT_STRING;
				for (unsigned int i = 0; i < value.size(); i++) {
					_contents |= uint64_t(value[i]) << (i << 3);
				}
				// Trailing zeroes remain from the initial assignment
			} else {
				char* allocated = allocate<char>(int((value.size() + 1) * sizeof(char)));
				strcpy(allocated, value.c_str());
				_contents = InternalType::LONG_STRING | (reinterpret_cast<uint64_t>(allocated) & POINTER_MASK);
			}
		}
		JSON(const char* value) {
			setString(value);
		}
		bool isString() const {
			return ((_contents & TYPE_MASK) == InternalType::SHORT_STRING || (_contents & TYPE_MASK) == InternalType::LONG_STRING);
		}
		std::string string() const {
			if ((_contents & TYPE_MASK) == InternalType::SHORT_STRING) {
				std::string result;
				for (int i = 0; i < STRING_BREAKPOINT; i++) {
					char letter = char(_contents >> (i << 3));
					if (!letter)
						break;
					result.push_back(letter);
				}
				return result;
			} else if ((_contents & TYPE_MASK) == InternalType::LONG_STRING) {
				return getHeap<char>();
			} else
				throw JSONexception("Value is not really a string");
		}
		void string(const std::string& value) {
			cleanup();
			setString(value);
		}
		const std::string& operator=(const std::string& value) {
			string(value);
			return value;
		}
		const char* operator=(const char* value) {
			string(value);
			return value;
		}
		operator std::string() {
			return string();
		}

		JSON(const ObjectType& value) {
			setObject(value);
		}
		inline ObjectType& setObject() {
			ObjectType* allocated = allocate<ObjectType>(sizeof(ObjectType));
			_contents = InternalType::OBJECT | (reinterpret_cast<uint64_t>(allocated) & POINTER_MASK);
			return *new(allocated) ObjectType(0);
		}
		inline void setObject(const ObjectType& value) {
			ObjectType* allocated = allocate<ObjectType>(sizeof(ObjectType));
			new(allocated) ObjectType(value);
			_contents = InternalType::OBJECT | (reinterpret_cast<uint64_t>(allocated) & POINTER_MASK);
		}
		bool isObject() const {
			return ((_contents & TYPE_MASK) == InternalType::OBJECT);
		}
		ObjectType& object() {
			if ((_contents & TYPE_MASK) != InternalType::OBJECT)
				throw JSONexception("Value is not really an object");
			return *getHeap<ObjectType>();
		}
		const ObjectType& object() const {
			if ((_contents & TYPE_MASK) != InternalType::OBJECT)
				throw JSONexception("Value is not really an object");
			return *getHeap<ObjectType>();
		}
		void object(const ObjectType& value) {
			cleanup();
			setObject(value);
		}
		const ObjectType& operator=(const ObjectType& value) {
			setObject(value);
			return value;
		}
		operator ObjectType() {
			return object();
		}

		JSON(const ArrayType& value) {
			setArray(value);
		}
		inline ArrayType& setArray() {
			ArrayType* allocated = allocate<ArrayType>(sizeof(ArrayType));
			_contents = InternalType::ARRAY | (reinterpret_cast<uint64_t>(allocated) & POINTER_MASK);
			return *new(allocated) ArrayType();
		}
		inline void setArray(const ArrayType& value) {
			ArrayType* allocated = allocate<ArrayType>(sizeof(ArrayType));
			new(allocated) ArrayType(value);
			_contents = InternalType::ARRAY | (reinterpret_cast<uint64_t>(allocated) & POINTER_MASK);
		}
		bool isArray() const {
			return ((_contents & TYPE_MASK) == InternalType::ARRAY);
		}
		ArrayType& array() {
			if ((_contents & TYPE_MASK) != InternalType::ARRAY)
				throw JSONexception("Value is not really an array");
			return *getHeap<ArrayType>();
		}
		const ArrayType& array() const {
			if ((_contents & TYPE_MASK) != InternalType::ARRAY)
				throw JSONexception("Value is not really an array");
			return *getHeap<ArrayType>();
		}
		void array(const ArrayType& value) {
			cleanup();
			setArray(value);
		}
		const ArrayType& operator=(const ArrayType& value) {
			setArray(value);
			return value;
		}
		operator ArrayType() {
			return array();
		}

		size_t size() const {
			switch (_contents & TYPE_MASK) {
			case InternalType::SHORT_STRING: {
				size_t size = 0;
				while (size < STRING_BREAKPOINT) {
					if (!((_contents & 0xffull) << (size << 3)))
						break;
					size++;
				}
				return size;
			}
			case InternalType::LONG_STRING:
				return strlen(getHeap<char>());
			case InternalType::OBJECT:
				return getHeap<ObjectType>()->size();
			case InternalType::ARRAY:
				return getHeap<ArrayType>()->size();
			default:
				throw JSONexception("Getting size of a JSON type that doesn't define size");
			}
		}
		void push_back(const JSON& added) {
			array().push_back(added);
		}
		JSON& operator[] (size_t index) {
			return array()[index];
		}
		const JSON& operator[] (size_t index) const {
			if ((_contents & TYPE_MASK) != InternalType::ARRAY)
				throw JSONexception("Value is not really an array");
			return getHeap<ArrayType>()->at(index);
		}
		JSON& operator[] (const String& index) {
			return object()[index];
		}
		const JSON& operator[] (const String& index) const {
			if ((_contents & TYPE_MASK) != InternalType::OBJECT)
				throw JSONexception("Value is not really an object");
			return getHeap<ObjectType>()->at(index);
		}
		JSON& operator[] (const char* index) {
			return operator[](String(index));
		}
		const JSON& operator[] (const char* index) const {
			return operator[](String(index));
		}

		JSON& operator=(const JSON& other) {
			cleanup();
			_contents = other._contents;
			if (usesHeap())
				refcount()++;
			return *this;
		}
		JSON& operator=(JSON&& other) {
			cleanup();
			_contents = other._contents;
			other._contents = InternalType::NIL;
			return *this;
		}

		template <typename Format>
		auto to() const {
			return Format::serialise(*this);
		}

		template <typename Format, typename SourceType>
		static JSON from(const SourceType& source) {
			static_assert (std::is_same<std::decay_t<decltype(Format::deserialise(SourceType()))>, JSON>::value,
					"Format object does not take the given type as argument to deserialise");
			return Format::deserialise(source);
		}

		inline std::string toString() const;
		inline static JSON fromString(const std::string& source);

		template <typename Format>
		void saveAs(const std::string& fileName) const {
			SerialisableInternals::DiskAccessor<Format, void>::template save<JSON>(fileName, *this);
		}

		template <typename Format>
		static JSON loadAs(const std::string& fileName) {
			return SerialisableInternals::DiskAccessor<Format, void>::template load<JSON>(fileName);
		}
		inline void save(const std::string& fileName) const;
		inline static JSON load(const std::string& fileName);

		friend std::ostream& operator<<(std::ostream& stream , const JSON& json);
	};

	virtual JSON toJSON() const = 0;
	virtual void fromJSON(const JSON& source) = 0;


	/*!
	* \brief Serialises the object as a custom type
	* \tparam A class with a static method serialise(JSON)
	* \return The serialised value
	*
	* \note It calls the overloaded serialisation() method
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	template <typename Format>
	auto to() const {
		return toJSON().to<Format>();
	}

	/*!
	* \brief Deserialises the object from a custom type
	* \tparam A class with a static method deserialise() that returns JSON
	* \param The value to be deserialised
	*
	* \note It calls the overloaded serialisation() method
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	template <typename Format, typename SourceType>
	void from(const SourceType& source) {
		fromJSON(JSON::from<Format>(source));
	}

	/*!
	* \brief Serialises the object to a JSON string
	* \return The JSON string
	*
	* \note It calls the overloaded serialisation() method
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline std::string toString() const {
		return toJSON().toString();
	}

	/*!
	* \brief Loads the object from a JSON string
	* \param The JSON string
	*
	* \note It calls the overloaded serialisation() method
	* \note If the string is blank, nothing is done
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline void fromString(const std::string& source) {
		fromJSON(JSON::fromString(source));
	}

	/*!
	* \brief Saves the object to a custom format file
	* \tparam The format
	* \param The name of the file
	*
	* \note It calls the overloaded serialisation() method
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	template <typename Format>
	void saveAs(const std::string& fileName) const {
		toJSON().saveAs<Format>(fileName);
	}

	/*!
	* \brief Loads the object from a custom format file
	* \tparam The format
	* \param The name of the file
	*
	* \note It calls the overloaded serialisation() method
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	template <typename Format>
	void loadAs(const std::string& fileName) {
		fromJSON(JSON::loadAs<Format>(fileName));
	}

	/*!
	* \brief Loads the object from a JSON file
	* \param The name of the JSON file
	*
	* \note It calls the overloaded serialisation() method
	* \note If the file cannot be read, nothing is done
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline void load(const std::string& fileName) {
		fromJSON(JSON::load(fileName));
	}

	/*!
	* \brief Saves the object to a JSON file
	* \param The name of the JSON file
	*
	* \note It calls the overloaded serialisation() method
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline void save(const std::string& fileName) const {
		toJSON().save(fileName);
	}
};

class Serialisable : ISerialisable {

public:

	using SerialisationError = ISerialisable::SerialisationError;
	using JSONtype = ISerialisable::JSONtype;
	using JSON = ISerialisable::JSON;

private:
	static const char* base64chars() {
		static const char* held = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		return held;
	}
	class InverseBase64
	{
	public:
		static InverseBase64& getInstance()
		{
			static InverseBase64 instance;
			return instance;
		}
		const char* get() {
			return data.data();
		}
		InverseBase64(InverseBase64 const&) = delete;
		void operator=(InverseBase64 const&)  = delete;
	private:
		std::array<char, 256> data;
		InverseBase64() {
			for (int i = 0; i < 256; i++)
				data[i] = 0;
			for (unsigned int i = 0; i < 64; i++) {
				data[int(base64chars()[i])] = i;
			}
		}
	};

public:

	static std::string toBase64(const std::vector<uint8_t>& from) {
		const uint8_t* start = from.data();
		std::string result;
		for (unsigned int i = 0; i < from.size(); i += 3) {
			const uint8_t* s = reinterpret_cast<const unsigned char*>(start + i);
			char piece[5] = "====";
			piece[0] = base64chars()[s[0] >> 2];
			piece[1] = base64chars()[((s[0] & 3) << 4) + (s[1] >> 4)];
			if (&s[1] - start < int(from.size())) {
				piece[2] = base64chars()[((s[1] & 15) << 2) + (s[2] >> 6)];
				if (&s[2] - start < int(from.size()))
					piece[3] = base64chars()[s[2] & 63];
			}
			result.append(piece);
		}
		return result;
	}

	static std::vector<uint8_t> fromBase64(const std::string& from) {
		const char* start = from.c_str();
		std::vector<uint8_t> result;
		const char* invb64 = InverseBase64::getInstance().get();
		for (unsigned int i = 0; i < from.size(); i += 4) {
			const char* s = start + i;
			result.push_back((invb64[int(s[0])] << 2) | (invb64[int(s[1])] >> 4));
			if (s[2] != '=') {
				result.push_back((invb64[int(s[1])] << 4) | (invb64[int(s[2])] >> 2));
				if (s[3] != '=') result.push_back((invb64[int(s[2])] << 6) | invb64[int(s[3])]);
			}
		}
		return result;
	}

private:
	mutable JSON _json;
	mutable bool _saving;

protected:
	/*!
	* \brief Should all the synch() method on all members that are to be saved
	*
	* \note If something unusual needs to be done, the saving() method returns if it's being saved or not
	* \note The sync() method knows whether to save it or not
	* \note Const correctness is violated here, but I find it better than duplication
	*/
	virtual void serialisation() = 0;

	/*!
	* \brief Returns if the structure is being saved or loaded
	*
	* \return Whether it's saved (loaded if false)
	*
	* \note Result is meaningless outside a serialisation() overload
	*/
	inline bool saving() {
		return _saving;
	}

	/*!
	* \brief Saves or loads a value
	* \param The name of the value in the output/input file
	* \param Reference to the value
	* \return false if the value was absent while reading, true otherwise
	*/
	template <typename T>
	inline bool synch(const std::string& key, T& value) {
		static_assert(SerialisableInternals::Serialiser<T, void>::valid,
				"Trying to serialise a non-serialisable type");
		JSON::ObjectType& object = _json.object();
		if (_saving) {
			object[key] = SerialisableInternals::Serialiser<T, void>::serialise(value);
		} else {
			auto found = object.find(key);
			if (found != object.end()) {
				SerialisableInternals::Serialiser<T, void>::deserialise(value, found->second);
			} else return false;
		}
		return true;
	}

public:

	/*!
	* \brief Serialises the object to JSON
	* \return The JSON
	*
	* \note It calls the overloaded serialisation() method
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline JSON toJSON() const override {
		_json.setObject();
		_saving = true;
		const_cast<Serialisable*>(this)->serialisation();
		return _json;
	}

	/*!
	* \brief Loads the object from a JSON object
	* \param The JSON string
	*
	* \note It calls the overloaded serialisation() method
	* \note If the string is blank, nothing is done
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline void fromJSON(const JSON& source) override {
		JSON::Type type = source.type();
		if (type == JSON::Type::NIL) {
			// Maybe once should this optionally throw an exception?
			return;
		}
		if (type != JSON::Type::OBJECT)
			throw SerialisationError("Deserialising JSON from a wrong type");
		_json = source;
		_saving = false;
		serialisation();
		_json = nullptr;
	}
};

std::ostream& operator<<(std::ostream& stream , const Serialisable::JSON::String& str) {
	stream << std::string(str);
	return stream;
}

std::ostream& operator<<(std::ostream& stream , const Serialisable::JSON& json) {
	switch (json._contents & Serialisable::JSON::TYPE_MASK) {
	case Serialisable::JSON::InternalType::NIL:
		stream << std::string("null");
		break;
	case Serialisable::JSON::InternalType::BOOLEAN:
		stream << std::string(json.boolean() ? "true" : "false");
		break;
	case Serialisable::JSON::InternalType::SHORT_STRING:
	case Serialisable::JSON::InternalType::LONG_STRING:
		stream << '"' << json.string() << '"';
		break;
	case Serialisable::JSON::InternalType::OBJECT: {
		const Serialisable::JSON::ObjectType& object = *json.getHeap<Serialisable::JSON::ObjectType>();
		stream << '{';
		bool separated = false;
		for (auto& it : object) {
			if (!separated)
				separated = true;
			else
				stream << ", ";
			stream << " \"" << it.first << "\" : " << it.second;
		}
		stream << '}';
		break;
	}
	case Serialisable::JSON::InternalType::ARRAY: {
		const Serialisable::JSON::ArrayType& array = *json.getHeap<Serialisable::JSON::ArrayType>();
		stream << '[';
		for (unsigned int i = 0; i < array.size(); i++) {
			stream << array[i];
			if (i < array.size() - 1)
				stream << ", ";
		}
		stream << ']';
		break;
	}
	default:
		stream << json.number();
		break;
	}
	return stream;
}

namespace SerialisableInternals {

struct JSONformat {
	static std::string serialise(const Serialisable::JSON& serialised)  {
		std::stringstream stream;
		toStream(serialised, stream, 0);
		return stream.str();
	}

	static Serialisable::JSON deserialise(const std::string& source) {
		std::stringstream stream(source);
		return fromStream(stream);
	}

	static void toStream(const Serialisable::JSON& serialised, std::ostream& stream, int depth = 0) {
		switch(serialised.type()) {
		case Serialisable::JSON::Type::NIL:
			stream << "null";
			break;
		case Serialisable::JSON::Type::NUMBER:
			stream << serialised.number();
			break;
		case Serialisable::JSON::Type::BOOL:
			stream << (serialised.boolean() ? "true" : "false");
			break;
		case Serialisable::JSON::Type::STRING:
			stream.put('"');
			stream << serialised.string();
			stream.put('"');
			break;
		case Serialisable::JSON::Type::OBJECT:
		{
			stream.put('{');
			const auto object = serialised.object();
			if (object.empty()) {
				stream.put('}');
				return;
			}
			stream.put('\n');
			bool first = true;
			for (auto& it : object) {
				if (first)
					first = false;
				else {
					stream.put(',');
					stream.put('\n');
				}
				indent(stream, depth + 1);
				writeString(stream, it.first);
				stream.put(':');
				stream.put(' ');
				toStream(it.second, stream, depth + 1);
			}
			stream.put('\n');
			indent(stream, depth);
			stream.put('}');
			break;
		}
		case Serialisable::JSON::Type::ARRAY:
		{
			stream.put('[');
			const std::vector<Serialisable::JSON>& array = serialised.array();
			if (array.empty()) {
				stream.put(']');
				return;
			}
			for (unsigned int i = 0; i < array.size(); i++) {
				stream.put('\n');
				indent(stream, depth + 1);
				toStream(array[i], stream, depth + 1);
				if (i < array.size() - 1) stream.put(',');
			}
			stream.put('\n');
			indent(stream, depth);
			stream.put(']');
			break;
		}
		default:
			throw Serialisable::SerialisationError("Memory-corrupted JSON");
		}
	}

	static void indent(std::ostream& out, int depth) {
		for (int i = 0; i < depth; i++)
			out.put('\t');
	}
	static void writeString(std::ostream& out, const std::string& written) {
		out.put('"');
		for (unsigned int i = 0; i < written.size(); i++) {
			if (written[i] == '"') {
				out.put('/');
				out.put('"');
			} else if (written[i] == '\n') {
				out.put('\\');
				out.put('n');
			} else if (written[i] == '\\') {
				out.put('\\');
				out.put('\\');
			} else
				out.put(written[i]);
		}
		out.put('"');
	}

	static Serialisable::JSON fromStream(std::istream& stream) {
		auto readString = [&stream] () -> std::string {
			char letter = stream.get();
			std::string collected;
			while (letter != '"') {
				if (letter == '\\') {
					letter = stream.get();
					if (letter == '"') collected.push_back('"');
					else if (letter == 'n') collected.push_back('\n');
					else if (letter == '\\') collected.push_back('\\');
				} else {
					collected.push_back(letter);
				}
				letter = stream.get();
			}
			return collected;
		};
		auto readWhitespace = [&stream] () -> char {
			char letter;
			do {
				letter = stream.get();
			} while (letter == ' ' || letter == '\t' || letter == '\n' || letter == ',');
			return letter;
		};

		char letter = readWhitespace();
		if (letter == 0 || letter == EOF) return Serialisable::JSON();
		else if (letter == '"') {
			return Serialisable::JSON(readString());
		}
		else if (letter == 't') {
			if (stream.get() == 'r' && stream.get() == 'u' && stream.get() == 'e')
				return Serialisable::JSON(true);
			else
				throw(std::runtime_error("JSON parser found misspelled bool 'true'"));
		}
		else if (letter == 'f') {
			if (stream.get() == 'a' && stream.get() == 'l' && stream.get() == 's' && stream.get() == 'e')
				return Serialisable::JSON(false);
			else
				throw(std::runtime_error("JSON parser found misspelled bool 'false'"));
		}
		else if (letter == 'n') {
			if (stream.get() == 'u' && stream.get() == 'l' && stream.get() == 'l')
				return Serialisable::JSON();
			else
				throw(std::runtime_error("JSON parser found misspelled keyword 'null'"));
		}
		else if (letter == '-' || (letter >= '0' && letter <= '9') || letter == '+' || letter == '.') {
			std::string asString;
			asString.push_back(letter);
			letter = stream.get();
			while (letter == '-' || letter == '+' || letter == 'E' || letter == 'e' || letter == '.' || (letter >= '0' && letter <= '9')) {
				if (!stream.good()) break;
				asString.push_back(letter);
				letter = stream.get();
			}
			stream.unget();
			std::stringstream parsing(asString);
			double number;
			parsing >> number;
			return Serialisable::JSON(number);
		}
		else if (letter == '{') {
			Serialisable::JSON retval;
			retval.setObject();
			do {
				letter = readWhitespace();
				if (letter == '"') {
					const std::string& name = readString();
					letter = readWhitespace();
					if (letter != ':') throw(std::runtime_error("JSON parser expected an additional ':' somewhere"));
					retval[name] = fromStream(stream);
				} else break;
			} while (letter != '}');
			return retval;
		}
		else if (letter == '[') {
			Serialisable::JSON retval;
			auto& array = retval.setArray();
			letter = readWhitespace();
			while (letter != ']') {
				stream.unget();
				array.push_back(fromStream(stream));
				letter = readWhitespace();
			}
			return retval;
		} else {
			throw std::runtime_error(std::string("JSON parser found unexpected character ") + letter);
		}
		return Serialisable::JSON();
	}
};

template <typename Format>
struct DiskAccessor<Format, std::enable_if<
		std::is_same<decltype(Format::toStream(std::declval<Serialisable::JSON>(), std::declval<std::ostream>())), void>::value &&
		std::is_same<decltype(Format::fromStream(std::declval<std::ostream>())), Serialisable::JSON>::value>> {

	template <typename Internal>
	static void save(const std::string& fileName, const Internal& source) {
		std::ofstream stream(fileName, std::ios::binary);
		Format::toStream(source, stream);
	}

	template <typename Internal>
	static Internal load(const std::string& fileName) {
		std::ifstream stream(fileName, std::ios::binary);
		stream >> std::noskipws;
		return Format::fromStream(stream);
	}
};
}

inline std::string Serialisable::JSON::toString() const {
	return to<SerialisableInternals::JSONformat>();
}

inline Serialisable::JSON Serialisable::JSON::fromString(const std::string& source) {
	return from<SerialisableInternals::JSONformat>(source);
}

inline void Serialisable::JSON::save(const std::string& fileName) const {
	saveAs<SerialisableInternals::JSONformat>(fileName);
}

inline Serialisable::JSON Serialisable::JSON::load(const std::string& fileName) {
	return loadAs<SerialisableInternals::JSONformat>(fileName);
}

namespace SerialisableInternals {
template <typename Serialised>
struct Serialiser<Serialised, std::enable_if_t<std::is_integral<Serialised>::value>> {
	constexpr static bool valid = true;
	/*!
	* \brief Saves an arithmetic integer value
	* \param The value
	* \return The constructed JSON
	*/
	static Serialisable::JSON serialise(Serialised value) {
		return Serialisable::JSON(value);
	}
	/*!
	* \brief Loads an arithmetic integer value
	* \param Reference to the result value
	* \param The JSON
	* \throw If the type is wrong
	*/
	static void deserialise(Serialised& result, Serialisable::JSON value) {
		result = value.number();
	}
};

template <typename Serialised>
struct Serialiser<Serialised, std::enable_if_t<std::is_floating_point<Serialised>::value>> {
	constexpr static bool valid = true;
	/*!
	* \brief Saves an arithmetic floating point value
	* \param The value
	* \return The constructed JSON
	*/
	static Serialisable::JSON serialise(Serialised value) {
		return Serialisable::JSON(value);
	}
	/*!
	* \brief Loads an arithmetic floating point value
	* \param Reference to the result value
	* \param The JSON
	* \throw If the type is wrong
	*/
	static void deserialise(Serialised& result, const Serialisable::JSON& value) {
		result = value.number();
	}
};

template <>
struct Serialiser<std::string, void> {
	constexpr static bool valid = true;
	/*!
	* \brief Saves a string value
	* \param The value
	* \return The constructed JSON
	*/
	static Serialisable::JSON serialise(const std::string& value) {
		return Serialisable::JSON(value);
	}
	/*!
	* \brief Loads a string value
	* \param Reference to the result value
	* \param The JSON
	* \throw If the type is wrong
	*/
	static void deserialise(std::string& result, const Serialisable::JSON& value) {
		result = value.string();
	}
};

template <>
struct Serialiser<std::vector<uint8_t>, void> {
	constexpr static bool valid = true;
	/*!
	* \brief Saves a binary value expressed as a vector of uint8_t with base64 encoding to string
	* \param The value
	* \return The constructed JSON
	*/
	static Serialisable::JSON serialise(const std::vector<uint8_t>& value) {
		return Serialisable::JSON(Serialisable::toBase64(value));
	}
	/*!
	* \brief Loads a string value
	* \param Reference to the result value
	* \param The JSON
	* \throw If the type is wrong
	*/
	static void deserialise(std::vector<uint8_t>& result, const Serialisable::JSON& value) {
		result = Serialisable::fromBase64(value.string());
	}
};

template <typename Serialised>
struct Serialiser<Serialised, std::enable_if_t<std::is_enum<Serialised>::value>> {
	constexpr static bool valid = true;
	/*!
	* \brief Saves an enum as integer
	* \param The value
	* \return The constructed JSON
	*/
	static Serialisable::JSON serialise(Serialised value) {
		return Serialisable::JSON(std::underlying_type_t<Serialised>(value));
	}
	/*!
	* \brief Loads an integer as enum
	* \param The JSON
	* \return The value
	* \throw If the type is wrong
	*/
	static void deserialise(Serialised& result, const Serialisable::JSON& value) {
		result = Serialised(value.number());
	}
};

template <>
struct Serialiser<bool, void> {
	constexpr static bool valid = true;
	/*!
	* \brief Saves a boolean value
	* \param The value
	* \return The constructed JSON
	*/
	static Serialisable::JSON serialise(bool value) {
		return Serialisable::JSON(value);
	}
	/*!
	* \brief Loads a boolean value
	* \param Reference to the result value
	* \param The JSON
	* \throw If the type is wrong
	*/
	static void deserialise(bool& result, const Serialisable::JSON& value) {
		result = value.boolean();
	}
};

struct UnusualSerialisable { }; // Inherit from this to avoid using the following serialisation choice

template <typename Serialised>
struct Serialiser<Serialised, std::enable_if_t<std::is_base_of<Serialisable, Serialised>::value
		&& !std::is_base_of<UnusualSerialisable, Serialised>::value>> {
	constexpr static bool valid = true;
	/*!
	* \brief Saves an object of a class derived from Serialisable as JSON
	* \param The object
	* \return The constructed JSON
	*/
	static Serialisable::JSON serialise(Serialised value) {
		return value.toJSON();
	}
	/*!
	* \brief Loads JSON into an object of a class derived from Serialisable
	* \param Reference to the result value
	* \param The JSON
	* \throw If the something's wrong with the JSON
	*/
	static void deserialise(Serialised& result, const Serialisable::JSON& value) {
		result.fromJSON(value);
	}
};

template <typename T>
struct Serialiser<std::vector<T>, std::enable_if_t<Serialiser<T, void>::valid>> {
	constexpr static bool valid = true;
	/*!
	* \brief Saves a vector of serialisable values
	* \param The vector
	* \return The constructed JSON
	*/
	static Serialisable::JSON serialise(const std::vector<T>& value) {
		auto made = Serialisable::JSON(Serialisable::JSON::ArrayType(value.size()));
		for (unsigned int i = 0; i < value.size(); i++)
			made[i] = Serialiser<T, void>::serialise(value[i]);
		return made;
	}
	/*!
	* \brief Loads a vector of serialisable values
	* \param Reference to the result value
	* \param The JSON
	* \throw If the type is wrong
	*/
	static void deserialise(std::vector<T>& result, const Serialisable::JSON& value) {
		const std::vector<Serialisable::JSON>& got = value.array();
		result.resize(got.size());
		for (unsigned int i = 0; i < got.size(); i++)
			Serialiser<T, void>::deserialise(result[i], got[i]);
	}
};

template <typename T>
struct Serialiser<std::unordered_map<std::string, T>, std::enable_if_t<Serialiser<T, void>::valid>> {
	constexpr static bool valid = true;
	/*!
	* \brief Saves a hashtable of serialisable values
	* \param The vector
	* \return The constructed JSON
	*/
	static Serialisable::JSON serialise(const std::unordered_map<std::string, T>& value) {
		auto made = Serialisable::JSON(Serialisable::JSON::ObjectType(value.size() * 1.5));
		for (auto& it : value)
			made[it.first] = Serialiser<T, void>::serialise(it.second);
		return made;
	}
	/*!
	* \brief Loads a hashtable of serialisable values
	* \param Reference to the result value
	* \param The JSON
	* \throw If the type is wrong
	*/
	static void deserialise(std::unordered_map<std::string, T>& result, const Serialisable::JSON& value) {
		const auto& got = value.object();
		for (auto it = result.begin(); it != result.end(); ) {
			if (got.find(it->first) == got.end())
				it = result.erase(it);
			else
				++it;
		}
		for (auto& it : got)
			Serialiser<T, void>::deserialise(result[it.first], it.second);
	}
};

template <typename T>
struct Serialiser<std::shared_ptr<T>, std::enable_if_t<Serialiser<T, void>::valid>> {
	constexpr static bool valid = true;
	/*!
	* \brief Saves a shared pointer to a serialisable value
	* \param The vector
	* \return The constructed JSON
	*/
	static Serialisable::JSON serialise(const std::shared_ptr<T>& value) {
		if (value)
			return Serialiser<T, void>::serialise(*value);
		else
			return Serialisable::JSON(); // null
	}
	/*!
	* \brief Loads a shared pointer to a serialisable value
	* \param Reference to the result value
	* \param The JSON
	* \throw If the type is wrong
	*/
	static void deserialise(std::shared_ptr<T>& result, const Serialisable::JSON& value) {
		if (value && value.type() != Serialisable::JSON::Type::NIL) {
			if (!result)
				result = std::make_shared<T>();
			Serialiser<T, void>::deserialise(*result, value);
		} else
			result = std::shared_ptr<T>(); // nullptr
	}
};

template <typename T>
struct Serialiser<std::unique_ptr<T>, std::enable_if_t<Serialiser<T, void>::valid>> {
	constexpr static bool valid = true;
	/*!
	* \brief Saves a unique pointer to a serialisable value
	* \param The vector
	* \return The constructed JSON
	*/
	static Serialisable::JSON serialise(const std::unique_ptr<T>& value) {
		if (value)
			return Serialiser<T, void>::serialise(*value);
		else
			return Serialisable::JSON(); // null
	}
	/*!
	* \brief Loads a unique pointer to a serialisable value
	* \param Reference to the result value
	* \param The JSON
	* \throw If the type is wrong
	*/
	static void deserialise(std::unique_ptr<T>& result, const Serialisable::JSON& value) {
		if (value && value.type() != Serialisable::JSON::Type::NIL) {
			if (!result)
				result = std::make_unique<T>();
			Serialiser<T, void>::deserialise(*result, value);
		} else
			result = std::unique_ptr<T>(); // nullptr
	}
};

template <>
struct Serialiser<Serialisable::JSON, void> {
	constexpr static bool valid = true;
	/*!
	* \brief Dummy for using custom JSON as member
	* \param The value
	* \return The constructed JSON
	* \note JSON null is nullptr
	*/
	static Serialisable::JSON serialise(const Serialisable::JSON& value) {
		if (value)
			return value;
		else
			return Serialisable::JSON(); // null
	}
	/*!
	* \brief Dummy for using custom JSON as member
	* \param Reference to the result value
	* \param The JSON
	* \throw If the type is wrong
	* \note JSON null is nullptr
	*/
	static void deserialise(Serialisable::JSON& result, const Serialisable::JSON& value) {
		if (value.type() != Serialisable::JSON::Type::NIL)
			result = value;
		else
			result = nullptr;
	}
};

#if __cplusplus > 201402L
template <typename T>
struct Serialiser<std::optional<T>, std::enable_if_t<Serialiser<T, void>::valid>> {
	constexpr static bool valid = true;
	/*!
	* \brief Saves an optional serialisable value
	* \param The vector
	* \return The constructed JSON
	*/
	static Serialisable::JSON serialise(const std::optional<T>& value) {
		if (value)
			return Serialiser<T, void>::serialise(*value);
		else
			return Serialisable::JSON(); // null
	}
	/*!
	* \brief Loads an optional serialisable value
	* \param Reference to the result value
	* \param The JSON
	* \throw If the type is wrong
	*/
	static void deserialise(std::optional<T>& result, const Serialisable::JSON& value) {
		if (value && value.type() != Serialisable::JSON::Type::NIL) {
			if (!result)
				result = std::make_optional<T>();
			Serialiser<T, void>::deserialise(*result, value);
		} else
			result = std::nullopt;
	}
};
#endif
}

#endif //SERIALISABLE_BY_DUGI_HPP
