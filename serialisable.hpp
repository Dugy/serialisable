/*
* \brief Class convenient and duplication-free serialisation and deserialisation of its descendants into JSON, with one function to handle both loading and saving
*
* See more at https://github.com/Dugy/serialisable
*/

#ifndef SERIALISABLE_BY_DUGI_HPP
#define SERIALISABLE_BY_DUGI_HPP

// A flag allowing to adjust the default precision when using the condensed format. Possibilities:
// HALF_PRECISION - 15 bit float (almost half-precision), 1 bit is sign, 6 exponent, 8 mantissa, the imprecision is about 0.2%, maximal value is in the order of ten power 9
// SINGLE_PRECISION - regular float
// DOUBLE_PRECISION - regular double
// Default is HALF_PRECISION
#ifndef SERIALISABLE_BY_DUGI_CONDENSED_PREFER_PRECISION
#define SERIALISABLE_BY_DUGI_CONDENSED_PREFER_PRECISION HALF_PRECISION
#endif

#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <memory>
#include <fstream>
#include <exception>
#include <sstream>
#include <type_traits>
#include <cmath>
#include <algorithm>

class Serialisable {

	struct CondensedInfo {
		enum CondensedPrefix : uint8_t {
			HALF_PRECISION_FLOAT = 0b10000000,
			SHORT_STRING = 0b01100000,
			RESERVED_1 = 0b01111110,
			LONG_STRING = 0b01111111,
			MINIMAL_INTEGER = 0b01000000,
			COMMON_OBJECT = 0b00111000,
			RESERVED_2 = 0b00111101,
			UNCOMMON_OBJECT = 0b00111110,
			RARE_OBJECT = 0b00111111,
			SMALL_UNIQUE_OBJECT = 0b00110000,
			LARGE_UNIQUE_OBJECT = 0b00110110,
			HASHTABLE= 0b00110111,
			SHORT_ARRAY = 0b00100000,
			RESERVED_3 = 0b00101101,
			LONG_ARRAY = 0b00101111,
			VERY_SHORT_INTEGER = 0b00010000,
			DOUBLE = 0x0f,
			FLOAT = 0x0e,
			SIGNED_LONG_INTEGER = 0x0d,
			UNSIGNED_LONG_INTEGER = 0x0c,
			SIGNED_INTEGER = 0x0b,
			UNSIGNED_INTEGER = 0x0a,
			SIGNED_SHORT_INTEGER = 0x09,
			UNSIGNED_SHORT_INTEGER = 0x08,
			RESERVED_4 = 0x04,
			TRUE = 0x03,
			FALSE = 0x02,
			NIL = 0x01,
			TERMINATOR = 0x00,
		};
		constexpr static int MAX_SHORT_STRING_SIZE = 30;
		constexpr static int HALF_FLOAT_EXPONENT_BITS = 6;
		constexpr static int HALF_FLOAT_MANTISSA_BITS = 8;
		constexpr static int MAX_SHORT_ARRAY_SIZE = 14;
		constexpr static int MAX_COMMON_OBJECT_ID = 5;
		constexpr static int MAX_UNCOMMON_OBJECT_ID = MAX_COMMON_OBJECT_ID + 1 + 0xff;
		constexpr static int MAX_SMALL_UNIQUE_OBJECT_SIZE = 6;
		constexpr static int STRING_FINAL_BIT_FLIP = 0x80;

		constexpr static int HALF_PRECISION_FLOAT_MASK = 0x7f;
		constexpr static int SHORT_STRING_MASK = 0x1f;
		constexpr static int MINIMAL_INTEGER_MASK = 0x1f;
		constexpr static int MINIMAL_INTEGER_NUMBER_MASK = 0x0f;
		constexpr static int MINIMAL_INTEGER_SIGN_MASK = 0x10;
		constexpr static int OBJECT_MASK = 0x07;
		constexpr static int SHORT_ARRAY_MASK = 0x0f;
		constexpr static int VERY_SHORT_INTEGER_MASK = 0x0f;
		constexpr static int VERY_SHORT_INTEGER_SIGN_MASK = 0x08;
		constexpr static int VERY_SHORT_INTEGER_PREFIX_MASK = 0x07;
		constexpr static int RESERVED_4_MASK = 0x03;
	};

	struct ObjectMapEntry {
		int index;
		bool used = false;
	};

public:
	enum class JSONtype : uint8_t {
		NIL, // Can't be NULL, it's a macro in C
		STRING,
		DOUBLE,
		INTEGER,
		BOOL,
		ARRAY,
		OBJECT
	};
	struct JSON {
		inline virtual JSONtype type() {
			return JSONtype::NIL;
		}
		inline virtual std::string& getString() {
			throw(std::runtime_error("String value is not really string"));
		}
		inline virtual double& getDouble() {
			throw(std::runtime_error("Double value is not really double"));
		}
		inline virtual int64_t& getInt() {
			throw(std::runtime_error("Integer value is not really integer"));
		}
		inline virtual bool& getBool() {
			throw(std::runtime_error("Bool value is not really bool"));
		}
		inline virtual std::vector<std::shared_ptr<JSON>>& getVector() {
			throw(std::runtime_error("Array value is not really array"));
		}
		inline virtual std::unordered_map<std::string, std::shared_ptr<JSON>>& getObject() {
			throw(std::runtime_error("Object value is not really an object"));
		}
		inline virtual void write(std::ostream& out, int = 0) {
			out << "null";
		}
		inline void writeToFile(const std::string& fileName) {
			std::ofstream out(fileName);
			if (!out.good()) throw(std::runtime_error("Could not write to file " + fileName));
			this->write(out, 0);
		}
		std::vector<uint8_t> condensed() {
			std::vector<uint8_t> result;
			auto mapping = generateObjectMapping();
			writeCondensed(result, mapping);
			return result;
		}
		inline virtual void writeCondensed(std::vector<uint8_t>& buffer,
				std::unordered_map<std::string, ObjectMapEntry>& objectMapping) const {
			buffer.push_back(CondensedInfo::NIL);
		}
		virtual ~JSON() = default;
	protected:
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
		static void indent(std::ostream& out, int depth) {
			for (int i = 0; i < depth; i++)
				out.put('\t');
		}
		std::unordered_map<std::string, ObjectMapEntry> generateObjectMapping() {
			std::unordered_map<std::string, int> list;
			addToObjectList(list);

			std::vector<std::pair<std::string, int>> ordered;
			for (auto& it : list) {
				ordered.push_back(std::move(it));
			}
			std::sort(ordered.begin(), ordered.end(), [] (std::pair<std::string, int> first, std::pair<std::string, int> second) {
				return first.second > second.second;
			});
			
			std::unordered_map<std::string, ObjectMapEntry> result;
			for (unsigned int i = 0; i < ordered.size(); i++) {
				if (ordered[i].second <= 1) break; // Objects with a single occurrence are not saved this way
				if (i > 0xffff + CondensedInfo::MAX_UNCOMMON_OBJECT_ID) break; // Cannot batch so many object types
				result[ordered[i].first] = { int(i) };
			}
			return result;
		}
		virtual void addToObjectList(std::unordered_map<std::string, int>& list) const { }
		static void addToObjectList(std::shared_ptr<JSON> object, std::unordered_map<std::string, int>& list) {
			object->addToObjectList(list);
		}
	};
	struct JSONstring : public JSON {
		std::string _contents;
		JSONstring(const std::string& from = "") : _contents(from) {}

		inline virtual JSONtype type() {
			return JSONtype::STRING;
		}
		inline virtual std::string& getString() {
			return _contents;
		}
		inline void write(std::ostream& out, int = 0) override {
			writeString(out, _contents);
		}
		inline void writeCondensed(std::vector<uint8_t>& buffer,
				std::unordered_map<std::string, ObjectMapEntry>& objectMapping) const override {
			if (_contents.size() < CondensedInfo::MAX_SHORT_STRING_SIZE) {
				buffer.push_back(CondensedInfo::SHORT_STRING + _contents.size());
				for (auto c : _contents) {
					buffer.push_back(static_cast<uint8_t>(c));
				}
			} else {
				buffer.push_back(CondensedInfo::LONG_STRING);
				for (auto c : _contents) {
					buffer.push_back(static_cast<uint8_t>(c));
				}
				buffer.push_back(CondensedInfo::TERMINATOR);
			}
		}
	protected:
		void addToObjectList(std::unordered_map<std::string, int>& list) const override { }
	};
	struct JSONdouble : public JSON {
		double _value;
		enum class Hint : uint8_t {
			ABSENT,
			HALF_PRECISION,
			SINGLE_PRECISION,
			DOUBLE_PRECISION,
		};
		mutable Hint _hint;


		JSONdouble(double from = 0, Hint hint = Hint::ABSENT) : _value(from), _hint(hint) {}

		inline virtual JSONtype type() {
			return JSONtype::DOUBLE;
		}
		inline virtual double& getDouble() {
			return _value;
		}
		inline void write(std::ostream& out, int = 0) override {
			std::stringstream stream;
			stream << _value;
			std::string made = stream.str();
			out << made;
			if (made.find('.') == std::string::npos && made.find('e') == std::string::npos && made.find('e') == std::string::npos)
				out << ".0";
		}
		void updateCondensedSizeHint() const {
			uint64_t triedBinary = *reinterpret_cast<const uint64_t*>(&_value);
			double tried = fabs(_value);
			_hint = Hint::DOUBLE_PRECISION;
			if (tried > std::numeric_limits<float>::max() || (tried < std::numeric_limits<float>::min() && tried > 0))
				return; // Leave double, number is outside range
			if (Hint::SERIALISABLE_BY_DUGI_CONDENSED_PREFER_PRECISION != Hint::DOUBLE_PRECISION || float(tried) == tried || (triedBinary & 0x00000000fffffffc)) {
				// Either double is not preferred or it won't lose precision anyway or there are a lot of trailing blank bits
				_hint = Hint::SINGLE_PRECISION;
				constexpr float MAX_HALF_PRECISION = 8.57316e+09;
				constexpr float MIN_HALF_PRECISION_POSITIVE = 9.34961e-10;
				if (tried > MAX_HALF_PRECISION || (tried < MIN_HALF_PRECISION_POSITIVE && tried > 0))
					return; // Leave float
				if (Hint::SERIALISABLE_BY_DUGI_CONDENSED_PREFER_PRECISION == Hint::HALF_PRECISION || (triedBinary & 0x007ffffffffffffc)) {
					// Either float is not preferred or there are really a lot of trailing blank bits
					_hint = Hint::HALF_PRECISION;
				}
			}
		}
		inline void writeCondensed(std::vector<uint8_t>& buffer,
				std::unordered_map<std::string, ObjectMapEntry>& objectMapping) const override {
			if (_hint == Hint::ABSENT) updateCondensedSizeHint();
			if (_hint == Hint::HALF_PRECISION) {
				uint64_t source = *reinterpret_cast<const uint64_t*>(&_value);
				uint8_t result = 0x80 | ((source & 0x8000000000000000) >> 57); // Identification prefix and sign (1 + 1 bits)
				result |= ((source & 0x7ff0000000000000) >> 52) - 0x3e0; // Exponent (6 bits)
				buffer.push_back(result);
				buffer.push_back((source & 0x000fffffffffffff) >> 44); // Mantissa (1 byte)
			} else if (_hint == Hint::SINGLE_PRECISION) {
				buffer.push_back(CondensedInfo::FLOAT);
				float asFloat = float(_value);
				uint32_t extractor = *reinterpret_cast<const uint32_t*>(&asFloat);
				for (int i = 0; i < sizeof(float) * 8; i += 8) {
					buffer.push_back((extractor & (0xff << i)) >> i);
				}
			} else if (_hint == Hint::DOUBLE_PRECISION) {
				buffer.push_back(CondensedInfo::DOUBLE);
				uint64_t extractor = *reinterpret_cast<const uint64_t*>(&_value);
				for (int i = 0; i < sizeof(double) * 8; i += 8)
					buffer.push_back((extractor & (0xffull << i)) >> i);
			}
		}
	protected:
		void addToObjectList(std::unordered_map<std::string, int>& list) const override { }
	};
	struct JSONint : public JSON {
		int64_t _value;
		JSONint(int64_t from = 0) : _value(from) {}

		inline virtual JSONtype type() {
			return JSONtype::INTEGER;
		}
		inline virtual int64_t& getInt() {
			return _value;
		}
		inline void write(std::ostream& out, int = 0) {
			out << _value;
		}
		inline void writeCondensed(std::vector<uint8_t>& buffer,
				std::unordered_map<std::string, ObjectMapEntry>& objectMapping) const override {
			auto writeBinary = [&buffer] (uint8_t lead, auto number) {
				buffer.push_back(lead);
				for (int i = 0; i < sizeof(number) * 8; i += 8)
					buffer.push_back((number >> i) & 0xffull);
			};
			if (_value <= 15 && _value >= -16) {
				buffer.push_back((static_cast<int8_t>(_value) & (CondensedInfo::MINIMAL_INTEGER_MASK)) | CondensedInfo::MINIMAL_INTEGER);
			} else if (_value <= 2047 && _value >= -2048) {
				buffer.push_back(CondensedInfo::VERY_SHORT_INTEGER | ((_value & 0x0f00) >> 8));
				buffer.push_back(_value & 0xff);
			} else if (_value < std::numeric_limits<int16_t>::max() && _value > std::numeric_limits<int16_t>::min())
				writeBinary(CondensedInfo::SIGNED_SHORT_INTEGER, static_cast<int16_t>(_value));
			else if (_value < std::numeric_limits<uint16_t>::max() && _value > std::numeric_limits<uint16_t>::min())
				writeBinary(CondensedInfo::UNSIGNED_SHORT_INTEGER, static_cast<uint16_t>(_value));
			else if (_value < std::numeric_limits<int32_t>::max() && _value > std::numeric_limits<int32_t>::min())
				writeBinary(CondensedInfo::SIGNED_INTEGER, static_cast<int32_t>(_value));
			else if (_value < std::numeric_limits<uint32_t>::max() && _value > std::numeric_limits<uint32_t>::min())
				writeBinary(CondensedInfo::UNSIGNED_INTEGER, static_cast<uint32_t>(_value));
			else if (_value < std::numeric_limits<int64_t>::max() && _value > std::numeric_limits<int64_t>::min())
				writeBinary(CondensedInfo::SIGNED_LONG_INTEGER, static_cast<int64_t>(_value));
			else if (_value < std::numeric_limits<uint64_t>::max() && _value > std::numeric_limits<uint64_t>::min())
				writeBinary(CondensedInfo::UNSIGNED_LONG_INTEGER, static_cast<uint64_t>(_value));
		}
	protected:		
		void addToObjectList(std::unordered_map<std::string, int>& list) const override { }
	};
private:
	
	struct JSONdoubleOrInt : public JSONint {
		double valueDouble_;
		JSONdoubleOrInt(int64_t from = 0) : JSONint(from), valueDouble_(double(from)) {}

		inline double& getDouble() override {
			return valueDouble_;
		}
	};
public:
	struct JSONbool : public JSON {
		bool _value;
		JSONbool(bool from = false) : _value(from) {}

		inline virtual JSONtype type() {
			return JSONtype::BOOL;
		}
		inline virtual bool& getBool() {
			return _value;
		}
		inline void write(std::ostream& out, int = 0) override {
			out << (_value ? "true" : "false");
		}
		inline void writeCondensed(std::vector<uint8_t>& buffer,
				std::unordered_map<std::string, ObjectMapEntry>& objectMapping) const override {
			if (_value)
				buffer.push_back(CondensedInfo::TRUE);
			else
				buffer.push_back(CondensedInfo::FALSE);
		}
	protected:		
		virtual void addToObjectList(std::unordered_map<std::string, int>& list) { }
	};
	struct JSONobject : public JSON {
		std::unordered_map<std::string, std::shared_ptr<JSON>> _contents;
		JSONobject() {}

		inline virtual JSONtype type() {
			return JSONtype::OBJECT;
		}
		inline virtual std::unordered_map<std::string, std::shared_ptr<JSON>>& getObject() {
			return _contents;
		}
		inline void write(std::ostream& out, int depth = 0) override {
			if (_contents.empty()) {
				out.put('{');
				out.put('}');
				return;
			}
			out.put('{');
			out.put('\n');
			bool first = true;
			for (auto& it : _contents) {
				if (first)
					first = false;
				else {
					out.put(',');
					out.put('\n');
				}
				indent(out, depth + 1);
				writeString(out, it.first);
				out.put(':');
				out.put(' ');
				it.second->write(out, depth + 1);
			}
			out.put('\n');
			indent(out, depth);
			out.put('}');
		}
		inline void writeCondensed(std::vector<uint8_t>& buffer,
				std::unordered_map<std::string, ObjectMapEntry>& objectMapping) const override {
			if (_contents.empty()) {
				buffer.push_back(CondensedInfo::SMALL_UNIQUE_OBJECT); // Does not need to be saved
				return;
			}
			std::pair<std::string, bool> descriptor = getDescriptor();
			if (descriptor.second) {
				auto mapping = objectMapping.find(descriptor.first);
				if (mapping != objectMapping.end()) {
					int index = mapping->second.index;
					if (index <= CondensedInfo::MAX_COMMON_OBJECT_ID)
						buffer.push_back(CondensedInfo::COMMON_OBJECT | index);
					else if (index <= CondensedInfo::MAX_UNCOMMON_OBJECT_ID) {
						index -= CondensedInfo::MAX_COMMON_OBJECT_ID + 1;
						buffer.push_back(CondensedInfo::UNCOMMON_OBJECT);
						buffer.push_back(index);
					} else {
						index -= CondensedInfo::MAX_UNCOMMON_OBJECT_ID + 1;
						buffer.push_back(CondensedInfo::UNCOMMON_OBJECT);
						buffer.push_back(index >> 8);
						buffer.push_back(index & 0xff);
					}
					if (!mapping->second.used) {
						if (!descriptor.first.empty()) {
							for (auto c : descriptor.first)
								buffer.push_back(c);
							buffer.push_back(CondensedInfo::TERMINATOR);
							mapping->second.used = true;
						}
					}
				} else {
					if (_contents.size() < CondensedInfo::MAX_SMALL_UNIQUE_OBJECT_SIZE) {
						buffer.push_back(CondensedInfo::SMALL_UNIQUE_OBJECT | _contents.size());
						for (auto c : descriptor.first)
							buffer.push_back(c);
					} else {
						buffer.push_back(CondensedInfo::LARGE_UNIQUE_OBJECT);
						for (auto c : descriptor.first)
							buffer.push_back(c);
						buffer.push_back(CondensedInfo::TERMINATOR);
					}
				}

				auto ordered = getOrdered();
				for (auto& it : ordered) {
					it.second->writeCondensed(buffer, objectMapping);
				}
			} else {
				buffer.push_back(CondensedInfo::HASHTABLE);
				for (auto& it : _contents)
					if (!it.first.empty()) {
						for (auto c : it.first)
							buffer.push_back(c);
						buffer.push_back(CondensedInfo::TERMINATOR);
				}
				if (_contents.find("") != _contents.end()) // Empty string must go last
					buffer.push_back(CondensedInfo::TERMINATOR);
				buffer.push_back(CondensedInfo::TERMINATOR);
				for (auto& it : _contents)
					if (!it.first.empty()) {
						it.second->writeCondensed(buffer, objectMapping);
					}
				auto empty = _contents.find("");
				if (empty != _contents.end())
					empty->second->writeCondensed(buffer, objectMapping);
			}
		}
	private:
		void addToObjectList(std::unordered_map<std::string, int>& list) const override {
			if (_contents.empty())
				return;
			std::pair<std::string, bool> descriptor = getDescriptor();
			if (descriptor.second)
				list[descriptor.first]++;

			for (auto& it : _contents)
				JSON::addToObjectList(it.second, list);
		}
	protected:	
		std::vector<std::pair<std::string, std::shared_ptr<JSON>>> getOrdered() const {
			// They must be sorted in order to notice identical objects
			std::vector<std::pair<std::string, std::shared_ptr<JSON>>> ordered;
			for (auto& it : _contents)
				ordered.push_back(it);
			std::sort(ordered.begin(), ordered.end(), [] (auto first, auto second) {
				return first.first < second.first;
			});
			return ordered;
		}
		std::pair<std::string, bool> getDescriptor() const {
			std::string composed;
			auto ordered = getOrdered();
			for (auto& it : ordered) {
				if (it.first.empty()) {
					composed.push_back(CondensedInfo::STRING_FINAL_BIT_FLIP);
				}
				for (unsigned int i = 0; i < it.first.size(); i++) {
					if (it.first[i] > 0) {
						if (i < it.first.size() - 1)
							composed.push_back(it.first[i]);
						else
							composed.push_back(uint8_t(it.first[i]) | CondensedInfo::STRING_FINAL_BIT_FLIP);
					} else {
						return { "", false };
					}
				}
			}
			return { composed, true };
		}
	};
	struct JSONarray : public JSON {
		std::vector<std::shared_ptr<JSON>> _contents;
		JSONarray() {}

		inline virtual JSONtype type() {
			return JSONtype::ARRAY;
		}
		inline virtual std::vector<std::shared_ptr<JSON>>& getVector() {
			return _contents;
		}
		inline void write(std::ostream& out, int depth = 0) override {
			out.put('[');
			if (_contents.empty()) {
				out.put(']');
				return;
			}
			for (unsigned int i = 0; i < _contents.size(); i++) {
				out.put('\n');
				indent(out, depth + 1);
				_contents[i]->write(out, depth + 1);
				if (i < _contents.size() - 1) out.put(',');
			}
			out.put('\n');
			indent(out, depth);
			out.put(']');
			
		}
		inline void writeCondensed(std::vector<uint8_t>& buffer,
				std::unordered_map<std::string, ObjectMapEntry>& objectMapping) const override {
			if (_contents.size() < CondensedInfo::MAX_SHORT_ARRAY_SIZE) {
				buffer.push_back(CondensedInfo::SHORT_ARRAY | _contents.size());
				for (auto& it : _contents)
					it->writeCondensed(buffer, objectMapping);
			} else {
				buffer.push_back(CondensedInfo::LONG_ARRAY);
				for (auto& it : _contents)
					it->writeCondensed(buffer, objectMapping);
				buffer.push_back(CondensedInfo::TERMINATOR);
			}
		}
	protected:		
		void addToObjectList(std::unordered_map<std::string, int>& list) const override {
			for (auto& it : _contents) {
				JSON::addToObjectList(it, list);
			}
		}
	};

	static std::shared_ptr<JSON> parseJSON(std::istream& in) {
		auto readString = [&in] () -> std::string {
			char letter = in.get();
			std::string collected;
			while (letter != '"') {
				if (letter == '\\') {
					if (in.get() == '"') collected.push_back('"');
					else if (in.get() == 'n') collected.push_back('\n');
					else if (in.get() == '\\') collected.push_back('\\');
				} else {
					collected.push_back(letter);
				}
				letter = in.get();
			}
			return collected;
		};
		auto readWhitespace = [&in] () -> char {
			char letter;
			do {
				letter = in.get();
			} while (letter == ' ' || letter == '\t' || letter == '\n' || letter == ',');
			return letter;
		};

		char letter = readWhitespace();
		if (letter == 0 || letter == EOF) return std::make_shared<JSON>();
		else if (letter == '"') {
			return std::make_shared<JSONstring>(readString());
		}
		else if (letter == 't') {
			if (in.get() == 'r' && in.get() == 'u' && in.get() == 'e')
				return std::make_shared<JSONbool>(true);
			else
				throw(std::runtime_error("JSON parser found misspelled bool 'true'"));
		}
		else if (letter == 'f') {
			if (in.get() == 'a' && in.get() == 'l' && in.get() == 's' && in.get() == 'e')
				return std::make_shared<JSONbool>(false);
			else
				throw(std::runtime_error("JSON parser found misspelled bool 'false'"));
		}
		else if (letter == 'n') {
			if (in.get() == 'u' && in.get() == 'l' && in.get() == 'l')
				return std::make_shared<JSON>();
			else
				throw(std::runtime_error("JSON parser found misspelled keyword 'null'"));
		}
		else if (letter == '-' || (letter >= '0' && letter <= '9')) {
			std::string asString;
			asString.push_back(letter);
			bool hasDecimal = false;
			do {
				letter = in.get();
				if (!hasDecimal && (letter == '.' || letter == 'e' || letter == 'E'))
					hasDecimal = true;
				asString.push_back(letter);
			} while (letter == '-' || letter == '+' || letter == 'E' || letter == 'e' || letter == ',' || letter == '.' || (letter >= '0' && letter <= '9'));
			in.unget();
			std::stringstream parsing(asString);
			if (hasDecimal) {
				double number;
				parsing >> number;
				return std::make_shared<JSONdouble>(number);
			}
			else {
				int64_t number;
				parsing >> number;
				return std::make_shared<JSONdoubleOrInt>(number); // It may happen to be actually meant as double
			}
		}
		else if (letter == '{') {
			auto retval = std::make_shared<JSONobject>();
			do {
				letter = readWhitespace();
				if (letter == '"') {
					const std::string& name = readString();
					letter = readWhitespace();
					if (letter != ':') throw(std::runtime_error("JSON parser expected an additional ':' somewhere"));
					retval->getObject()[name] = parseJSON(in);
				} else break;
			} while (letter != '}');
			return retval;
		}
		else if (letter == '[') {
			auto retval = std::make_shared<JSONarray>();
			letter = readWhitespace();
			while (letter != ']') {
				in.unget();
				retval->getVector().push_back(parseJSON(in));
				letter = readWhitespace();
			}
			return retval;
		} else {
			throw std::runtime_error(std::string("JSON parser found unexpected character ") + letter);
		}
		return std::make_shared<JSON>();
	}
	static std::shared_ptr<JSON> parseJSON(const std::string& fileName) {
		std::ifstream in(fileName);
		if (!in.good()) return std::make_shared<JSON>();
		return parseJSON(in);
	}

private:
	static std::shared_ptr<JSON> parseCondensed(uint8_t const*& source, const uint8_t* end, std::vector<std::unique_ptr<std::vector<std::string>>>& objects) {
		// Recursion invariant: source always points to 1 byte before the start of the object
		auto next = [&] () {
			source++;
			if (source >= end)
				throw std::runtime_error("Condensed JSON got to an unexpected end of data");
		};
		auto peek = [&] () {
			if (source + 1 >= end)
				throw std::runtime_error("Condensed JSON got to an unexpected end of data");
			return *(source + 1);
		};
		auto parseInt = [&] (auto typeIdentificator) {
			decltype(typeIdentificator) made = 0;
			for (int i = 0; i < sizeof(typeIdentificator) * 8; i += 8) {
				next();
				made |= uint64_t(*source) << i;
			}
			return made;
		};
		auto readCodeString = [&] () {
			next();
			if (*source == CondensedInfo::STRING_FINAL_BIT_FLIP)
				return std::string("");

			std::string made;
			while (true) {
				if (*source < CondensedInfo::STRING_FINAL_BIT_FLIP) {
					made.push_back(*source);
					next();
				} else {
					made.push_back(*source & 0x7f);
					return made;
				}
			}
		};
		auto parseObjectUsingDict = [&] (const std::vector<std::string>& names) {
			auto made = std::make_shared<JSONobject>();
			for (const std::string& it : names) {
				made->_contents[it] = parseCondensed(source, end, objects);
			}
			return made;
		};
		auto parseObject = [&] (const int index) {
			if (objects.size() < index + 1)
				objects.resize(index + 1);
			if (!objects[index]) {
				objects[index] = std::make_unique<std::vector<std::string>>();
				while (peek() != CondensedInfo::TERMINATOR)
					objects[index]->push_back(readCodeString());
				next();
			}
			return parseObjectUsingDict(*objects[index]);
		};

		next();
		if (*source & CondensedInfo::HALF_PRECISION_FLOAT) {
			unsigned long int result = (*source & 0x40ull) << 57; // Sign
			result |= (0x3e0 + (*source & 0x3full)) << 52; // Exponent
			next();
			result |= uint64_t(*source) << 44; // Mantissa
			return std::make_shared<JSONdouble>(*reinterpret_cast<double*>(&result), JSONdouble::Hint::HALF_PRECISION);
		} else if (*source == CondensedInfo::LONG_STRING) {
			auto made = std::make_shared<JSONstring>();
			next();
			while (*source) {
				made->_contents.push_back(*source);
				next();
			};
			return made;
		} else if (*source == CondensedInfo::RESERVED_1) {
			throw(std::runtime_error("Condensed JSON version is too low"));
		} else if ((*source & 0b11100000) == CondensedInfo::SHORT_STRING) {
			auto made = std::make_shared<JSONstring>();
			int length = *source & CondensedInfo::SHORT_STRING_MASK;
			for (int i = 0; i < length; i++) {
				next();
				made->_contents.push_back(*source);
			}
			return made;
		} else if ((*source & 0b11100000) == CondensedInfo::MINIMAL_INTEGER) {
			auto made = std::make_shared<JSONint>();
			made->_value = (*source & CondensedInfo::MINIMAL_INTEGER_NUMBER_MASK);
			if (*source & CondensedInfo::MINIMAL_INTEGER_SIGN_MASK)
				made->_value |= 0xfffffffffffffff0;
			return made;
		} else if (*source == CondensedInfo::RESERVED_2) {
			throw(std::runtime_error("Condensed JSON version is too low"));
		} else if (*source == CondensedInfo::UNCOMMON_OBJECT) {
			next();
			int index = *source + CondensedInfo::MAX_COMMON_OBJECT_ID;
			return parseObject(index);
		} else if (*source == CondensedInfo::RARE_OBJECT) {
			next();
			int index = *source << 8;
			next();
			index += *source + CondensedInfo::MAX_UNCOMMON_OBJECT_ID;
			return parseObject(index);
		} else if ((*source & CondensedInfo::COMMON_OBJECT) == CondensedInfo::COMMON_OBJECT) {
			int index = *source & CondensedInfo::OBJECT_MASK;
			return parseObject(index);
		} else if (*source == CondensedInfo::LARGE_UNIQUE_OBJECT) {
			std::vector<std::string> names;
			while (peek() != CondensedInfo::TERMINATOR) {
				names.push_back(readCodeString());
			}
			next();
			return parseObjectUsingDict(names);
		} else if (*source == CondensedInfo::HASHTABLE) {
			std::vector<std::string> names;
			next();
			while (*source != CondensedInfo::TERMINATOR) {
				std::string made;
				while (*source != CondensedInfo::TERMINATOR) {
					made.push_back(*source);
					next();
				}
				next();
				names.push_back(made);
			}
			if (peek() == CondensedInfo::TERMINATOR) {
				names.push_back("");
				next();
			}
			return parseObjectUsingDict(names);
		} else if ((*source & 0xf0) == CondensedInfo::SMALL_UNIQUE_OBJECT) {
			int size = *source & CondensedInfo::OBJECT_MASK;
			std::vector<std::string> names;
			for (int i = 0; i < size; i++)
				names.push_back(readCodeString());
			return parseObjectUsingDict(names);
		} else if (*source == CondensedInfo::LONG_ARRAY) {
			auto made = std::make_shared<JSONarray>();
			while (peek() != CondensedInfo::TERMINATOR)
				made->_contents.push_back(parseCondensed(source, end, objects));
			made->_contents.shrink_to_fit();
			return made;
		} else if ((*source & 0xf0) == CondensedInfo::SHORT_ARRAY) {
			auto made = std::make_shared<JSONarray>();
			int size = *source & CondensedInfo::SHORT_ARRAY_MASK;
			made->_contents.reserve(size);
			for (int i = 0; i < size; i++)
				made->_contents.push_back(parseCondensed(source, end, objects));
			made->_contents.shrink_to_fit();
			return made;
		} else if ((*source & 0xf0) == CondensedInfo::VERY_SHORT_INTEGER) {
			auto made = std::make_shared<JSONint>();
			made->_value = (*source & CondensedInfo::VERY_SHORT_INTEGER_PREFIX_MASK) << 8;
			if (*source & CondensedInfo::VERY_SHORT_INTEGER_SIGN_MASK)
				made->_value |= 0xfffffffffffff800;
			next();
			made->_value |= *source;
			return made;
		} else if (*source == CondensedInfo::DOUBLE) {
			uint64_t buffer = 0;
			for (int i = 0; i < sizeof(double) * 8; i += 8) {
				next();
				buffer |= uint64_t(*source) << i;
			}
			return std::make_shared<JSONdouble>(*reinterpret_cast<double*>(&buffer), JSONdouble::Hint::DOUBLE_PRECISION);
		} else if (*source == CondensedInfo::FLOAT) {
			uint32_t buffer = 0;
			for (int i = 0; i < sizeof(float) * 8; i += 8) {
				next();
				buffer |= uint32_t(*source) << i;
			}
			return std::make_shared<JSONdouble>(*reinterpret_cast<float*>(&buffer), JSONdouble::Hint::SINGLE_PRECISION);
		} else if (*source == CondensedInfo::SIGNED_LONG_INTEGER) {
			return std::make_shared<JSONint>(parseInt(int64_t()));
		} else if (*source == CondensedInfo::UNSIGNED_LONG_INTEGER) {
			return std::make_shared<JSONint>(parseInt(uint64_t()));
		} else if (*source == CondensedInfo::SIGNED_INTEGER) {
			return std::make_shared<JSONint>(parseInt(int32_t()));
		} else if (*source == CondensedInfo::UNSIGNED_INTEGER) {
			return std::make_shared<JSONint>(parseInt(uint32_t()));
		} else if (*source == CondensedInfo::SIGNED_SHORT_INTEGER) {
			return std::make_shared<JSONint>(parseInt(int16_t()));
		} else if (*source == CondensedInfo::UNSIGNED_SHORT_INTEGER) {
			return std::make_shared<JSONint>(parseInt(uint16_t()));
		} else if (*source == CondensedInfo::TRUE) {
			return std::make_shared<JSONbool>(true);
		} else if (*source == CondensedInfo::FALSE) {
			return std::make_shared<JSONbool>(false);
		} else if (*source == CondensedInfo::NIL) {
			return std::make_shared<JSON>();
		} else if (*source == CondensedInfo::NIL) {
			throw(std::runtime_error("Condensed JSON stumbled upon an unexpected ending symbol"));
		}
		throw(std::runtime_error("Condensed JSON failed to recognise type information: " + std::to_string(*source) + " after " + std::to_string(*(source - 1))));
	}

public:
	static std::shared_ptr<JSON> parseCondensed(const std::vector<uint8_t>& source) {
		const uint8_t* data = source.data() - 1;
		std::vector<std::unique_ptr<std::vector<std::string>>> objects;
		return parseCondensed(data, source.data() + source.size(), objects);
	}


private:
	mutable std::shared_ptr<JSON> _json;
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
	* \brief Saves or loads a string value
	* \param The name of the value in the output/input file
	* \param Reference to the value
	* \return false if the value was absent while reading, true otherwise
	*/
	inline bool synch(const std::string& key, std::string& value) {
		if (_saving) {
			_json->getObject()[key] = std::make_shared<JSONstring>(value);
		} else {
			auto found = _json->getObject().find(key);
			if (found != _json->getObject().end()) {
				value = found->second->getString();
			} else return false;
		}
		return true;
	}
	
	/*!
	* \brief Saves or loads an arithmetic value
	* \param The name of the value in the output/input file
	* \param Reference to the value
	* \return false if the value was absent while reading, true otherwise
	*
	* \note The value is converted from and to a double for JSON conformity
	*/
	template<typename T>
	typename std::enable_if<std::is_floating_point<T>::value, bool>::type
	synch(const std::string& key, T& value) {
		if (_saving) {
			_json->getObject()[key] = std::make_shared<JSONdouble>(double(value));
		} else {
			auto found = _json->getObject().find(key);
			if (found != _json->getObject().end()) {
				value = T(found->second->getDouble());
			} return false;
		}
		return true;
	}

	/*!
	* \brief Saves or loads an arithmetic integer value
	* \param The name of the value in the output/input file
	* \param Reference to the value
	* \return false if the value was absent while reading, true otherwise
	*/
	template<typename T>
	typename std::enable_if<std::is_integral<T>::value, bool>::type
		synch(const std::string& key, T& value) {
		if (_saving) {
			_json->getObject()[key] = std::make_shared<JSONint>(int64_t(value));
		}
		else {
			auto found = _json->getObject().find(key);
			if (found != _json->getObject().end()) {
				value = T(found->second->getInt());
			} return false;
		}
		return true;
	}

	/*!
	* \brief Saves or loads an enum as integer
	* \param The name of the value in the output/input file
	* \param Reference to the value
	* \return false if the value was absent while reading, true otherwise
	*/
	template<typename T>
	typename std::enable_if<std::is_enum<T>::value, bool>::type
		synch(const std::string& key, T& value) {
		if (_saving) {
			_json->getObject()[key] = std::make_shared<JSONint>(std::underlying_type_t<T>(value));
		}
		else {
			auto found = _json->getObject().find(key);
			if (found != _json->getObject().end()) {
				value = T(found->second->getInt());
			} return false;
		}
		return true;
	}

	/*!
	* \brief Saves or loads a bool
	* \param The name of the value in the output/input file
	* \param Reference to the value
	* \return false if the value was absent while reading, true otherwise
	*/
	inline bool synch(const std::string& key, bool& value) {
		if (_saving) {
			_json->getObject()[key] = std::make_shared<JSONbool>(value);
		} else {
			auto found = _json->getObject().find(key);
			if (found != _json->getObject().end()) {
				value = found->second->getBool();
			} else return false;
		}
		return true;
	}
	
	/*!
	* \brief Saves or loads an object derived from Serialisable held in a smart pointer
	* \param The name of the value in the output/input file
	* \param Reference to the pointer
	* \return false if the value was absent while reading, true otherwise
	*
	\ \note The smart pointer class must be dereferencable through operator*(), constructible from raw pointer to the class and the ! operation must result in a number
	* \note If not null, the contents will be overwritten, so raw pointers must be initalised before calling it, but no memory leak will occur
	*/
	template<typename T>
	typename std::enable_if<!std::is_base_of<Serialisable, T>::value
			&& std::is_same<bool, typename std::remove_reference<decltype(std::declval<Serialisable>().synch(std::declval<std::string>(), *std::declval<T>()))>::type>::value
			&& std::is_constructible<T, typename std::remove_reference<decltype(*std::declval<T>())>::type*>::value
			&& std::is_arithmetic<typename std::remove_reference<decltype(!std::declval<T>())>::type>::value , bool>::type
	synch(const std::string& key, T& value) {
		if (_saving) {
			if (!value)
				_json->getObject()[key] = std::make_shared<JSON>();
			else {
				synch(key, *value);
			}
		} else {
			auto found = _json->getObject().find(key);
			if (found != _json->getObject().end()) {
				if (found->second->type() != JSONtype::NIL) {
					value = std::move(T(new typename std::remove_reference<decltype(*std::declval<T>())>::type()));
					synch(key, *value);
				} else
					value = nullptr;
			} else {
				value = nullptr;
				return false;
			}
		}
		return true;
	}
	
	/*!
	* \brief Saves or loads an object derived from Serialisable
	* \param The name of the value in the output/input file
	* \param Reference to the value
	* \return false if the value was absent while reading, true otherwise
	*/
	template<typename T>
	typename std::enable_if<std::is_base_of<Serialisable, T>::value, bool>::type
	synch(const std::string& key, T& value) {
		value._saving = _saving;
		if (_saving) {
			auto making = std::make_shared<JSONobject>();
			value._json = making;
			value.serialisation();
			_json->getObject()[key] = making;
			value._json.reset();
		} else {
			auto found = _json->getObject().find(key);
			if (found != _json->getObject().end()) {
				value._json = found->second;
				value.serialisation();
				value._json.reset();
			} else return false;
		}
		return true;
	}
	
	/*!
	* \brief Saves or loads a vector of objects derived from Serialisable
	* \param The name of the value in the output/input file
	* \param Reference to the vector
	* \return false if the value was absent while reading, true otherwise
	*
	* \note Class must be default constructible
	*/
	template<typename T>
	typename std::enable_if<std::is_base_of<Serialisable, T>::value, bool>::type
	synch(const std::string& key, std::vector<T>& value) {
		if (_saving) {
			auto making = std::make_shared<JSONarray>();
			for (unsigned int i = 0; i < value.size(); i++) {
				auto innerMaking = std::make_shared<JSONobject>();
				value[i]._saving = true;
				value[i]._json = innerMaking;
				value[i].serialisation();
				value[i]._json.reset();
				making->getVector().push_back(innerMaking);
			}
			_json->getObject()[key] = making;
		} else {
			value.clear();
			auto found = _json->getObject().find(key);
			if (found != _json->getObject().end()) {
				for (unsigned int i = 0; i < found->second->getVector().size(); i++) {
					value.push_back(T());
					T& filled = value.back();
					filled._saving = false;
					filled._json = found->second->getVector()[i];
					filled.serialisation();
					filled._json.reset();
				}
			} else return false;
		}
		return true;
	}
	
	/*!
	* \brief Saves or loads a vector of smart pointers to objects derived from Serialisable
	* \param The name of the value in the output/input file
	* \param Reference to the vector
	* \return false if the value was absent while reading, true otherwise
	*
	* \note The smart pointer class must be dereferencable through operator*() and constructible from raw pointer to the class
	* \note The class must be default constructible
	* \note Using a vector of raw pointers may cause memory leaks if there is some content before loading
	*/
	template<typename T>
	typename std::enable_if<std::is_base_of<Serialisable, typename std::remove_reference<decltype(*std::declval<T>())>::type>::value
			&& std::is_constructible<T, typename std::remove_reference<decltype(*std::declval<T>())>::type*>::value, bool>::type
	synch(const std::string& key, std::vector<T>& value) {
		if (_saving) {
			auto making = std::make_shared<JSONarray>();
			for (unsigned int i = 0; i < value.size(); i++) {
				auto innerMaking = std::make_shared<JSONobject>();
				(*value[i])._saving = true;
				(*value[i])._json = innerMaking;
				(*value[i]).serialisation();
				(*value[i])._json.reset();
				making->getVector().push_back(innerMaking);
			}
			_json->getObject()[key] = making;
		} else {
			value.clear();
			auto found = _json->getObject().find(key);
			if (found != _json->getObject().end()) {
				for (unsigned int i = 0; i < found->second->getVector().size(); i++) {
					value.emplace_back(new typename std::remove_reference<decltype(*std::declval<T>())>::type());
					T& filled = value.back();
					(*filled)._saving = false;
					(*filled)._json = found->second->getVector()[i];
					(*filled).serialisation();
					(*filled)._json.reset();
				}
			} else return false;
		}
		return true;
	}

public:
	/*!
	* \brief Serialises the object to a JSON string
	* \return The JSON string
	*
	* \note It calls the overloaded process() method
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline std::string serialise() const {
		std::shared_ptr<JSON> target = std::make_shared<JSONobject>();
		_json = target;
		_saving = true;
		const_cast<Serialisable*>(this)->serialisation();
		std::stringstream out;
		_json->write(out);
		_json = nullptr;
		return out.str();
	}

	/*!
	* \brief Serialises the object to condensed JSON
	* \return A vector of uint8_t with the contents
	*
	* \note It calls the overloaded process() method
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline std::vector<uint8_t> serialiseCondensed() const {
		std::shared_ptr<JSON> target = std::make_shared<JSONobject>();
		_json = target;
		_saving = true;
		const_cast<Serialisable*>(this)->serialisation();
		std::vector<uint8_t> out = _json->condensed();
		_json = nullptr;
		return out;
	}
	
	/*!
	* \brief Saves the object to a JSON file
	* \param The name of the JSON file
	*
	* \note It calls the overloaded serialisation() method
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline void save(const std::string& fileName) const {
		_json = std::make_shared<JSONobject>();
		_saving = true;
		const_cast<Serialisable*>(this)->serialisation();
		_json->writeToFile(fileName);
		_json.reset();
	}

	/*!
	* \brief Loads the object from a JSON string
	* \param The JSON string
	*
	* \note It calls the overloaded process() method
	* \note If the string is blank, nothing is done
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline void deserialise(const std::string& source) {
		std::stringstream sourceStream(source);
		std::shared_ptr<JSON> target = parseJSON(sourceStream);
		_json = target;
		if (_json->type() == JSONtype::NIL) {
			_json = nullptr;
			return;
		}
		_saving = false;
		serialisation();
		_json = nullptr;
	}

	/*!
	* \brief Loads the object from condensed JSON
	* \return A vector of uint8_t with the contents
	*
	* \note It calls the overloaded process() method
	* \note If the string is blank, nothing is done
	* \note Not only that it's not thread-safe, it's not even reentrant
	*/
	inline void deserialise(const std::vector<uint8_t>& source) {
		std::shared_ptr<JSON> target = parseCondensed(source);
		_json = target;
		if (_json->type() == JSONtype::NIL) {
			_json = nullptr;
			return;
		}
		_saving = false;
		serialisation();
		_json = nullptr;
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
		_json = parseJSON(fileName);
		if (_json->type() == JSONtype::NIL) {
			_json.reset();
			return;
		}
		_saving = false;
		serialisation();
		_json.reset();
	}
};

#endif //SERIALISABLE_BY_DUGI_HPP
